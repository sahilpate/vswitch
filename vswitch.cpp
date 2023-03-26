// Standard Libraries
#include <condition_variable>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>

// PcapPlusPlus
#include <EthLayer.h>
#include <IPv4Layer.h>
#include <Packet.h>
#include <PcapFileDevice.h>
#include <PcapLiveDeviceList.h>
#include <SystemUtils.h>

// Flex
#include <FlexLexer.h>

/*
 * RawPcktCompare - Overloads the () operator to, when given two pcpp::RawPacket's, provide a unique
 * comparison between their actual values. It enables the pcpp::RawPacket type to be used with
 * various standard containers or library functions which need a comparison function, typically for
 * sorting.
 */
struct RawPcktCompare {
    bool operator() (pcpp::RawPacket a, pcpp::RawPacket b) const {
	for(int i = 0; i < a.getRawDataLen() && i < b.getRawDataLen(); i++) {
	    if((a.getRawData())[i] == (b.getRawData())[i]) {
		continue;
	    } else {
		return (a.getRawData())[i] < (b.getRawData())[i];
	    }
	}

	return a.getRawDataLen() < b.getRawDataLen();
    }
};

/*
 * MacAddrCompare - Overloads the () operator to, when given two pcpp::MacAddress's, provide a
 * unique comparison between their actual values. It enables the pcpp::MacAddress type to be used
 * with various standard containers or library functions which need a comparison function, typically
 * for sorting.
 */
struct MacAddrCompare {
    bool operator() (pcpp::MacAddress a, pcpp::MacAddress b) const {
	uint64_t a_full = 0, b_full = 0;

	a.copyTo(reinterpret_cast<uint8_t *>(&a_full));
	b.copyTo(reinterpret_cast<uint8_t *>(&b_full));

	return a_full < b_full;
    }
};

/*
 * MacAddrTable - An abstraction for the table used to make forwarding decisions. This enables self
 * learning, where mappings from a given MAC address to an interface are added as frames arrive,
 * read from when deciding where to forward frames, and aged out over time.
 */
class MacAddrTable {
public:
    void push_mapping(pcpp::MacAddress mac_addr, pcpp::PcapLiveDevice *intf) {
	table_access.lock();
	table[mac_addr] = {intf, std::time(nullptr)};
	table_access.unlock();
    }

    pcpp::PcapLiveDevice *get_mapping(pcpp::MacAddress mac_addr) {
	table_access.lock();
	auto table_it = table.find(mac_addr);
	if(table_it == table.end()) {
	    table_access.unlock();
	    return nullptr;
	}

	pcpp::PcapLiveDevice *ret_intf = table_it->second.first;
	table_access.unlock();
	return ret_intf;
    }

    int age_mappings() {
	int num_aged_out = 0;
	auto table_it = table.begin();
	std::time_t elem_time, cur_time = std::time(nullptr);
	double diff;

	table_access.lock();
	while(table_it != table.end()) {
	    elem_time = table_it->second.second;
	    diff = difftime(cur_time, elem_time);

	    if(diff > max_age) {
		num_aged_out++;
		table.erase(table_it++);
	    } else {
	        table_it++;
	    }
	}
	table_access.unlock();

	return num_aged_out;
    }

    void print_mactbl(std::ostream &out) {
	this->age_mappings();
	std::vector<std::string> headers = {"Mac Addresses", "Ports", "Time to Live"};

	out << std::setw(30) << "" << "Mac Address Table" << std::endl;
	out << std::string(80, '-') << std::endl << std::endl;

	for(auto str : headers) {
	    out << std::setw(20) << std::left << str;
	}
	out << std::endl;
	for(auto str : headers) {
	    out << std::setw(20) << std::left << std::string(str.size(), '-');
	}
	out << std::endl;

	table_access.lock();
	for(auto [mac_addr, info] : table) {
	    auto [intf, timestamp] = info;
	    double ttl = max_age - difftime(std::time(nullptr), timestamp);

	    std::cout << std::setw(20) << std::left << mac_addr.toString();
	    std::cout << std::setw(20) << std::left << intf->getName();
	    std::cout << std::setw(20) << std::left << ttl << std::endl;
	}
	table_access.unlock();
	std::cout << std::endl;
	return;
    }

    static const int max_age = 5; // in seconds

private:
    std::map<pcpp::MacAddress,
	     std::pair<pcpp::PcapLiveDevice *, std::time_t>,
	     MacAddrCompare> table;
    std::mutex table_access;
};

/*
 * PQueueEntry - Represents a queue entry in the PacketQueue class. The raw packet itself and the
 * interface it came in on are recorded when initially pushed onto the queue. Other information is
 * filled in during processing, and that info is used when popping and egressing the packet.
 */
