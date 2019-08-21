sudo tc qdisc del dev v1 clsact
sudo tc qdisc del dev v2 clsact

sudo rm /sys/fs/bpf/tc/globals/map_mes_start
sudo rm /sys/fs/bpf/tc/globals/map_mes_end
sudo rm /sys/fs/bpf/tc/globals/map_mes_cnt
