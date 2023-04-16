/*
 * vswitch_shmem.hpp - Header file for VswitchShemem.
 *
 * Stores various information about the current state of the virtual switch. Only one instance of
 * this class should be created, and will be read and written to by most threads.
 */

#ifndef VSWITCH_SHMEM_HPP
#define VSWITCH_SHMEM_HPP

#include "counters.hpp"
#include "mac_addr_table.hpp"
#include "packet_queue.hpp"
#include "duplicate_manager.hpp"
#include "vlans.hpp"

class VswitchShmem {
public:
    VswitchShmem(std::vector<pcpp::PcapLiveDevice *> veth_intfs)
	: veth_intfs(veth_intfs),
	  counters(veth_intfs.size()),
	  dup_mgr(veth_intfs.size()),
	  vlans(veth_intfs.size())
	{}

    std::vector<pcpp::PcapLiveDevice *> veth_intfs;
    Counters counters;
    PacketQueue packet_queue;
    DuplicateManager dup_mgr;
    MacAddrTable mac_tbl;
    Vlans vlans;
};

#endif // VSWITCH_SHMEM_HPP
