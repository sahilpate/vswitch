#ifndef DUPLICATE_MANAGER_HPP
#define DUPLICATE_MANAGER_HPP

/*
 * DuplicateManager - In PcapPlusPlus when egressing a packet out of an interface which has also
 * been opened for capturing, that packet will be detected as ingressing on that same interface.
 * This data structure keeps a record of packets and the interface they have egressed on so that
 * they can be ignored when they're re-detected.
 */
class DuplicateManager {
public:
    DuplicateManager(int total_intfs);
    void mark_duplicate(int intf_indx, pcpp::RawPacket pckt);
    bool check_duplicate(int intf_indx, pcpp::RawPacket pckt);

private:
    /*
     * RawPcktCompare - Overloads the () operator to, when given two pcpp::RawPacket's, provide a
     * unique comparison between their actual values. It enables the pcpp::RawPacket type to be used
     * with various standard containers or library functions which need a comparison function,
     * typically for sorting.
     */
    struct RawPcktCompare {
	bool operator() (const pcpp::RawPacket &a, const pcpp::RawPacket &b) const;
    };

    std::vector<std::map<pcpp::RawPacket, int, RawPcktCompare>> seen;
    std::vector<std::mutex> intf_locks;
};

#endif // DUPLICATE_MANAGER_HPP
