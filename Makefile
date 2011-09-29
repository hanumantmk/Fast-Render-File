CC=gcc
CFLAGS= -O3
#CFLAGS= -Wall -ggdb

render.c: Makefile frf.h

render: render.c frf.o

test.frf:
	./build_frf.pl --content content.txt --p13n p13n.txt > test.frf

clean:
	rm -f test.frf render test_output.txt *.o

test: render test.frf
	time ./render test.frf > test_output.txt
