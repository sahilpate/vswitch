/*
 * duplicate_manager.cpp - Implementation of the DuplicateManager class and a comparison operator
 * for raw packets.
 */

#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <Packet.h>
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

std::string DuplicateManager::to_string(std::string prefix) {
    std::ostringstream oss;

    for(long unsigned int i = 0; i < seen.size(); i++) {
	oss
	    << prefix
	    << std::string(20, '=')
	    << " Packets stored for interface "
	    << i << ' '
	    << std::string(20, '=')
	    << std::endl;

	intf_locks[i].lock();
	for(auto [raw_pckt, cnt] : seen[i]) {
	    pcpp::RawPacket raw_pckt_cpy(raw_pckt);
	    pcpp::Packet parsed_pckt(&raw_pckt_cpy);

	    oss << prefix << cnt;
	    cnt == 1 ? oss << " copy of" : oss << " copies of";
	    oss << std::endl;

	    std::vector<std::string> pckt_strings;
	    parsed_pckt.toStringList(pckt_strings);
	    for(auto s : pckt_strings) {
		oss << prefix << s << std::endl;
	    }
	    oss << std::endl;
	}
	intf_locks[i].unlock();
    }

    return oss.str();
}

int DuplicateManager::num_packets_for_intf(long unsigned int intf_indx) {
    if(intf_indx >= seen.size()) {
	throw std::out_of_range("Attemping to read out of range of duplicate_manager seen vector.");
    }
    return seen[intf_indx].size();
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
