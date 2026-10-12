CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
.PHONY: all test clean
all: build/server
build/server: src/server.c
	mkdir -p build
	$(CC) $(CFLAGS) -pthread $< -o $@
test: build/server
	python3 tests/integration.py
clean:
	$(RM) build/server