class PQueueEntry {
public:
    PQueueEntry() {}

    PQueueEntry(pcpp::RawPacket pckt, pcpp::PcapLiveDevice *src_intf)
	: pckt(pckt),
	  src_intf(src_intf)
	{}

    pcpp::RawPacket pckt;
    pcpp::PcapLiveDevice *src_intf;
    std::vector<pcpp::PcapLiveDevice *> dst_intfs;
};

/*
 * PacketQueue - A thread-safe, FIFO queue implemented using a fixed size circular buffer.
 * It follows the "best effort" model; if the queue is full when attemping to push a new element,
 * that element is immediately dropped (rather than waiting for space to be made).
 */
class PacketQueue {
public:
    bool push_packet(pcpp::RawPacket pckt, pcpp::PcapLiveDevice *src_intf) {
	PQueueEntry obj(pckt, src_intf);

	prod_mtx.lock();

	if(space == 0) {
	    prod_mtx.unlock();
	    return false;
	}

	packet_queue[in] = obj;
	in = (in + 1) % queue_size;
	space--;
	prod_mtx.unlock();

	proc_mtx.lock();
	to_proc++;
	proc_mtx.unlock();
	proc_cond.notify_one();

	return true;
    }

    void process_packet(MacAddrTable *mac_tbl,
			 std::vector<pcpp::PcapLiveDevice *> *veth_intfs) {
	std::unique_lock<std::mutex> local_mtx(proc_mtx);
	while(to_proc == 0) {
	    proc_cond.wait(local_mtx);
	}

	// Update MAC address table based on incoming packet
	PQueueEntry &entry = packet_queue[proc];
	pcpp::Packet parsed_pckt(&entry.pckt);
	pcpp::EthLayer *eth_layer = parsed_pckt.getLayerOfType<pcpp::EthLayer>();
	mac_tbl->push_mapping(eth_layer->getSourceMac(), entry.src_intf);

	// Make forwarding decision based on MAC table
	std::vector<pcpp::PcapLiveDevice *> out_intfs;
	pcpp::PcapLiveDevice *mapping = mac_tbl->get_mapping(eth_layer->getDestMac());

	if(mapping == nullptr) {
	    // Broadcast if no mapping exists
	    for(auto it : *veth_intfs) {
		if(it != entry.src_intf) {
		    out_intfs.push_back(it);
		}
	    }
	} else if(mapping != entry.src_intf) {
	    // Otherwise, if the packet is destined for a different intf from the src, forward to it
	    out_intfs.push_back(mapping);
	}

	entry.dst_intfs = out_intfs;

	// Increment buffer pointers
	proc = (proc + 1) % queue_size;
	to_proc--;
	local_mtx.~unique_lock<std::mutex>();

	cons_mtx.lock();
	objects++;
	cons_mtx.unlock();
	cons_cond.notify_one();

	return;
    }

    PQueueEntry pop_packet() {
	std::unique_lock<std::mutex> local_mtx(cons_mtx);
	while(objects == 0) {
	    cons_cond.wait(local_mtx);
	}

	PQueueEntry popped_val = packet_queue[out];
	out = (out + 1) % queue_size;
	objects--;
	local_mtx.~unique_lock<std::mutex>();

	prod_mtx.lock();
	space++;
	prod_mtx.unlock();

	return popped_val;
    }

    const static int queue_size = 10;
    PQueueEntry packet_queue[queue_size];

private:
    int in = 0, space = queue_size; // producer
    int proc = 0, to_proc = 0;      // packet processor
    int out = 0, objects = 0;       // consumer

    std::condition_variable proc_cond, cons_cond;
    std::mutex prod_mtx, proc_mtx, cons_mtx;
};

/*
 * DuplicateManager - In PcapPlusPlus when egressing a packet out of an interface which has also
 * been opened for capturing, that packet will be detected as ingressing on that same interface.
 * This data structure keeps a record of packets and the interface they have egressed on so that
 * they can be ignored when they're re-detected.
 */
class DuplicateManager {
public:
    DuplicateManager(int total_intfs)
	: seen(total_intfs),
	  intf_locks(total_intfs)
	{}

    void mark_duplicate(int intf_indx, pcpp::RawPacket pckt) {
	auto &cur_map = seen[intf_indx];

	intf_locks[intf_indx].lock();
	if(cur_map.count(pckt) > 0) {
	    cur_map[pckt]++;
	} else {
	    cur_map[pckt] = 1;
	}
	intf_locks[intf_indx].unlock();
    }

