CC?=gcc
CFLAGS?=-O2 -Wall -Wextra -std=c99
DESTDIR?=
PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin

all: mzip

mzip: main.c mzip.c mzip.h config.h
	$(CC) $(CFLAGS) -o mzip main.c mzip.c

mall:
	meson build && ninja -C build
	# Ensure a convenient top-level binary exists for quick invocation
	cp -f build/mzip ./mzip

install: mzip
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -f mzip $(DESTDIR)$(BINDIR)/mzip

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mzip

test:
	meson build && ninja -C build
	cp -f build/mzip ./mzip
	$(MAKE) -C test
	rm -rf build

test2:
	meson build -Db_sanitize=address && ninja -C build
	cp -f build/mzip ./mzip
	CFLAGS="-fsanitize=address $(CFLAGS)" $(MAKE) -C test
	rm -rf build

clean:
	rm -rf build mzip

.PHONY: all mall clean install uninstall test test2
