CC=gcc
CFLAGS= -O3
#CFLAGS= -Wall -ggdb

render.c: Makefile

render: render.c

out.frf:
	./build_frf.pl --content content.txt --p13n p13n.txt --output test

clean:
	rm -f test.frf render test_output.txt

test: render out.frf
	time ./render test.frf > test_output.txt
