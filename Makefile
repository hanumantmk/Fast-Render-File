TARGETS = render stat make_frf
LIBS = frf.o frf_maker.o frf_transform.o frf_malloc.o frf_transform.tab.o lex.yy.o
LIBS_HEADERS = frf.h frf_maker.h frf_malloc.h frf_transform.h
LFLAGS = -ljansson -lpcre -ldl
#CFLAGS += -O3
CFLAGS += -O0 -Wall -ggdb

default: $(TARGETS) frf_transform_base.so libfrf.a
	cd FRF; perl Makefile.PL; make

%.o: %.c Makefile $(LIBS_HEADERS)
	$(CC) $(CFLAGS) -c $<

$(TARGETS): % : %.o libfrf.a
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@

libfrf.a: $(LIBS)
	ar rcs $@ $^

test.frf: data/content.json data/p13n.txt make_frf frf_transform_base.so
	./make_frf data/content.json test.frf data/p13n.txt

valgrind_write: test.frf make_frf
	valgrind --leak-check=yes ./make_frf data/content.json test.frf data/p13n.txt

valgrind_read: test.frf render
	valgrind --leak-check=yes ./render test.frf > /dev/null

test: frf_transform_base.so render stat test.frf
	time ./render test.frf > /dev/null
	./render test.frf seek `./render test.frf get_offset 10` | grep -c oxto0@0x2fp.net
	./stat test.frf

clean:
	rm -f test.frf $(TARGETS) *.so *.o libfrf.a *.tab.c *.tab.h lex.yy.c
	cd FRF; make clean; rm -f Makefile.old

frf_transform.tab.c frf_transform.tab.h: frf_transform.y frf_transform.h
	bison -d frf_transform.y

lex.yy.c: frf_transform.l frf_transform.tab.h frf_transform.h
	flex frf_transform.l

frf_transform_base.so: frf_transform_base.o frf_malloc.o
	$(CC) $(CFLAGS) -shared frf_malloc.o frf_transform_base.c -o frf_transform_base.so
