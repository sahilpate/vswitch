/*
 * vlans.cpp - Implementation file for Vlans
 *
 * Implements all of the class functions declared in include/vlans.hpp.
 */

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
    if(vlan == DEFAULT_VLAN || vlan < 0 || vlan > 4094) {
	return false;
    }

    vlans.insert(vlan);
    return true;
}

bool Vlans::remove_vlan(int vlan) {
    if(vlan == DEFAULT_VLAN || vlan < 0 || vlan > 4094) {
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
