/*
 * counters.hpp - Header file for Counters
 *
 * This maintains the ingress/egress byte and packet counts for traffic entering and exiting the
 * vswitch on a per-interface basis. It is thread safe and is primarily used with the CLI for the
 * user to monitor the flow of traffic through the program.
 */

#ifndef COUNTERS_HPP
#define COUNTERS_HPP

#include <iostream>
#include <mutex>
#include <vector>
#include <PcapLiveDevice.h>

class Counters {
public:
    enum CntType {
	ING, EGR
    };

    Counters(long unsigned size);
    void increment_counters(int intf, int bytes, CntType type);
    void create_snapshot();
    void print_counters(std::ostream &out, const std::vector<pcpp::PcapLiveDevice *> &veth_intfs);

private:
    struct CounterData {
	int ingress_pckts = 0;
	int egress_pckts = 0;
	int ingress_bytes = 0;
	int egress_bytes = 0;
    };

    std::vector<struct CounterData> counters;
    std::vector<struct CounterData> counters_snapshot;
    std::vector<std::mutex> ingress_locks;
    std::vector<std::mutex> egress_locks;
};

#endif // COUNTERS_HPP
