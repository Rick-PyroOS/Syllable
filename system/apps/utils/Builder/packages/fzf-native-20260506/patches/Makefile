# fzf-native for Syllable: the library os::FuzzyString (libsyllable) is
# built on.  Replaces the project's Makefile, which builds libfzf.so for
# Telescope's Lua FFI.  This builds one static library of PIC objects (so it
# links into libsyllable.so, which keeps its symbols private with
# --exclude-libs).  Built as C99 (GCC 4.1.2 defaults to GNU89; fzf.c
# declares variables in for loops and uses compound literals).
#
#   make CC=i586-pc-syllable-gcc AR=i586-pc-syllable-ar RANLIB=i586-pc-syllable-ranlib
#   make install prefix=/path/to/sysroot
#
# The sources are below nvim-telescope/ (the Go module proxy's zip of the
# commit, which CMake unpacks to nvim-telescope/telescope-fzf-native.nvim@v.../),
# or in src/ of this folder (a checkout of the repository).
#
# Installs $(prefix)/lib/libfzf.a and $(prefix)/include/fzf/fzf.h
# (#include <fzf/fzf.h>).

CC     = gcc
AR     = ar
RANLIB = ranlib
prefix = /usr/local

SRC := $(firstword $(wildcard nvim-telescope/telescope-fzf-native.nvim*/) ./)
vpath %.c $(SRC)src

CFLAGS = -O2
ALL_CFLAGS = -std=gnu99 -fPIC -Wall -I$(SRC)src $(CFLAGS)

all: libfzf.a

fzf.o: fzf.c $(SRC)src/fzf.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

libfzf.a: fzf.o
	rm -f $@
	$(AR) rc $@ fzf.o
	$(RANLIB) $@

install: all
	mkdir -p $(DESTDIR)$(prefix)/lib $(DESTDIR)$(prefix)/include/fzf
	cp libfzf.a $(DESTDIR)$(prefix)/lib/
	cp $(SRC)src/fzf.h $(DESTDIR)$(prefix)/include/fzf/

clean:
	rm -f fzf.o libfzf.a

.PHONY: all install clean
