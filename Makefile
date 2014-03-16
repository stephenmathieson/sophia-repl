
CC ?= gcc
VALGRIND ?= valgrind
SRC = $(wildcard src/*.c)
DEPS = $(wildcard deps/*/*.c)
OBJS = $(SRC:.c=.o) $(DEPS:.c=.o)
CFLAGS = -Wall -Wextra -Ideps -Isrc
VALGRIND_OPTS ?= --leak-check=full
LDFLAGS = -lsophia

repl: $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	rm -f example repl $(OBJS)

.PHONY: clean
