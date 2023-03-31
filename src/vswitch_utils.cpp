/*
 * vswitch_utils.cpp - Implemenation file for project utitlities.
 *
 * Contains definitions for all utility functions in include/vswitch_utils.hpp
 */

#include <iostream>
#include <string>
#include <vector>
#include <PcapLiveDevice.h>
#include <PcapLiveDeviceList.h>
#include "vswitch_utils.hpp"

std::vector<pcpp::PcapLiveDevice *> get_intfs_prefixed_by(const std::string &prefix) {
    std::vector<pcpp::PcapLiveDevice *> veth_intfs, all_intfs =
	pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();

    for(auto intf : all_intfs) {
	std::string intf_name = intf->getName();
	if(intf_name.compare(0, prefix.size(), prefix) == 0) {
	    if(!intf->open()) {
		std::cerr << "Could not open intf " << intf_name << std::endl;
	    }
	    veth_intfs.push_back(intf);
	}
    }

    return veth_intfs;
}
