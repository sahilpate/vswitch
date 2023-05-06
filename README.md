# vswitch: Layer 2 Networking from Scratch
A program which emulates the behavior of a managed network switch on Linux using Docker, C++, and [PcapPlusPlus](https://pcapplusplus.github.io/). It was created to fulfill my [project requirement](https://www.uml.edu/honors/project-thesis/) for the Honors College at the University of Massachusetts, Lowell.

![Alt text](/screenshots/main_demo.png)

## Build
This project depends on PcapPlusPlus for packet capturing, parsing, and crafting. To install it, refer to the project's [official documentation](https://pcapplusplus.github.io/docs/install).

It also depends on Flex for scanning CLI tokens, CMake for compilation, and Docker for container configuration. On a Debian based system, install them with the following commands.
```
sudo apt install flex
sudo apt install cmake
sudo apt install podman-docker
```

To build the project itself, clone it from GitHub and run the following CMake commands.
```
git clone https://github.com/sahilpate/vswitch
cd vswitch
cmake --build build
cmake -S . -B build
```

### Docker Image
Scripts are included to create an array of Docker containers to be used in conjunction with the executables produced in the build process. These containers use the provided image named `host_template`, which is Ubuntu with some basic networking tools. To load it, run the following.
```
docker load < images/host_template.tar
```

If this fails, create your own version of the image by running the following.

```
docker run -itd --rm --name tmp ubuntu
docker exec tmp apt update
docker exec tmp apt upgrade
docker exec tmp apt install iproute2 -y
docker exec tmp apt-get install iputils-ping -y
docker exec tmp apt-get install traceroute -y
docker exec tmp apt-get install tcpdump -y
docker commit tmp host_template
docker stop tmp
```

## Container Configuration
### Main Docker Setup
To initialize the container configuration used for this project, run the script `scripts/init_hosts_switch.sh` with the following arguments. Because it mounts the containers' namespaces to the directory used by `ip netns` and also creates ip links, it must be run as root. Run at your own risk.
```
sudo scripts/init_hosts_switch.sh {num_hosts} {proj_directory}
```

The first argument specifies the number of host containers to generate, and the second is used to mount the project directory to the `vswitch` container. The configuration generated will resemble the following image.

![Alt text](/screenshots/docker_config_main.png)

### Testing Setup
The tests for this project use a slightly different configuration. Run `scripts/init_test_env.sh` with the following arguments to set it up.
```
sudo scripts/init_test_env.sh {num_ports} {proj_directory}
```

Run the `test_orchestrator` program to execute all tests.
```
sudo ./test_orchestrator
```

As the tests run, the configuration resembles the following image.
![Alt text](/screenshots/docker_config_tests.png)

## Accessing and Using the CLI
To enter the CLI, run the `vswitch` program from within the `vswitch` container with the following.
```
sudo docker exec -i vswitch vswitch/vswitch
```
All commands are loosely based off those found in the Arista [user manual](https://www.arista.com/assets/data/docs/Manuals/EOS-4.17.2F-Manual.pdf) for EOS version 4.17.2F.

### General Commands
![Alt text](/screenshots/cli_general.png)

`show interfaces` - Shows a listing of all ports the program is currently listening to and sending packets out of.

`show interfaces counters` - Shows the number of packets and bytes which have entered and exited each port.

`clear counters` - Resets all packet counters back to 0.

### MAC Address Table
![Alt text](/screenshots/cli_mac.png)

`show mac address-table` - Shows the current state of the MAC address table, which is used to make forwarding decisions.

`mac address-table aging-time {uint}` - Changes the time-to-live for all table entries.

`show mac address-table aging-time` - Shows the current time-to-live for all table entries

### VLANs
![Alt text](/screenshots/cli_vlan.png)

`show vlan` - Shows all VLANs, along with the ports on each of them.

`no vlan {uint}` - Removes the provided VLAN, if it's valid and not the default VLAN

`{port-name} vlan {uint}` - Places the given port onto the given VLAN if they are both valid.

## Thanks
Thanks Professors William Moloney and Benyuan Liu for supporting me through this project, Jim Kurose and Keith Ross for writing a [fantastic textbook](https://gaia.cs.umass.edu/kurose_ross/index.php), and the folks at Arista for giving me my first introduction to networking and the inspiration for this project.
