mes_pkt.o: mes_pkt.c
	clang -O2 -Wall -target bpf -I../include -c mes_pkt.c -o mes_pkt.o

mes_show: mes_show.c
	gcc -O2 -Wall -I../include -o mes_show mes_show.c
