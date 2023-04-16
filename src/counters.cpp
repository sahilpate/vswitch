/*
 * counters.cpp - Implementation file for the Counters class
 *
 * Contains the implementation for all functions in include/counters.hpp, including ones to
 * increment interface byte and packet counts, create a snapshot of the class's current state,
 * and print the current values stored in the class (subtracted by the latest snapshot).
 */

#include <iomanip>
#include "counters.hpp"

Counters::Counters(long unsigned size)
    : counters(size),
      counters_snapshot(size),
      ingress_locks(size),
      egress_locks(size)
{}

void Counters::increment_counters(int intf, int bytes, CntType type) {
    if(intf < 0 || intf >= static_cast<int>(counters.size())) {
	return;
    }

    switch(type) {
    case ING:
	ingress_locks[intf].lock();
	counters[intf].ingress_pckts++;
	counters[intf].ingress_bytes += bytes;
	ingress_locks[intf].unlock();
	break;

    case EGR:
	egress_locks[intf].lock();
	counters[intf].egress_pckts++;
	counters[intf].egress_bytes += bytes;
	egress_locks[intf].unlock();
	break;
    }

    return;
}

void Counters::create_snapshot() {
    for(long unsigned i = 0; i < counters.size(); i++) {
	ingress_locks[i].lock();
	counters_snapshot[i].ingress_bytes = counters[i].ingress_bytes;
	counters_snapshot[i].ingress_pckts = counters[i].ingress_pckts;
	ingress_locks[i].unlock();

	egress_locks[i].lock();
	counters_snapshot[i].egress_bytes = counters[i].egress_bytes;
	counters_snapshot[i].egress_pckts = counters[i].egress_pckts;
	egress_locks[i].unlock();
    }

    return;
}

void Counters::print_counters(std::ostream &out,
			      const std::vector<pcpp::PcapLiveDevice *> &veth_intfs) {
    int pad = 16;
    std::vector<std::string> headers = {"Port", "InBytes", "InPckts", "OutBytes", "OutPckts"};

    out << std::setw(pad) << std::left << headers[0];
    for(long unsigned int i = 1; i < headers.size(); i++) {
	out << std::setw(pad) << std::right << headers[i];
    }
    out << std::endl;

    for(long unsigned int i = 0; i < veth_intfs.size(); i++) {
	out << std::setw(pad) << std::left << veth_intfs[i]->getName();

	out << std::right;

	ingress_locks[i].lock();
	out << std::setw(pad) << (counters[i].ingress_bytes - counters_snapshot[i].ingress_bytes);
	out << std::setw(pad) << (counters[i].ingress_pckts - counters_snapshot[i].ingress_pckts);
	ingress_locks[i].unlock();

	egress_locks[i].lock();
	out << std::setw(pad) << (counters[i].egress_bytes - counters_snapshot[i].egress_bytes);
	out << std::setw(pad) << (counters[i].egress_pckts - counters_snapshot[i].egress_pckts);
	egress_locks[i].unlock();

	out << std::endl;
    }

    out << std::endl;
    return;
}
