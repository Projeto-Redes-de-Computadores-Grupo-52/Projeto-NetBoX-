CC     = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Isrc
SRCS   = $(shell find src -name '*.c')

user: $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

debug: CFLAGS += -g -O0 -fsanitize=address,undefined
debug: clean user

clean:
	rm -f user