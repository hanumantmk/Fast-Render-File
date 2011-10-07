TARGETS = render stat
LIBS = frf.o
LIBS_HEADERS = frf.h
#CFLAGS= -O3
CFLAGS += -Wall -ggdb

default: $(TARGETS) libfrf.a
	cd FRF; perl Makefile.PL; make

%.o: %.c Makefile $(LIBS_HEADERS)
	$(CC) $(CFLAGS) -c $<

$(TARGETS): % : %.o libfrf.a
	$(CC) $(CFLAGS) $^ -o $@

libfrf.a: $(LIBS)
	ar rcs $@ $^

test.frf: data/content.txt data/p13n.txt bin/build_frf.pl bin/run_with_env
	bin/run_with_env bin/build_frf.pl --content data/content.txt --p13n data/p13n.txt --output test.frf

test: render stat test.frf
	time ./render test.frf > /dev/null
	./render test.frf seek `./render test.frf get_offset 10` | grep -c oxto0@0x2fp.net
	./stat test.frf

clean:
	rm -f test.frf $(TARGETS) *.o *.a
	cd FRF; make clean; rm -f Makefile.old
