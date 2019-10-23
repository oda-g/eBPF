load_kp_bpf: load_kp_bpf.c
	gcc -O2 -Wall -I../include -o load_kp_bpf load_kp_bpf.c -lelf

test_prog.o: test_prog.c
	clang -O2 -Wall -target bpf -I../include -c test_prog.c -o test_prog.o
