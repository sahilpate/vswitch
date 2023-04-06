/*
 * vlans.hpp - Header file for Vlans
 *
 * This provides an abstraction for the active VLANs in the system. It maintains a set of the VLANs
 * the user has created, and a vector that represents the interface-to-VLAN mapping. It has public
 * accessor and mutator functions to ensure users of this class do not attempt to create or operate
 * on interfaces and VLANs which should not or do not exist.
 *
 * Thread safe access has only been implemented for the intf-to-VLAN mapping vector since it is the
 * only data structure that should be accessed by multiple threads. Any accessing or mutating of the
 * VLAN set (add_vlan(), remove_vlan(), and add_intf_to_vlan()) is not thread safe since only one
 * thread, the CLI user, should call them.
 */

#ifndef VLANS_HPP
#define VLANS_HPP

#include <mutex>
#include <set>
#include <vector>

class Vlans {
public:
    Vlans(int num_intfs);
    int get_vlan_for_intf(int intf);
    bool add_vlan(int vlan);
    bool remove_vlan(int vlan);
    bool add_intf_to_vlan(int intf, int vlan);

private:
    const int DEFAULT_VLAN = 1;
    std::vector<int> intf_to_vlan;
    std::vector<std::mutex> intf_vlan_mapping_access;
    std::set<int> vlans;
};

#endif // VLANS_HPP
