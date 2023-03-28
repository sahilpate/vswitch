#!/bin/bash

if [ "$UID" != "0" ]
then
    echo -e "This script must be run with root permissions to create Docker" \
	 "containers and virtual ethernet interfaces.\nRun at your own risk."
    exit 1
fi

num_conts=$1
src_path=$2
switch_name="vswitch"
num_re=^[1-9][0-9]{0,1}$
single_dig_re=^[1-9]$

# Validate user input
if [ -z $num_conts ] || [ -z $src_path ]
then
    echo "Usage: init-hosts-switch.sh {num_hosts} {src_path}"
    echo "Pass two args, the # of hosts to be connected to the vswitch, and" \
	 "the absolute path to the source directory for this project (which" \
	 "will be mounted to the vswitch container)."
    exit 1
fi

if ! [[ $num_conts =~ $num_re ]]
then
    echo "Please enter a positive number below 100."
    exit 1
fi

# Create a vswitch container, plus $num_conts additional host containers.
# Also disconnect them from the default, Docker bridge network.
cont_ids[0]=$(docker run -itd --rm -v ${src_path}:/vswitch \
		     --name ${switch_name} host_template)
docker network disconnect bridge ${switch_name}
for ((i = 1; i <= $num_conts; i++))
do
    cont_name="host${i}"
    cont_ids[i]=$(docker run -itd --rm --name ${cont_name} host_template)
    docker network disconnect bridge ${cont_name}
done

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

# Create virtual ethernet interfaces to connect each host to the virtual
# switch. Interfaces are named with the convention "{src}-{dst}". Intfs at the
# vswitch are assigned IP addresses with the convention "192.168.0.1{host#}",
# and those at the hosts are done as "192.168.0.{host#}".
# Also, on each host, add a default route through the new veth intf.
for ((i = 1; i <= $num_conts; i++))
do
    host_name="host${i}"
    intf_host_side="${host_name}-${switch_name}"
    intf_switch_side="${switch_name}-${host_name}"

    ip link add ${intf_host_side} type veth peer name ${intf_switch_side}
    ip link set ${intf_host_side} netns ${cont_ids[${i}]}
    ip link set ${intf_switch_side} netns ${cont_ids[0]}

    padded=$i
    if [[ $i =~ $single_dig_re ]]
    then
	padded="0${i}"
    fi
    ip -n ${cont_ids[${i}]} addr add 192.168.0.${i} dev ${intf_host_side}
    ip -n ${cont_ids[0]} addr add 192.168.0.1${padded} dev ${intf_switch_side}

    ip -n ${cont_ids[${i}]} link set ${intf_host_side} up
    ip -n ${cont_ids[0]} link set ${intf_switch_side} up

    ip netns exec ${cont_ids[${i}]} ip route add default dev ${intf_host_side}
done

echo "Created $((num_conts+1)) containers. Access them by running:"
echo -e "sudo docker exec -it {cont_name} bash\n"
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
