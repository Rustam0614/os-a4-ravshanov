CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
LDLIBS  = -lm

all: ilon

ilon: main.c
	$(CC) $(CFLAGS) main.c -o ilon $(LDLIBS)

run: ilon
	./ilon

clean:
	rm -f ilon

.PHONY: all run clean