    bool check_duplicate(int intf_indx, pcpp::RawPacket pckt) {
	auto &cur_map = seen[intf_indx];
	bool is_dup = false;

	intf_locks[intf_indx].lock();
	if(cur_map.count(pckt) > 0) {
	    is_dup = true;
	    if(cur_map[pckt] <= 1) {
		cur_map.erase(pckt);
	    } else {
		cur_map[pckt]--;
	    }
	}
	intf_locks[intf_indx].unlock();

	return is_dup;
    }

private:
    std::vector<std::map<pcpp::RawPacket, int, RawPcktCompare>> seen;
    std::vector<std::mutex> intf_locks;
};

/*
 * VswitchShemem - Stores various information about the current state of the virtual switch. Only
 * one instance of this class should be created, and will be read and written to by most threads.
 */
class VswitchShmem {
public:
    VswitchShmem(std::vector<pcpp::PcapLiveDevice *> veth_intfs)
	: veth_intfs(veth_intfs),
	  ingress_count(veth_intfs.size(), 0),
	  dup_mgr(veth_intfs.size())
	{}

    std::vector<pcpp::PcapLiveDevice *> veth_intfs;
    std::vector<int> ingress_count;
    PacketQueue packet_queue;
    DuplicateManager dup_mgr;
    MacAddrTable mac_tbl;
};

/*
 * CliInterpreter - This class maintains a tree of valid serieses of CLI tokens and the functions
 * that should be called when they are inputted. When given a series of tokens and their literal
 * values through the interpret() function, the appropriate function will be called if the series
 * is valid. Otherwise, it will return an error code.
 */
class CliInterpreter {
public:
    enum token {
	ROOT, NL, EXIT, SHOW, MAC, ADDR_TBL, INTF, COUNT, NAME, UINT
    };

    CliInterpreter(VswitchShmem *shmem) : root(ROOT), shmem(shmem) {
	for(auto cmd_func : commands) {
	    auto [cmd, func] = cmd_func;
	    this->add_cmd(root, cmd.begin(), cmd.end(), func);
	}
	return;
    }

    int interpret(std::vector<token> tokens, std::vector<std::string> args) {
	long unsigned int i, j;
	auto cur_node = &root;
	for(i = 0; i < tokens.size(); i++) {
	    bool found = false;
	    for(j = 0; j < cur_node->children.size(); j++) {
		if(tokens[i] == cur_node->children[j].tkn) {
		    found = true;
		    cur_node = &cur_node->children[j];
		    break;
		}
	    }

	    if(found == false) {
		return -1;
	    }
	}

	if(cur_node->func == nullptr) {
	    return -1;
	}

	cur_node->func(args);
	return 0;
    }

private:
    class InterpreterTreeNode {
    public:
	InterpreterTreeNode(token tkn) : tkn(tkn) {}
	InterpreterTreeNode(
	    token tkn, std::function<void(std::vector<std::string>)> func)
	    : tkn(tkn), func(func) {}

	token tkn;
	std::function<void(std::vector<std::string>)> func;
	std::vector<InterpreterTreeNode> children;
    };

    InterpreterTreeNode root;
    VswitchShmem *shmem;

    std::function<void(std::vector<std::string>)> show_mac_addrtbl = [&](std::vector<std::string>) {
	shmem->mac_tbl.print_mactbl(std::cout);
    };

    const std::vector<std::pair<std::vector<token>,std::function<void(std::vector<std::string>)>>>
    commands = {
	{{SHOW, MAC, ADDR_TBL}, show_mac_addrtbl}
    };

    void add_cmd(InterpreterTreeNode &node,
		 std::vector<token>::iterator cur,
		 std::vector<token>::iterator end,
		 std::function<void(std::vector<std::string>)> func) {
	if(cur == end) {
	    node.func = func;
	    return;
	}

	long unsigned int nxt_tkn_indx;
	for(nxt_tkn_indx = 0; nxt_tkn_indx < node.children.size(); nxt_tkn_indx++) {
	    if(node.children[nxt_tkn_indx].tkn == *cur) {
		break;
	    }
	}

	if(nxt_tkn_indx == node.children.size()) {
	    node.children.push_back(InterpreterTreeNode(*cur));
	}

	add_cmd(node.children[nxt_tkn_indx], ++cur, end, func);
	return;
    }
};

/*
 * receive_packet() - Passed to pcpp::PcapLiveDevice.startCapture(), which is called for every
 * vswitch interface. startCapture() creates a new thread which listens for traffic on the
 * corresponding interface, and this function is called whenever a new packet arrives. If the packet
 * is not a duplicate (see DuplicateManager), then it is queued to be sent.
 */
