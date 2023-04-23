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
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave = data.test_waves[0];

    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
        pcpp::RawPacket pckt = create_broadcast_pckt(data.veth_intfs[i]);

	wave.pckts_to_transmit.push_back({pckt, data.veth_intfs[i]});
	data.dup_mgr.mark_duplicate(i, pckt);
	for(long unsigned int j = 0; j < data.veth_intfs.size(); j++) {
	    if(i == j) {
		continue;
	    }
	    wave.expected.mark_duplicate(j, pckt);
	}
    }

    std::default_random_engine rng(time(0));
    std::shuffle(std::begin(wave.pckts_to_transmit), std::end(wave.pckts_to_transmit), rng);
    return;
}

/*
 * learning_test_setup() - A single packet is broadcasted out a single, random interface. All other
 * interfaces then reply with a single message destined for the original interface.
 *
 * Configuration: default
 */
void learning_test_setup(TestData &data) {
    // Wave 1 - Broadcast out one random intf
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave1 = data.test_waves[0];
    unsigned orig_intf = rand() % data.veth_intfs.size();

    pcpp::RawPacket orig_pckt = create_broadcast_pckt(data.veth_intfs[orig_intf]);
    wave1.pckts_to_transmit.push_back({orig_pckt, data.veth_intfs[orig_intf]});
    data.dup_mgr.mark_duplicate(orig_intf, orig_pckt);
    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	if(i == orig_intf) {
	    continue;
	}
	wave1.expected.mark_duplicate(i, orig_pckt);
    }

    // Wave 2 - All other interfaces send a frame to the original interface
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave2 = data.test_waves[1];

    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	if(i == orig_intf) {
	    continue;
	}

	pcpp::RawPacket direct_pckt = create_pckt(data.veth_intfs[i], data.veth_intfs[orig_intf]);
	wave2.pckts_to_transmit.push_back({direct_pckt, data.veth_intfs[i]});
	data.dup_mgr.mark_duplicate(i, direct_pckt);
	wave2.expected.mark_duplicate(orig_intf, direct_pckt);
    }

    return;
}

/*
 * aging_test_setup() - A single packet is broadcasted out a single, random interface. All other
 * interfaces then reply with a single message destined for the original interface *after a delay*.
 *
 * Configuration: mac address-table aging-time 1
 */
void aging_test_setup(TestData &data) {
    // Wave 1 - Broadcast out one random intf
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave1 = data.test_waves[0];
    unsigned orig_intf = rand() % data.veth_intfs.size();

    wave1.delay = 5;
    pcpp::RawPacket orig_pckt = create_broadcast_pckt(data.veth_intfs[orig_intf]);
    wave1.pckts_to_transmit.push_back({orig_pckt, data.veth_intfs[orig_intf]});
    data.dup_mgr.mark_duplicate(orig_intf, orig_pckt);
    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	if(i == orig_intf) {
	    continue;
	}
	wave1.expected.mark_duplicate(i, orig_pckt);
    }

    // Wave 2 - All other interfaces send a frame to the original interface
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave2 = data.test_waves[1];

    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	if(i == orig_intf) {
	    continue;
	}

	pcpp::RawPacket direct_pckt = create_pckt(data.veth_intfs[i], data.veth_intfs[orig_intf]);
	wave2.pckts_to_transmit.push_back({direct_pckt, data.veth_intfs[i]});
	data.dup_mgr.mark_duplicate(i, direct_pckt);

	for(long unsigned int j = 0; j < data.veth_intfs.size(); j++) {
	    if(j == i) {
		continue;
	    }
	    wave2.expected.mark_duplicate(j, direct_pckt);
	}
    }

    return;
}

/*
 * mult_mac_test_setup() - Broadcast a packet out the first interface. Then, for each interface i in
 * range 2-N, send a packet from i directed to all interfaces < i. Because of MAC learning, none of
 * the packets sent after wave 1 should be broadcasted.
 *
 * Configuration: mac address-table aging-time 128
 */
void mult_mac_test_setup(TestData &data) {
    // Wave 1 - Initial broadcast
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave1 = data.test_waves[0];
    pcpp::RawPacket first_pckt = create_broadcast_pckt(data.veth_intfs[0]);
    wave1.pckts_to_transmit.push_back({first_pckt, data.veth_intfs[0]});
    data.dup_mgr.mark_duplicate(0, first_pckt);

    for(long unsigned int i = 1; i < data.veth_intfs.size(); i++) {
	wave1.expected.mark_duplicate(i, first_pckt);
    }

    // Waves 2-N - Selective transmission
    for(long unsigned int i = 1; i < data.veth_intfs.size(); i++) {
	data.test_waves.push_back(TestWave(data.veth_intfs.size()));
	TestWave &cur_wave = data.test_waves[i];

	for(long unsigned int j = 0; j < i; j++) {
	    pcpp::RawPacket cur_pckt = create_pckt(data.veth_intfs[i], data.veth_intfs[j]);

	    cur_wave.pckts_to_transmit.push_back({cur_pckt, data.veth_intfs[i]});
	    data.dup_mgr.mark_duplicate(i, cur_pckt);
	    cur_wave.expected.mark_duplicate(j, cur_pckt);
	}
    }

    return;
}

