/*
 * mac_addr_table.hpp - Header file for MacAddrTable.
 *
 * An abstraction for the table used to make forwarding decisions. This enables self learning, where
 * mappings from a given MAC address to an interface are added as frames arrive, read from when
 * deciding where to forward frames, and aged out over time.
 */

#ifndef MAC_ADDR_TABLE_HPP
#define MAC_ADDR_TABLE_HPP

#include <map>
#include <mutex>
#include <MacAddress.h>
#include <PcapLiveDevice.h>

class MacAddrTable {
public:
    void push_mapping(pcpp::MacAddress mac_addr, pcpp::PcapLiveDevice *intf);
    pcpp::PcapLiveDevice *get_mapping(pcpp::MacAddress mac_addr);
    int age_mappings();
    unsigned get_max_age();
    bool modify_aging_time(unsigned int new_age);
    void print_mactbl(std::ostream &out);

private:
    /*
     * MacAddrCompare - Overloads the () operator to, when given two pcpp::MacAddress's, provide a
     * unique comparison between their actual values.
     */
    struct MacAddrCompare {
	bool operator() (const pcpp::MacAddress &a, const pcpp::MacAddress &b) const;
    };


    std::map<pcpp::MacAddress,
	     std::pair<pcpp::PcapLiveDevice *, std::time_t>,
	     MacAddrCompare> table;
    std::mutex table_access;
    int max_age = 15; // in seconds
};

#endif // MAC_ADDR_TABLE_HPP
