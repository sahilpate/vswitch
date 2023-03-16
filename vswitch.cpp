#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <IPv4Layer.h>
#include <Packet.h>
#include <PcapFileDevice.h>
#include <PcapLiveDeviceList.h>
#include <SystemUtils.h>

/*
 * PacketQueue - A generic, thread-safe, FIFO queue implemented using a fixed size circular buffer.
 * It follows the "best effort" model; if the queue is full when attemping to push a new element,
 * that element is immediately dropped (rather than waiting for space to be made).
 */
template <typename T>
class PacketQueue {
public:
    bool push_packet(T obj) {
	prod_mtx.lock();

	if(space == 0) {
	    prod_mtx.unlock();
	    return false;
	}

	packet_queue[in] = obj;
	in = (in + 1) % queue_size;
	space--;
	prod_mtx.unlock();

	cons_mtx.lock();
	objects++;
	cons_mtx.unlock();
	cons_cond.notify_one();

	return true;
    }

    T pop_packet() {
	std::unique_lock<std::mutex> local_mtx(cons_mtx);
	while(objects == 0) {
	    cons_cond.wait(local_mtx);
	}

	T popped_val = packet_queue[out];
	out = (out + 1) % queue_size;
	objects--;
	local_mtx.~unique_lock<std::mutex>();

	prod_mtx.lock();
	space++;
	prod_mtx.unlock();

	return popped_val;
    }


    const static int queue_size = 10;
    T packet_queue[queue_size];

private:
    int in = 0, out = 0, space = queue_size, objects = 0;
    std::condition_variable cons_cond;
    std::mutex prod_mtx, cons_mtx;
};

/*
 * RawPcktCompare - Overloads the () operator to, when given two pcpp::RawPacket's, determine which
 * one has the smallest acutal value. It enables the pcpp::RawPacket type to be used with various
 * standard containers or library functions which need a comparison function, typically for sorting.
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
    PacketQueue<std::pair<pcpp::RawPacket, pcpp::PcapLiveDevice *>> packet_queue;
    DuplicateManager dup_mgr;
};

/*
 * process_packet() - Passed to pcpp::PcapLiveDevice.startCapture(), which is called for every
 * vswitch interface. startCapture() creates a new thread which listens for traffic on the
 * corresponding interface, and this function is called whenever a new packet arrives. If the packet
 * is not a duplicate (see DuplicateManager), then it is queued to be sent.
 */
static void process_packet(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie) {
    VswitchShmem *data = static_cast<VswitchShmem *>(cookie);
    pcpp::Packet parsedPacket(packet);

    for(long unsigned int i = 0; i < data->veth_intfs.size(); i++) {
	if(dev == data->veth_intfs[i]) {
	    if(data->dup_mgr.check_duplicate(i, *packet)) {
		return;
	    }

	    data->ingress_count[i]++;
	    data->packet_queue.push_packet(std::pair(*packet, dev));
	    break;
	}
    }

    return;
}

/*
 * send_packets() - A single thread is made with this function, which waits on the packet queue in
 * in the VswitchShmem instance, and transmits the packet whenever it is available.
 */
void send_packets(VswitchShmem *data) {
    while(true) {
	auto[pckt, src_intf_ptr] = data->packet_queue.pop_packet();

	for(long unsigned int i = 0; i < data->veth_intfs.size(); i++) {
	    auto intf_ptr = data->veth_intfs[i];

	    if(intf_ptr == src_intf_ptr) {
		continue;
	    }

	    data->dup_mgr.mark_duplicate(i, pckt);
	    intf_ptr->sendPacket(pckt);
	}
    }
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
	intf->startCapture(process_packet, &data);
	std::cout << intf->getName() << std::endl;
    }
    std::cout << std::endl;
    std::thread egress(send_packets, &data);

    pcpp::multiPlatformSleep(10);
    for(auto intf : veth_intfs) {
	intf->stopCapture();
    }

    for(auto intf : veth_intfs) {
	std::cout << "Closing " << intf->getName() << std::endl;
	intf->close();
    }

    return 0;
}
