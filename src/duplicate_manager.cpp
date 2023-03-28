/*
 * duplicate_manager.cpp - Implementation of the DuplicateManager class and a comparison operator
 * for raw packets.
 */

#include <map>
#include <mutex>
#include <vector>
#include <RawPacket.h>
#include "duplicate_manager.hpp"

DuplicateManager::DuplicateManager(int total_intfs)
    : seen(total_intfs),
      intf_locks(total_intfs)
{}

void DuplicateManager::mark_duplicate(int intf_indx, pcpp::RawPacket pckt) {
    auto &cur_map = seen[intf_indx];

    intf_locks[intf_indx].lock();
    if(cur_map.count(pckt) > 0) {
	cur_map[pckt]++;
    } else {
	cur_map[pckt] = 1;
    }
    intf_locks[intf_indx].unlock();
}

bool DuplicateManager::check_duplicate(int intf_indx, pcpp::RawPacket pckt) {
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

bool DuplicateManager::RawPcktCompare::operator() (const pcpp::RawPacket &a,
						   const pcpp::RawPacket &b) const {
    for(int i = 0; i < a.getRawDataLen() && i < b.getRawDataLen(); i++) {
	if((a.getRawData())[i] == (b.getRawData())[i]) {
	    continue;
	} else {
	    return (a.getRawData())[i] < (b.getRawData())[i];
	}
    }

    return a.getRawDataLen() < b.getRawDataLen();
}
