sudo ip netns add ns1
sudo ip netns add ns2
sudo ip link add dev v1 type veth peer name n1
sudo ip link add dev v2 type veth peer name n2
sudo ip link add dev br1 type bridge
sudo ip link set dev n1 netns ns1
sudo ip link set dev n2 netns ns2
sudo ip link set dev v1 up
sudo ip link set dev v2 up
sudo ip link set dev v1 master br1
sudo ip link set dev v2 master br1
sudo ip link set dev br1 up
sudo ip -n ns1 addr add 192.168.10.1/24 dev n1
sudo ip -n ns2 addr add 192.168.10.2/24 dev n2
sudo ip -n ns1 link set dev n1 up
sudo ip -n ns2 link set dev n2 up

# command example
# # sudo ip netns exec ns2 nc -u -l 5001
# # sudo ip netns exec ns1 nc -u 192.168.10.2 5001
