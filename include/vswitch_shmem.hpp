/*
 * vswitch_shmem.hpp - Header file for VswitchShemem.
 *
 * Stores various information about the current state of the virtual switch. Only one instance of
 * this class should be created, and will be read and written to by most threads.
 */

#ifndef VSWITCH_SHMEM_HPP
#define VSWITCH_SHMEM_HPP

#include "mac_addr_table.hpp"
#include "packet_queue.hpp"
#include "duplicate_manager.hpp"

class VswitchShmem {
public:
    VswitchShmem(std::vector<pcpp::PcapLiveDevice *> veth_intfs)
	: veth_intfs(veth_intfs),
	  ingress_count(veth_intfs.size(), 0),
	  dup_mgr(veth_intfs.size())
	{}

    std::vector<pcpp::PcapLiveDevice *> veth_intfs;
    std::vector<int> ingress_count;
    PacketQueue packet_queue;
    DuplicateManager dup_mgr;
    MacAddrTable mac_tbl;
};

#endif // VSWITCH_SHMEM_HPP
