
BIN ?= sophia
PREFIX ?= /usr/local
CC ?= gcc
VALGRIND ?= valgrind
SRC = repl.c
DEPS = $(wildcard deps/*/*.c)
OBJS = $(SRC:.c=.o) $(DEPS:.c=.o)
CFLAGS = -Wall -Wextra -Ideps
VALGRIND_OPTS ?= --leak-check=full
LDFLAGS = -lsophia

$(BIN): $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

install: $(BIN)
	mkdir -p $(PREFIX)/bin
	cp -f $(BIN) $(PREFIX)/bin/

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clean:
	rm -f example $(BIN) $(OBJS)

.PHONY: clean install uninstall