/*
 * vlan_broadcast_test_setup() - Broadcast a packet out every interface, but all odd-indexed
 * interfaces are placed on a separate VLAN. Only expect the broadcasts to reach the interfaces on
 * the same VLAN.
 *
 * Configuration: *Expect exactly 6 interfaces*
 *     vlan 2
 *     vswitch-test2 vlan 2
 *     vswitch-test4 vlan 2
 *     vswitch-test6 vlan 2
 */
void vlan_broadcast_test_setup(TestData &data) {
    if(data.veth_intfs.size() != 6) {
	std::cerr << __func__
		  << ": Expected 6 interfaces, but has "
		  << data.veth_intfs.size()
		  << ". Skipping test..."
		  << std::endl;
	return;
    }

    // Wave 1 - Broadcast out all intfs. Even/odd intfs are on different VLANs.
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave = data.test_waves[0];
    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	pcpp::RawPacket pckt = create_broadcast_pckt(data.veth_intfs[i]);
	wave.pckts_to_transmit.push_back({pckt, data.veth_intfs[i]});
	data.dup_mgr.mark_duplicate(i, pckt);

	for(long unsigned int j = 0; j < data.veth_intfs.size(); j++) {
	    if(i != j && ((i % 2) == (j % 2))) {
		wave.expected.mark_duplicate(j, pckt);
	    }
	}
    }

    return;
}

/*
 * vlan_mac_tbl_test_setup() - After the first three interfaces are place on a VLAN, one in the VLAN
 * is randomly selected to send a broadcast frame, and the a second unique interface on the same
 * VLAN sends a reply to the first interface.
 *
 * Configuration: *Expect exactly 6 interfaces*
 *     vlan 2
 *     vswitch-test1 vlan 2
 *     vswitch-test2 vlan 2
 *     vswitch-test3 vlan 2
 */
void vlan_mac_tbl_test_setup(TestData &data) {
    if(data.veth_intfs.size() != 6) {
	std::cerr << __func__
		  << ": Expected 6 interfaces, but has "
		  << data.veth_intfs.size()
		  << ". Skipping test..."
		  << std::endl;
	return;
    }

    // Wave 1 - Broadcast a frame out a random interface within a VLAN
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave1 = data.test_waves[0];
    unsigned orig_intf = rand() % 3;
    pcpp::RawPacket orig_pckt = create_broadcast_pckt(data.veth_intfs[orig_intf]);

    wave1.pckts_to_transmit.push_back({orig_pckt, data.veth_intfs[orig_intf]});
    data.dup_mgr.mark_duplicate(orig_intf, orig_pckt);
    for(long unsigned int i = 0; i < 3; i++) {
	if(i == orig_intf) {
	    continue;
	}

	wave1.expected.mark_duplicate(i, orig_pckt);
    }

    // Wave 2 - Send a packet directly to the broadcasting intf from a different intf within the
    // same VLAN
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave2 = data.test_waves[1];
    unsigned snd_intf = orig_intf;
    while(snd_intf == orig_intf) {
	snd_intf = rand() % 3;
    }

    pcpp::RawPacket snd_pckt = create_pckt(data.veth_intfs[snd_intf], data.veth_intfs[orig_intf]);
    wave2.pckts_to_transmit.push_back({snd_pckt, data.veth_intfs[snd_intf]});
    data.dup_mgr.mark_duplicate(snd_intf, snd_pckt);
    wave2.expected.mark_duplicate(orig_intf, snd_pckt);

    return;
}

/*
 * vlan_intf_outside_mac_tbl_test_setup() - After the first three interfaces are place on a VLAN,
 * one in the VLAN is randomly selected to send a broadcast frame, and the a second unique
 * interface on a different VLAN sends a reply to the first interface. This frame should be dropped.
 *
 * Configuration: *Expect exactly 6 interfaces*
 *     vlan 2
 *     vswitch-test1 vlan 2
 *     vswitch-test2 vlan 2
 *     vswitch-test3 vlan 2
 */
