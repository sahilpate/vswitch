/*
 * duplicate_manager.hpp - Header file for DuplicateManager.
 *
 * In PcapPlusPlus when egressing a packet out of an interface which has also been opened for
 * capturing, that packet will be detected as ingressing on that same interface. This data structure
 * keeps a record of packets and the interface they have egressed on so that they can be ignored
 * when they're re-detected.
 */

#ifndef DUPLICATE_MANAGER_HPP
#define DUPLICATE_MANAGER_HPP

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <RawPacket.h>

class DuplicateManager {
public:
    DuplicateManager(int total_intfs);
    void mark_duplicate(int intf_indx, pcpp::RawPacket pckt);
    bool check_duplicate(int intf_indx, pcpp::RawPacket pckt);
    std::string to_string(std::string prefix = "");
    int num_packets_for_intf(long unsigned int intf_indx);

private:
    /*
     * RawPcktCompare - Overloads the () operator to, when given two pcpp::RawPacket's, provide a
     * unique comparison between their actual values.
     */
    struct RawPcktCompare {
	bool operator() (const pcpp::RawPacket &a, const pcpp::RawPacket &b) const;
    };

    std::vector<std::map<pcpp::RawPacket, int, RawPcktCompare>> seen;
    std::vector<std::mutex> intf_locks;
};

#endif // DUPLICATE_MANAGER_HPP