static void receive_packet(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie) {
    VswitchShmem *data = static_cast<VswitchShmem *>(cookie);
    pcpp::Packet parsedPacket(packet);

    for(long unsigned int i = 0; i < data->veth_intfs.size(); i++) {
	if(dev == data->veth_intfs[i]) {
	    if(data->dup_mgr.check_duplicate(i, *packet)) {
		return;
	    }

	    data->ingress_count[i]++;
	    data->packet_queue.push_packet(*packet, dev);
	    break;
	}
    }

    return;
}

/*
 * process_packets() - A single thread is made with this function, which waits for packets to
 * process on the packet queue. During processing, it fills in various metadata stored in the
 * PQueueEntry class. Most notably, it makes the forwarding decision for each queued packet.
 */
void process_packets(VswitchShmem *data) {
    while(true) {
	data->packet_queue.process_packet(&(data->mac_tbl), &(data->veth_intfs));
    }
}

/*
 * send_packets() - A single thread is made with this function, which waits on the packet queue in
 * in the VswitchShmem instance, and transmits the packet whenever it is available.
 */
void send_packets(VswitchShmem *data) {
    long unsigned int i, j;
    while(true) {
	PQueueEntry entry = data->packet_queue.pop_packet();

	for(i = 0; i < entry.dst_intfs.size(); i++) {
	    auto intf_ptr = entry.dst_intfs[i];

	    // TODO: Update the veth_intf vector to be a set instead, to avoid this redundant loop.
	    // This requires changes across the entire program, mainly in DuplicateManager.
	    for(j = 0; j < data->veth_intfs.size(); j++) {
		if(data->veth_intfs[j] == entry.dst_intfs[i]) {
		    break;
		}
	    }

	    data->dup_mgr.mark_duplicate(j, entry.pckt);
	    intf_ptr->sendPacket(entry.pckt);
	}
    }
}

/*
 * age_mac_addrs() - A single thread is made with this function, which removes old MAC to interface
 * mappings at a regular interval, that interval being the default maximum age all entries have.
 */
void age_mac_addrs(VswitchShmem *data) {
    while(true) {
	pcpp::multiPlatformSleep(data->mac_tbl.max_age);
	data->mac_tbl.age_mappings();
    }
}

/*
 * cli() - A single thread is made with this function, which handles the vswitch command line
 * interface. The function parses each line of user input through the help of a Flex file and passes
 * the output to CliInterpreter::interpret() to interpret the text.
 */
void cli(VswitchShmem *data) {
    CliInterpreter interpreter(data);
    FlexLexer *lexer = new yyFlexLexer;
    CliInterpreter::token token;

    while(true) {
	std::cout << "vswitch# ";
	std::vector<CliInterpreter::token> tokens;
	std::vector<std::string> args;
	while((token = static_cast<CliInterpreter::token>(lexer->yylex())) != interpreter.NL) {
	    tokens.push_back(token);
	    if(token == interpreter.NAME || token == interpreter.UINT) {
		args.push_back(std::string(lexer->YYText(), lexer->YYLeng()));
	    }
	}

	if(tokens.size() == 0) {
	    continue;
	} else if(tokens.size() == 1 && tokens[0] == interpreter.EXIT) {
	    break;
	} else if(interpreter.interpret(tokens, args ) == -1) {
	    std::cout << "Bad command" << std::endl;
	}
    }

    delete lexer;
    return;
}

/*
 * main() - Initializes the capturing threads for the appropriate interfaces (those whose names are
 * prefixed by "vswitch") and the single sending thread.
 */
int main(void) {
    std::vector<pcpp::PcapLiveDevice *> veth_intfs, all_intfs =
	pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();

    for(auto intf : all_intfs) {
	std::string intf_name = intf->getName();
	std::string prefix = "vswitch";
	if(intf_name.compare(0, prefix.size(), prefix) == 0) {
	    if(!intf->open()) {
		std::cerr << "Could not open intf " << intf_name << std::endl;
	    }
	    veth_intfs.push_back(intf);
	}
    }

    VswitchShmem data(veth_intfs);

    std::cout << "Starting capture on:" << std::endl;
    for(auto intf : veth_intfs) {
	intf->startCapture(receive_packet, &data);
	std::cout << intf->getName() << std::endl;
    }
    std::cout << std::endl;

    std::thread process(process_packets, &data);
    std::thread egress(send_packets, &data);
    std::thread mac_tbl_ager(age_mac_addrs, &data);
    std::thread cmd_line(cli, &data);

    cmd_line.join();

    for(auto intf : veth_intfs) {
	intf->stopCapture();
    }

    for(auto intf : veth_intfs) {
	std::cout << "Closing " << intf->getName() << std::endl;
	intf->close();
    }

    return 0;
}
