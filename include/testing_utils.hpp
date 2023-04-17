/*
 * testing_utils.hpp - Header file for various testing utilites.
 *
 * This contains declarations for a class and various functions to simplify interthread
 * communication between packet capturing threads created by PcapPlusPlus, as well as helper
 * functions for crafting packets, evaluating packets as they arrive, and evalutating test results
 * once all packets have been transmitted and received.
 */

#ifndef TESTING_UTILS_HPP
#define TESTING_UTILS_HPP

#include <mutex>
#include <vector>
#include <PcapLiveDevice.h>
#include <RawPacket.h>
#include "duplicate_manager.hpp"

class TestWave {
public:
    TestWave(long unsigned num_intfs);
    TestWave(long unsigned num_intfs, unsigned delay);

    DuplicateManager expected;
    std::vector<std::pair<pcpp::RawPacket, pcpp::PcapLiveDevice *>> pckts_to_transmit;
    unsigned delay;
};

/*
 * TestData - This is shared between the all running threads of the testing container program. It
 * is to be populated with various information about the current running test, most notably: the
 * the packets to be transmitted out each interface, and the packets which are expected to arrive
 * on each interface.

 */
class TestData {
public:
    enum status {FAIL, PASS, IN_PROGRESS};

    TestData(std::vector<pcpp::PcapLiveDevice *> veth_intfs);

    std::vector<TestWave> test_waves;
    long unsigned cur_wave;
    DuplicateManager dup_mgr;
    std::vector<pcpp::PcapLiveDevice *> veth_intfs;
    status test_status;
    std::mutex err_out;
};

/*
 * create_broadcast_pckt() - Returns a packet whose source MAC and IPv4 address matches that of the
 * given interface and whose destinations are the broadcast addresses. This packet also has an
 * empty payload.
 */
pcpp::RawPacket create_broadcast_pckt(pcpp::PcapLiveDevice *src_intf);

/*
 * verify_packet() - Passed to pcpp::PcapLiveDevice.startCapture(). It is called on every incoming
 * packet, and verifies that it is not a duplicate (see DuplicateManager) and that is a packet we
 * expected to see. If the packet is unexpected, it outputs an error message and marks the test as
 * failed.
 */
void verify_packet(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie);

/*
 * evaluate_wave_results() - After all test packets have been transmitted and the virtual switch
 * has bridged them back to the testing container, this verifies that all the packets we expected
 * to see were observed, and returns the wave's test result after doing this.
 */
TestData::status evaluate_wave_results(TestData &data, TestWave &cur_wave);

#endif // TESTING_UTILS_HPP
