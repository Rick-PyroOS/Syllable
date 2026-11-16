# SQLite for Syllable, from the amalgamation in the sqlite-autoconf tarball.
# The tarball's configure (autosetup) is not used: this builds the shared
# library, a static library of the same objects and the sqlite3 shell with the
# options Syllable needs, and installs them under $(prefix).
#
#   make CC=i586-pc-syllable-gcc AR=i586-pc-syllable-ar RANLIB=i586-pc-syllable-ranlib
#   make install prefix=/path/to/dest/system/resources/sqlite/3.54.1

CC     = gcc
AR     = ar
RANLIB = ranlib
prefix = /usr/local

SONAME  = libsqlite3.so.0
VERSION = 3.54.1

# For Syllable:
#  THREADSAFE=0         no libpthread at this point of the build (the tree's
#                       libpthread is built after the ports), as 3.5.5 was
#                       built (--disable-threadsafe).  Callers that use SQLite
#                       from more than one thread serialize their calls
#                       (os::SettingsCache does)
#  OMIT_LOAD_EXTENSION  dlopen() is in the tree's libdl, also built later
#  MAX_MMAP_SIZE=0      file mmap is unreliable on Syllable (posix audit P0-6)
#  DEFAULT_MEMSTATUS=0  no global memory statistics
#  DQS=0                no double-quoted string literals (SQLite's advice)
#  HAVE_USLEEP          busy waits sleep in milliseconds, not whole seconds
OPTS = -DSQLITE_THREADSAFE=0 \
       -DSQLITE_OMIT_LOAD_EXTENSION=1 \
       -DSQLITE_MAX_MMAP_SIZE=0 \
       -DSQLITE_DEFAULT_MEMSTATUS=0 \
       -DSQLITE_DQS=0 \
       -DHAVE_USLEEP=1 \
       -DSQLITE_ENABLE_FTS5 \
       -DSQLITE_ENABLE_RTREE

CFLAGS = -O2
ALL_CFLAGS = $(CFLAGS) $(OPTS)

all: $(SONAME) libsqlite3.a sqlite3

sqlite3.o: sqlite3.c sqlite3.h
	$(CC) $(ALL_CFLAGS) -fPIC -c sqlite3.c -o $@

$(SONAME): sqlite3.o
	$(CC) -shared -Wl,-soname=$(SONAME) -o $@ sqlite3.o -lm

# The same PIC object, so the static library links into shared objects too
# (libsyllable).  No libsqlite3.so link here: the build tree links the static
# library; Syllable's /usr/index/lib/libsqlite3.so is made by the port's LINKS
libsqlite3.a: sqlite3.o
	$(AR) rcs $@ sqlite3.o

# The shell links the static library, so it runs without libsqlite3.so.0
sqlite3: shell.c libsqlite3.a
	$(CC) $(ALL_CFLAGS) -o $@ shell.c libsqlite3.a -lm

install: all
	mkdir -p $(DESTDIR)$(prefix)/lib $(DESTDIR)$(prefix)/include $(DESTDIR)$(prefix)/bin
	cp $(SONAME) libsqlite3.a $(DESTDIR)$(prefix)/lib/
	cp sqlite3.h sqlite3ext.h $(DESTDIR)$(prefix)/include/
	cp sqlite3 $(DESTDIR)$(prefix)/bin/

.PHONY: all install
