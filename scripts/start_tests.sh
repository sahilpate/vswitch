#!/bin/bash

if [ "$UID" != "0" ]
then
    echo -e "This script must be run with root permissions to create Docker" \
	 "containers and virtual ethernet interfaces.\nRun at your own risk."
    exit 1
fi

num_intfs=$1
src_path=$2
switch_name="vswitch"
test_name="vswitch-testing"
single_dig_re=^[1-9]$

# Validate user input
if [ -z $num_intfs ] || [ -z $src_path ]
then
    echo "Usage: start-tests.sh {num_intfs} {src_path}"
    echo "Pass two args, the # of intfs between the vswitch and testing" \
	 "containers, and the absolute path to the source directory for this" \
	 "project (which will be mounted to the vswitch/testing containers)."
    exit 1
fi

# Create a vswitch container and a testing container.
cont_ids[0]=$(docker run -itd --rm -v ${src_path}:/vswitch \
		     --name ${switch_name} host_template)
docker network disconnect bridge ${switch_name}
cont_ids[1]=$(docker run -itd --rm -v ${src_path}:/vswitch \
		     --name ${test_name} host_template)
docker network disconnect bridge ${test_name}

# Docker network namespaces are not visible through regular `ip netns` commands
# by default. To expose them, mount them to files in /var/run/netns. See
# https://www.baeldung.com/linux/docker-network-namespace-invisible.
mkdir -p /var/run/netns
for i in "${cont_ids[@]}"
do
    touch /var/run/netns/${i}
    cont_pid=$(docker inspect -f '{{.State.Pid}}' ${i})
    mount -o bind /proc/${cont_pid}/ns/net /var/run/netns/${i}
done

# Create virtual ethernet interfaces to connect the testing container to the
# vswitch. Interfaces are named with the convention "{src}-{dst}". Intfs at the
# vswitch are assigned IP addresses with the convention "192.168.0.1{test#}",
# and those at the testing container are done as "192.168.0.{test#}".
# Also, on each host, add a default route through the new veth intf.
for ((i = 1; i <= $num_intfs; i++))
do
    test_name="test${i}"
    intf_test_side="${test_name}-${switch_name}"
    intf_switch_side="${switch_name}-${test_name}"

    ip link add ${intf_test_side} type veth peer name ${intf_switch_side}
    ip link set ${intf_test_side} netns ${cont_ids[1]}
    ip link set ${intf_switch_side} netns ${cont_ids[0]}

    padded=$i
    if [[ $i =~ $single_dig_re ]]
    then
	padded="0${i}"
    fi
    ip -n ${cont_ids[1]} addr add 192.168.0.${i} dev ${intf_test_side}
    ip -n ${cont_ids[0]} addr add 192.168.0.1${padded} dev ${intf_switch_side}

    ip -n ${cont_ids[1]} link set ${intf_test_side} up
    ip -n ${cont_ids[0]} link set ${intf_switch_side} up
done

docker ps
echo -e "\nInput anything to perform cleanup..."
read anything

# Stop (and delete) containers, and cleanup network namespace bindings
echo "Performing cleanup..."
for i in "${cont_ids[@]}"
do
    docker stop $i > /dev/null 2>&1
    echo "Stopped $i"
    ip netns delete $i
done
