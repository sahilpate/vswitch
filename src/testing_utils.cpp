/*
 * testing_utils.cpp - Implementation file for the TestData class and other various testing utility
 * functions, all defined in include/testing_utils.hpp
 */

#include <iostream>
#include <EthLayer.h>
#include <IPv4Layer.h>
#include <SystemUtils.h>
#include "duplicate_manager.hpp"
#include "testing_utils.hpp"

TestData::TestData(std::vector<pcpp::PcapLiveDevice *> veth_intfs)
    : dup_mgr(veth_intfs.size()),
      expected(veth_intfs.size()),
      veth_intfs(veth_intfs),
      test_status(IN_PROGRESS)
{}

pcpp::RawPacket create_broadcast_pckt(pcpp::PcapLiveDevice *src_intf) {
    pcpp::EthLayer eth_layer(src_intf->getMacAddress(),
			     pcpp::MacAddress("ff:ff:ff:ff:ff:ff"));

    pcpp::IPv4Layer ip_layer(src_intf->getIPv4Address(),
			     pcpp::IPv4Address("255.255.255.255"));
    ip_layer.getIPv4Header()->ipId = pcpp::hostToNet16(2000);
    ip_layer.getIPv4Header()->timeToLive = 64;

    pcpp::Packet pckt(eth_layer.getHeaderLen() + ip_layer.getHeaderLen());
    pckt.addLayer(&eth_layer);
    pckt.addLayer(&ip_layer);
    pckt.computeCalculateFields();
    return *pckt.getRawPacket();
}

void verify_packet(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie) {
    TestData *data = static_cast<TestData *>(cookie);
    pcpp::Packet parsed_packet(packet);

    for(long unsigned int i = 0; i < data->veth_intfs.size(); i++) {
	if(dev != data->veth_intfs[i]) {
	    continue;
	}

	if(data->dup_mgr.check_duplicate(i, *packet)) {
	    return;
	}

	if(data->expected.check_duplicate(i, *packet)) {
	    return;
	} else {
	    data->err_out.lock();
	    std::cerr
		<< "\tFAIL: Saw unexpected packet on interface "
		<< dev->getName()
		<< std::endl;

	    std::vector<std::string> pckt_strings;
	    parsed_packet.toStringList(pckt_strings);
	    for(auto s : pckt_strings) {
		std::cerr << '\t' << s << std::endl;
	    }
	    std::cerr << std::endl;

	    data->err_out.unlock();
	    data->test_status = TestData::FAIL;
	}
    }
    return;
}

TestData::status evaluate_test_results(TestData &data) {
    bool all_not_seen = false;
    for(long unsigned int i = 0; i < data.veth_intfs.size(); i++) {
	if(data.expected.num_packets_for_intf(i) > 0) {
	    data.test_status = TestData::FAIL;
	    all_not_seen = true;
	    std::cerr
		<< "\tFAIL: Not all expected packets were seen on interface "
		<< data.veth_intfs[i]->getName()
		<< std::endl;
	}
    }

    if(all_not_seen) {
	std::cerr << "\tThe following packets were expected to arrive, but did not:" << std::endl;
	std::cerr << data.expected.to_string("\t");
    }

    if(data.test_status == TestData::IN_PROGRESS) {
	return TestData::PASS;
    }
    return TestData::FAIL;
}
