/*
 * packet_queue.hpp - Header file for PQueue and PQueueEntry.
 *
 * PacketQueue is a thread-safe, FIFO queue implemented using a fixed size circular buffer.
 * It follows the "best effort" model; if the queue is full when attemping to push a new element,
 * that element is immediately dropped (rather than waiting for space to be made).
 *
 * PQueueEntry represents a queue entry in the PacketQueue class. The raw packet itself and the
 * interface it came in on are recorded when initially pushed onto the queue. Other information is
 * filled in during processing, and that info is used when popping and egressing the packet.
 */

#ifndef PACKET_QUEUE_HPP
#define PACKET_QUEUE_HPP

#include <condition_variable>
#include "vlans.hpp"

class PQueueEntry {
public:
    PQueueEntry();
    PQueueEntry(pcpp::RawPacket pckt, pcpp::PcapLiveDevice *src_intf);

    pcpp::RawPacket pckt;
    pcpp::PcapLiveDevice *src_intf;
    std::vector<pcpp::PcapLiveDevice *> dst_intfs;
};

class PacketQueue {
public:
    bool push_packet(pcpp::RawPacket pckt, pcpp::PcapLiveDevice *src_intf);
    void process_packet(MacAddrTable *mac_tbl,
			Vlans *vlans,
			std::vector<pcpp::PcapLiveDevice *> *veth_intfs);
    PQueueEntry pop_packet();

private:
    int in = 0, space = queue_size; // producer
    int proc = 0, to_proc = 0;      // packet processor
    int out = 0, objects = 0;       // consumer

    std::condition_variable proc_cond, cons_cond;
    std::mutex prod_mtx, proc_mtx, cons_mtx;

    const static int queue_size = 50;
    PQueueEntry packet_queue[queue_size];
};

#endif // PACKET_QUEUE_HPP
