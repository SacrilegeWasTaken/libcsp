CC     = cc
CFLAGS11 = -std=c11 -Wall -Wextra -pthread -Iinclude
CFLAGS23 = -std=c23 -Wall -Wextra -pthread -Iinclude

build-test: test/test.c include/csp.h
	$(CC) $(CFLAGS11) test/test.c -o test/test11
	$(CC) $(CFLAGS23) test/test.c -o test/test23

test: build-test
	./test/test11
	./test/test23

.PHONY: build-test test
