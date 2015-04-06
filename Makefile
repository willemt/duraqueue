GCOV_OUTPUT = *.gcda *.gcno *.gcov 
GCOV_CCFLAGS = -fprofile-arcs -ftest-coverage
CC     = gcc
CCFLAGS = -I. -Iinclude -Ideps/arrayqueue -Itests -g -Werror -fno-omit-frame-pointer -Wimplicit-function-declaration -fno-common -fsigned-char $(GCOV_CCFLAGS)


all: test

main.c:
	sh tests/make-tests.sh tests/test_*.c > main.c

test: main.c src/duraqueue.c deps/arrayqueue/arrayqueue.c tests/test_duraqueue.c tests/CuTest.c
	$(CC) $(CCFLAGS) -o $@ $^
	./test
	gcov duraqueue.c

dq: src/cli.c src/duraqueue.c deps/arrayqueue/arrayqueue.c
	$(CC) $(CCFLAGS) -o $@ $^

clean:
	rm -f main.c duraqueue.o dq $(GCOV_OUTPUT)
