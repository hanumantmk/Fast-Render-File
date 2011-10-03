CC=gcc
#CFLAGS= -O3
CFLAGS= -Wall -g

render.c: Makefile frf.h

render: render.c frf.o

stat: stat.c frf.o

test.frf: content.txt p13n.txt build_frf.pl
	./build_frf.pl --content content.txt --p13n p13n.txt > test.frf

clean:
	rm -f test.frf render stat test_output.txt *.o

test: render stat test.frf
	time ./render test.frf > /dev/null
	./render test.frf seek `./render test.frf get_offset 10` | grep -c test_number_10
	./stat test.frf
