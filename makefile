CC     = cc
CFLAGS11 = -std=c11 -Wall -Wextra -pthread -Iinclude
CFLAGS23 = -std=c23 -Wall -Wextra -pthread -Iinclude

build-test: test/test.c include/csp.h
	$(CC) $(CFLAGS11) test/test.c -o test/test11
	$(CC) $(CFLAGS23) test/test.c -o test/test23

build-test-asan: test/test.c include/csp.h
	$(CC) $(CFLAGS11) -fsanitize=address,undefined -g test/test.c -o test/test_asan

test: build-test
	./test/test11
	./test/test23

test-asan: build-test-asan
	./test/test_asan

.PHONY: build-test test build-test-asan test-asan
