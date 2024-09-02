CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Werror
LDFLAGS=-lexpat

PREFIX?=/usr/local
DESTDIR?=
BINDIR=$(DESTDIR)$(PREFIX)/bin/

SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)

PROG=sxmlv

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

install:
	install -Dm775 $(PROG) $(BINDIR)$(PROG)

uninstall:
	rm $(BINDIR)$(PROG)

clean:
	rm -rf $(OBJ) $(PROG) **/*.o

.PHONY: all clean install
