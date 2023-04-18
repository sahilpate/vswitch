/*
 * mac_addr_table.cpp - Implementation for the MacAddrTable class as well as a comparator operator
 * for raw MAC address values.
 */

#include <iomanip>
#include <iostream>
#include "mac_addr_table.hpp"

void MacAddrTable::push_mapping(pcpp::MacAddress mac_addr, pcpp::PcapLiveDevice *intf) {
    table_access.lock();
    table[mac_addr] = {intf, std::time(nullptr)};
    table_access.unlock();
}

pcpp::PcapLiveDevice *MacAddrTable::get_mapping(pcpp::MacAddress mac_addr) {
    table_access.lock();
    auto table_it = table.find(mac_addr);
    if(table_it == table.end()) {
	table_access.unlock();
	return nullptr;
    }

    pcpp::PcapLiveDevice *ret_intf = table_it->second.first;
    table_access.unlock();
    return ret_intf;
}

int MacAddrTable::age_mappings() {
    int num_aged_out = 0;
    auto table_it = table.begin();
    std::time_t elem_time, cur_time = std::time(nullptr);
    double diff;

    table_access.lock();
    while(table_it != table.end()) {
	elem_time = table_it->second.second;
	diff = difftime(cur_time, elem_time);

	if(diff > max_age) {
	    num_aged_out++;
	    table.erase(table_it++);
	} else {
	    table_it++;
	}
    }
    table_access.unlock();

    return num_aged_out;
}

unsigned MacAddrTable::get_max_age() {
    unsigned cur_max_age;
    table_access.lock();
    cur_max_age = max_age;
    table_access.unlock();
    return cur_max_age;
}

bool MacAddrTable::modify_aging_time(unsigned int new_age) {
    if(new_age < 1) {
	return false;
    }
    table_access.lock();
    max_age = new_age;
    table_access.unlock();
    return true;
}

void MacAddrTable::print_mactbl(std::ostream &out) {
    this->age_mappings();
    std::vector<std::string> headers = {"Mac Addresses", "Ports", "Time to Live"};

    out << std::setw(30) << "" << "Mac Address Table" << std::endl;
    out << std::string(80, '-') << std::endl << std::endl;

    for(auto str : headers) {
	out << std::setw(20) << std::left << str;
    }
    out << std::endl;
    for(auto str : headers) {
	out << std::setw(20) << std::left << std::string(str.size(), '-');
    }
    out << std::endl;

    table_access.lock();
    for(auto [mac_addr, info] : table) {
	auto [intf, timestamp] = info;
	double ttl = max_age - difftime(std::time(nullptr), timestamp);

	std::cout << std::setw(20) << std::left << mac_addr.toString();
	std::cout << std::setw(20) << std::left << intf->getName();
	std::cout << std::setw(20) << std::left << ttl << std::endl;
    }
    table_access.unlock();
    std::cout << std::endl;
    return;
}

bool MacAddrTable::MacAddrCompare::operator() (const pcpp::MacAddress &a,
					       const pcpp::MacAddress &b) const {
    uint64_t a_full = 0, b_full = 0;

    a.copyTo(reinterpret_cast<uint8_t *>(&a_full));
    b.copyTo(reinterpret_cast<uint8_t *>(&b_full));

    return a_full < b_full;
}
