/*
 * packet_queue.cpp - Implementation of the PacketQueue and PQueueEntry classes.
 */

#include <vector>
#include <EthLayer.h>
#include <RawPacket.h>
#include "mac_addr_table.hpp"
#include "packet_queue.hpp"
#include "vlans.hpp"

#include <iostream>

PQueueEntry::PQueueEntry() {}
PQueueEntry::PQueueEntry(pcpp::RawPacket pckt, pcpp::PcapLiveDevice *src_intf)
    : pckt(pckt),
      src_intf(src_intf)
{}

bool PacketQueue::push_packet(pcpp::RawPacket pckt, pcpp::PcapLiveDevice *src_intf) {
    PQueueEntry obj(pckt, src_intf);

    prod_mtx.lock();

    if(space == 0) {
	prod_mtx.unlock();
	return false;
    }

    packet_queue[in] = obj;
    in = (in + 1) % queue_size;
    space--;
    prod_mtx.unlock();

    proc_mtx.lock();
    to_proc++;
    proc_mtx.unlock();
    proc_cond.notify_one();

    return true;
}

void PacketQueue::process_packet(MacAddrTable *mac_tbl,
				 Vlans *vlans,
				 std::vector<pcpp::PcapLiveDevice *> *veth_intfs) {
    std::unique_lock<std::mutex> local_mtx(proc_mtx);
    while(to_proc == 0) {
	proc_cond.wait(local_mtx);
    }

    // Update MAC address table based on incoming packet
    PQueueEntry &entry = packet_queue[proc];
    pcpp::Packet parsed_pckt(&entry.pckt);
    pcpp::EthLayer *eth_layer = parsed_pckt.getLayerOfType<pcpp::EthLayer>();
    mac_tbl->push_mapping(eth_layer->getSourceMac(), entry.src_intf);

    // Make forwarding decision based on MAC table
    std::vector<pcpp::PcapLiveDevice *> out_intfs;
    pcpp::PcapLiveDevice *mapping = mac_tbl->get_mapping(eth_layer->getDestMac());

    // Get vector index of the current interface

    // TODO: Change MAC address table to use interface indices rather than PcapLiveDevices for addr
    // to intf mappings. Since the interface vector is constant through the runtime of the switch,
    // it should be safe to use their integer index to identify them.
    long unsigned int cur_intf;
    for(cur_intf = 0; cur_intf < veth_intfs->size(); cur_intf++) {
	if((*veth_intfs)[cur_intf] == entry.src_intf) {
	    break;
	}
    }

    int in_intf_vlan = vlans->get_vlan_for_intf(cur_intf);
    if(mapping == nullptr) {
	// Broadcast to intfs in VLAN if no mapping exists
	for(long unsigned int i = 0; i < veth_intfs->size(); i++) {
	    if(i == cur_intf || vlans->get_vlan_for_intf(i) != in_intf_vlan) {
		continue;
	    }

	    out_intfs.push_back((*veth_intfs)[i]);
	}
    } else if(mapping != entry.src_intf) {
	// Otherwise, if the packet is destined for a different intf from the src and exists on the
	// same VLAN, forward to it
	long unsigned int dst_intf;
	for(dst_intf = 0; dst_intf < veth_intfs->size(); dst_intf++) {
	    if((*veth_intfs)[dst_intf] == mapping) {
		break;
	    }
	}

	if(vlans->get_vlan_for_intf(dst_intf) == in_intf_vlan) {
	    out_intfs.push_back(mapping);
	}
    }

    entry.dst_intfs = out_intfs;

    // Increment buffer pointers
    proc = (proc + 1) % queue_size;
    to_proc--;
    local_mtx.~unique_lock<std::mutex>();

    cons_mtx.lock();
    objects++;
    cons_mtx.unlock();
    cons_cond.notify_one();

    return;
}

PQueueEntry PacketQueue::pop_packet() {
    std::unique_lock<std::mutex> local_mtx(cons_mtx);
    while(objects == 0) {
	cons_cond.wait(local_mtx);
    }

    PQueueEntry popped_val = packet_queue[out];
    out = (out + 1) % queue_size;
    objects--;
    local_mtx.~unique_lock<std::mutex>();

    prod_mtx.lock();
    space++;
    prod_mtx.unlock();

    return popped_val;
}