void vlan_intf_outside_mac_tbl_test_setup(TestData &data) {
    if(data.veth_intfs.size() != 6) {
	std::cerr << __func__
		  << ": Expected 6 interfaces, but has "
		  << data.veth_intfs.size()
		  << ". Skipping test..."
		  << std::endl;
	return;
    }

    // Wave 1 - Broadcast a frame out a random interface within a VLAN
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave1 = data.test_waves[0];
    unsigned orig_intf = rand() % 3;
    pcpp::RawPacket orig_pckt = create_broadcast_pckt(data.veth_intfs[orig_intf]);

    wave1.pckts_to_transmit.push_back({orig_pckt, data.veth_intfs[orig_intf]});
    data.dup_mgr.mark_duplicate(orig_intf, orig_pckt);
    for(long unsigned int i = 0; i < 3; i++) {
	if(i == orig_intf) {
	    continue;
	}

	wave1.expected.mark_duplicate(i, orig_pckt);
    }

    // Wave 2 - An intf outside the VLAN attempts and fails to use the MAC table entry
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave2 = data.test_waves[1];
    unsigned snd_intf = (rand() % 3) + 3;

    pcpp::RawPacket snd_pckt = create_pckt(data.veth_intfs[snd_intf], data.veth_intfs[orig_intf]);
    wave2.pckts_to_transmit.push_back({snd_pckt, data.veth_intfs[snd_intf]});
    data.dup_mgr.mark_duplicate(snd_intf, snd_pckt);

    return;
}

/*
 * multiple_vlans_test_setup() - Test that more than two VLANs can operate at the same time.
 *
 * Configuration: *Expect exactly 6 interfaces*
 *     vlan 100
 *     vlan 200
 *     vlan 300
 *     vswitch-test1 vlan 100
 *     vswitch-test2 vlan 100
 *     vswitch-test3 vlan 200
 *     vswitch-test4 vlan 200
 *     vswitch-test5 vlan 300
 *     vswitch-test6 vlan 300
 */
void multiple_vlans_test_setup(TestData &data) {
    if(data.veth_intfs.size() != 6) {
	std::cerr << __func__
		  << ": Expected 6 interfaces, but has "
		  << data.veth_intfs.size()
		  << ". Skipping test..."
		  << std::endl;
	return;
    }

    // Wave - Broadcast out all interfaces, expect reply from intf on same VLAN
    data.test_waves.push_back(TestWave(data.veth_intfs.size()));
    TestWave &wave = data.test_waves[0];

    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	pcpp::RawPacket pckt = create_broadcast_pckt(data.veth_intfs[i]);
	wave.pckts_to_transmit.push_back({pckt, data.veth_intfs[i]});
	data.dup_mgr.mark_duplicate(i, pckt);

	unsigned exp_indx = (i % 2) ? (i - 1) : (i + 1);
	wave.expected.mark_duplicate(exp_indx, pckt);
    }

    return;
}

/*
 * vlan_removal_test_setup() - Tests that when a VLAN is removed, the interfaces which were on it
 * are placed back into the default VLAN, and that they can now communicate with all other
 * interfaces on the default VLAN. This reuses the mult_mac_test. The "uniqueness" of this test
 * comes from its pre-configuration.
 *
 * Configuration: *Expect exactly 6 interfaces*
 *     vlan 123
 *     vswitch-test1 vlan 123
 *     vswitch-test5 vlan 123
 *     vswitch-test4 vlan 123
 *     no vlan 123
 */
void vlan_removal_test_setup(TestData &data) {
    if(data.veth_intfs.size() != 6) {
	std::cerr << __func__
		  << ": Expected 6 interfaces, but has "
		  << data.veth_intfs.size()
		  << ". Skipping test..."
		  << std::endl;
	return;
    }
    mult_mac_test_setup(data);
    return;
}

int main(int argc, char *argv[]) {
    std::map<std::string, std::function<void(TestData &)>> tests = {
	{"broadcast_test", broadcast_test_setup},
	{"learning_test", learning_test_setup},
	{"aging_test", aging_test_setup},
	{"mult_mac_test", mult_mac_test_setup},
	{"vlan_broadcast_test", vlan_broadcast_test_setup},
	{"vlan_mac_tbl_test", vlan_mac_tbl_test_setup},
	{"vlan_intf_outside_mac_tbl_test", vlan_intf_outside_mac_tbl_test_setup},
	{"multiple_vlans_test", multiple_vlans_test_setup},
	{"vlan_removal_test", vlan_removal_test_setup}
    };

    // Validate command line argument. Ensure the given strings corresponds to a valid test.
    if(argc != 2) {
	std::cerr << "Expected exactly 1 argument." << std::endl;
	return TestData::FAIL;
    }

    srand((unsigned)time(NULL));

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

    for(auto &wave : data.test_waves) {
	for(auto [pckt, intf_ptr] : wave.pckts_to_transmit) {
	    intf_ptr->sendPacket(pckt);
	}

	pcpp::multiPlatformSleep(wave.delay);

	auto rslt = evaluate_wave_results(data, wave);
	if(rslt == TestData::FAIL) {
	    std::cerr << "FAIL: At wave " << (data.cur_wave + 1) << std::endl;
	    return rslt;
	}

	data.cur_wave++;
    }

    for(auto intf : veth_intfs) {
	intf->stopCapture();
	intf->close();
    }

    return TestData::PASS;
}
