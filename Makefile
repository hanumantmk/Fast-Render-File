TARGETS = render stat make_frf
LIBS = frf.o frf_maker.o
LIBS_HEADERS = frf.h frf_maker.h
#CFLAGS= -O3
CFLAGS += -Wall -ggdb -ljansson -lpcre

default: $(TARGETS) libfrf.a
	cd FRF; perl Makefile.PL; make

%.o: %.c Makefile $(LIBS_HEADERS)
	$(CC) $(CFLAGS) -c $<

$(TARGETS): % : %.o libfrf.a
	$(CC) $(CFLAGS) $^ -o $@

libfrf.a: $(LIBS)
	ar rcs $@ $^

test.frf: data/content.json data/p13n.txt make_frf
	./make_frf data/content.json test.frf data/p13n.txt

valgrind: test.frf make_frf
	valgrind --leak-check=yes ./make_frf data/content.json test.frf data/p13n.txt

test: render stat test.frf
	time ./render test.frf > /dev/null
	./render test.frf seek `./render test.frf get_offset 10` | grep -c oxto0@0x2fp.net
	./stat test.frf

clean:
	rm -f test.frf $(TARGETS) *.o libfrf.a
	cd FRF; make clean; rm -f Makefile.old
