#ifndef MAC_ADDR_TABLE_HPP
#define MAC_ADDR_TABLE_HPP

#include <map>
#include <mutex>
#include <MacAddress.h>
#include <PcapLiveDevice.h>

/*
 * MacAddrTable - An abstraction for the table used to make forwarding decisions. This enables self
 * learning, where mappings from a given MAC address to an interface are added as frames arrive,
 * read from when deciding where to forward frames, and aged out over time.
 */
class MacAddrTable {
public:
    void push_mapping(pcpp::MacAddress mac_addr, pcpp::PcapLiveDevice *intf);
    pcpp::PcapLiveDevice *get_mapping(pcpp::MacAddress mac_addr);
    int age_mappings();
    void print_mactbl(std::ostream &out);

    static const int max_age = 5; // in seconds

private:
    /*
     * MacAddrCompare - Overloads the () operator to, when given two pcpp::MacAddress's, provide a
     * unique comparison between their actual values. It enables the pcpp::MacAddress type to be
     * used with various standard containers or library functions which need a comparison function,
     * typically for sorting.
     */
    struct MacAddrCompare {
	bool operator() (const pcpp::MacAddress &a, const pcpp::MacAddress &b) const;
    };


    std::map<pcpp::MacAddress,
	     std::pair<pcpp::PcapLiveDevice *, std::time_t>,
	     MacAddrCompare> table;
    std::mutex table_access;
};

#endif // MAC_ADDR_TABLE_HPP
