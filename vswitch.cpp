#include <iostream>
#include <map>
#include <IPv4Layer.h>
#include <Packet.h>
#include <PcapFileDevice.h>
#include <PcapLiveDeviceList.h>
#include <SystemUtils.h>

class Data {
public:
    Data(std::vector<pcpp::PcapLiveDevice *> veth_intfs) {
	this->veth_intfs = veth_intfs;
	this->ingress_count = std::vector<int>(veth_intfs.size(), 0);
	this->seen =
	    std::vector<std::map<uint8_t, int>>(veth_intfs.size(), std::map<uint8_t, int>());
    }

    std::vector<pcpp::PcapLiveDevice *> veth_intfs;
    std::vector<int> ingress_count;
    std::vector<std::map<uint8_t, int>> seen;
};

static void process_packet(pcpp::RawPacket *packet, pcpp::PcapLiveDevice *dev, void *cookie) {
    Data *data = static_cast<Data *>(cookie);

    pcpp::Packet parsedPacket(packet);

    for(int i = 0; i < data->veth_intfs.size(); i++) {
	if(dev == data->veth_intfs[i]) {

	    if(data->seen[i].count(*(packet->getRawData())) > 0) {
		std::cout
		    << "Received duplicate on "
		    << data->veth_intfs[i]->getName()
		    << ", Skipping: "
		    << parsedPacket << std::endl;
		return;
	    }

	    data->ingress_count[i]++;
	    std::cout
		<< data->ingress_count[i]
		<< ": Received a packet on "
		<< data->veth_intfs[i]->getName()
		<< ": "
		<< parsedPacket;
	}
    }

    for(int i = 0; i < data->veth_intfs.size(); i++) {
	if(dev != data->veth_intfs[i]) {
	    std::cout << "Sending to " << data->veth_intfs[i]->getName() << std::endl << std::endl;
	    data->seen[i][*(packet->getRawData())] = 0;
	    data->veth_intfs[i]->sendPacket(*packet);
	}
    }

    return;
}

int main(void) {
    std::vector<pcpp::PcapLiveDevice *> veth_intfs, all_intfs =
	pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();

    for(auto intf : all_intfs) {
	std::string intf_name = intf->getName();
	std::string prefix = "vswitch";
	if(intf_name.compare(0, prefix.size(), prefix) == 0) {
	    if(!intf->open()) {
		std::cerr << "Could not open intf " << intf_name << std::endl;
	    }
	    veth_intfs.push_back(intf);
	}
    }

    Data data(veth_intfs);

    std::cout << "Starting capture on:" << std::endl;
    for(auto intf : veth_intfs) {
	intf->startCapture(process_packet, &data);
	std::cout << intf->getName() << std::endl;
    }
    std::cout << std::endl;

    pcpp::multiPlatformSleep(10);
    for(auto intf : veth_intfs) {
	intf->stopCapture();
    }

    for(auto intf : veth_intfs) {
	std::cout << "Closing " << intf->getName() << std::endl;
	intf->close();
    }

    return 0;
}
