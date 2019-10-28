bpf_eventd: bpf_eventd.c
	gcc -O2 -Wall -I../include -o bpf_eventd bpf_eventd.c

test_prog.o: test_prog.c
	clang -O2 -Wall -target bpf -I../include -c test_prog.c -o test_prog.o

cs_eventd: cs_eventd.c
	gcc -O2 -Wall -I../include -o cs_eventd cs_eventd.c
