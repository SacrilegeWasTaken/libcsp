CC     = cc
CFLAGS = -std=c11 -Wall -Wextra -pthread -Iinclude

test: test/test.c include/csp.h
	$(CC) $(CFLAGS) test/test.c -o test/test

run-test: test
	./test/test

.PHONY: test run-test
