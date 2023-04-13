/*
 * vlans.cpp - Implementation file for Vlans
 *
 * Implements all of the class functions declared in include/vlans.hpp.
 */

#include <iomanip>
#include <PcapLiveDevice.h>
#include "vlans.hpp"

Vlans::Vlans(int num_intfs)
    : intf_to_vlan(num_intfs),
      intf_vlan_mapping_access(num_intfs) {
    for(int i = 0; i < num_intfs; i++) {
	intf_to_vlan[i] = DEFAULT_VLAN;
	vlans.insert(DEFAULT_VLAN);
    }
}

int Vlans::get_vlan_for_intf(int intf) {
    int vlan;
    if(intf < 0 || intf >= static_cast<int>(intf_to_vlan.size())) {
	return -1;
    }

    intf_vlan_mapping_access[intf].lock();
    vlan = intf_to_vlan[intf];
    intf_vlan_mapping_access[intf].unlock();
    return vlan;
}

bool Vlans::add_vlan(int vlan) {
    if(vlan == DEFAULT_VLAN || vlan <= 0 || vlan > 4094) {
	return false;
    }

    vlans.insert(vlan);
    return true;
}

bool Vlans::remove_vlan(int vlan) {
    if(vlan == DEFAULT_VLAN || vlan <= 0 || vlan > 4094) {
	return false;
    }

    for(long unsigned int i = 0; i < intf_to_vlan.size(); i++) {
	intf_vlan_mapping_access[i].lock();
	if(intf_to_vlan[i] == vlan) {
	    intf_to_vlan[i] = DEFAULT_VLAN;
	}
	intf_vlan_mapping_access[i].unlock();
    }
    vlans.erase(vlan);
    return true;
}

bool Vlans::add_intf_to_vlan(int intf, int vlan) {
    if(vlans.count(vlan) == 0) {
	return false;
    }

    if(intf < 0 || intf >= static_cast<int>(intf_to_vlan.size())) {
	return false;
    }

    intf_vlan_mapping_access[intf].lock();
    intf_to_vlan[intf] = vlan;
    intf_vlan_mapping_access[intf].unlock();

    return true;
}

void Vlans::print_vlans(std::ostream &out, const std::vector<pcpp::PcapLiveDevice *> &veth_intfs) {
    std::vector<std::pair<std::string, int>> headers = {
	{"VLAN", 5},
	{"Ports", 73}
    };

    for(auto [name, len] : headers) {
	out << std::setw(len + 1) << std::left << name;
    }
    out << std::endl;
    for(auto [name, len] : headers) {
	out << std::string(len, '-') << ' ';
    }
    out << std::endl;

    for(auto vlan : vlans) {
	std::string intfs;

	out << std::setw(headers[0].second + 1) << std::left << vlan;
	for(long unsigned i = 0; i < intf_to_vlan.size(); i++) {
	    if(intf_to_vlan[i] != vlan) {
		continue;
	    }

	    intfs.append(veth_intfs[i]->getName());
	    intfs.append(", ");
	}
	if(intfs.size() > 1) {
	    intfs.erase(intfs.size() - 2, 1);
	}
	out << std::setw(headers[1].second) << std::left << intfs;
	out << std::endl;
    }
    out << std::endl;

    return;
}
