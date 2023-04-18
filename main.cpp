/*
 * main.cpp - The project's entry point.
 *
 * It opens the appropriate interfaces for capturing, creates all threads necessary for the switch
 * to function, and closes the interfaces when it receives an exit command from the CLI.
 */

#include <PcapLiveDeviceList.h>
#include <SystemUtils.h>
#include <FlexLexer.h>
#include "cli.hpp"
#include "vswitch_shmem.hpp"
#include "vswitch_utils.hpp"

// Logo produced using: https://patorjk.com/software/taag/#p=display&f=Big%20Money-ne&t=vswitch
const std::string vswitch_header =
    "                                   /$$   /$$               /$$      \n"
    "                                  |__/  | $$              | $$      \n"
    " /$$    /$$ /$$$$$$$ /$$  /$$  /$$ /$$ /$$$$$$    /$$$$$$$| $$$$$$$ \n"
    "|  $$  /$$//$$_____/| $$ | $$ | $$| $$|_  $$_/   /$$_____/| $$__  $$\n"
    " \\  $$/$$/|  $$$$$$ | $$ | $$ | $$| $$  | $$    | $$      | $$  \\ $$\n"
    "  \\  $$$/  \\____  $$| $$ | $$ | $$| $$  | $$ /$$| $$      | $$  | $$\n"
    "   \\  $/   /$$$$$$$/|  $$$$$/$$$$/| $$  |  $$$$/|  $$$$$$$| $$  | $$\n"
    "    \\_/   |_______/  \\_____/\\___/ |__/   \\___/   \\_______/|__/  |__/\n";

/*
 * receive_packet() - Passed to pcpp::PcapLiveDevice.startCapture(), which is called for every
 * vswitch interface. startCapture() creates a new thread which listens for traffic on the
 * corresponding interface, and this function is called whenever a new packet arrives. If the packet
 * is not a duplicate (see DuplicateManager), then it is queued to be sent.
 */
static void receive_packet(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie) {
    VswitchShmem *data = static_cast<VswitchShmem *>(cookie);

    for(long unsigned int i = 0; i < data->veth_intfs.size(); i++) {
	if(dev == data->veth_intfs[i]) {
	    if(data->dup_mgr.check_duplicate(i, *packet)) {
		return;
	    }

	    data->counters.increment_counters(i, packet->getRawDataLen(), Counters::ING);
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
	data->packet_queue.process_packet(&(data->mac_tbl), &(data->vlans), &(data->veth_intfs));
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
	    data->counters.increment_counters(j, entry.pckt.getRawDataLen(), Counters::EGR);
	}
    }
}

/*
 * age_mac_addrs() - A single thread is made with this function, which removes old MAC to interface
 * mappings at a regular interval, that interval being the default maximum age all entries have.
 */
void age_mac_addrs(VswitchShmem *data) {
    while(true) {
	pcpp::multiPlatformSleep(data->mac_tbl.get_max_age());
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

    std::cout << std::endl << vswitch_header << std::endl;
    while(true) {
	std::cout << "vswitch# " << std::flush;
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
    std::vector<pcpp::PcapLiveDevice *> veth_intfs = get_intfs_prefixed_by("vswitch");
    VswitchShmem data(veth_intfs);

    for(auto intf : veth_intfs) {
	intf->startCapture(receive_packet, &data);
    }

    std::thread process(process_packets, &data);
    std::thread egress(send_packets, &data);
    std::thread mac_tbl_ager(age_mac_addrs, &data);
    std::thread cmd_line(cli, &data);

    cmd_line.join();

    for(auto intf : veth_intfs) {
	intf->stopCapture();
	intf->close();
    }

    return 0;
}
