/*
 * tests.cpp - Entry point for the tester program to run on the testing container.
 *
 * This contains various test setup functions, which fill in packets to send on available interfaces
 * and packets which are expected to be received on others.
 */

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <PcapLiveDevice.h>
#include <SystemUtils.h>
#include "testing_utils.hpp"
#include "vswitch_utils.hpp"

/*
 * broadcast_test_setup() - Crafts broadcast packet to send through each interface. It expects to
 * receive the same broadcast packet on every interface besides the one it was sent from.
 *
 * Configuration: default
 */
void broadcast_test_setup(TestData &data) {
    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
        pcpp::RawPacket pckt = create_broadcast_pckt(data.veth_intfs[i]);

	data.pckts_to_transmit.push_back({pckt, data.veth_intfs[i]});
	data.dup_mgr.mark_duplicate(i, pckt);
	for(long unsigned int j = 0; j < data.veth_intfs.size(); j++) {
	    if(i == j) {
		continue;
	    }
	    data.expected.mark_duplicate(j, pckt);
	}
    }

    auto rng = std::default_random_engine {};
    std::shuffle(std::begin(data.pckts_to_transmit), std::end(data.pckts_to_transmit), rng);
    return;
}

int main(int argc, char *argv[]) {
    std::map<std::string, std::function<void(TestData &)>> tests = {
	{"broadcast_test", broadcast_test_setup}
    };

    // Validate command line argument. Ensure the given strings corresponds to a valid test.
    if(argc != 2) {
	std::cerr << "Expected exactly 1 argument." << std::endl;
	return TestData::FAIL;
    }

    std::map<std::string, std::function<void(TestData &)>>::iterator test_it;
    test_it = tests.find(std::string(argv[1]));
    if(test_it == tests.end()) {
	std::cerr << "\"" << argv[1] << "\" is not a valid test." << std::endl;
	return TestData::FAIL;
    }

    std::vector<pcpp::PcapLiveDevice *> veth_intfs = get_intfs_prefixed_by("test");
    TestData data(veth_intfs);

    // Call the setup function for the test requested by the caller.
    test_it->second(data);

    // Run the test.
    for(auto intf : veth_intfs) {
	intf->startCapture(verify_packet, &data);
    }

    for(auto [pckt, intf_ptr] : data.pckts_to_transmit) {
	intf_ptr->sendPacket(pckt);
    }
    pcpp::multiPlatformSleep(1);

    for(auto intf : veth_intfs) {
	intf->stopCapture();
	intf->close();
    }

    return evaluate_test_results(data);
}
