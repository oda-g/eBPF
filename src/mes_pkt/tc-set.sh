sudo tc qdisc add dev v1 clsact
sudo tc filter add dev v1 ingress bpf da obj mes_pkt.o sec ingress
sudo tc qdisc add dev v2 clsact
sudo tc filter add dev v2 egress bpf da obj mes_pkt.o sec egress
