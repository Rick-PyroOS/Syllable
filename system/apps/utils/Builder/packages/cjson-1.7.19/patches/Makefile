# cJSON for Syllable.  Replaces the tarball's Makefile: that one adds -Werror
# with warnings GCC 4.1.2 doesn't know (-Wstrict-overflow) and
# -fstack-protector (no libssp here), and installs no static libraries.  This
# builds libcjson and libcjson_utils, shared and static (the same PIC objects,
# so the static ones link into shared objects too), and installs them with
# their headers (include/cjson/, as upstream: #include <cjson/cJSON.h>) under
# $(prefix).
#
#   make CC=i586-pc-syllable-gcc AR=i586-pc-syllable-ar RANLIB=i586-pc-syllable-ranlib
#   make install prefix=/path/to/dest/system/resources/cjson/1.7.19

CC     = gcc
AR     = ar
RANLIB = ranlib
prefix = /usr/local

VERSION      = 1.7.19
CJSON_SONAME = libcjson.so.1
UTILS_SONAME = libcjson_utils.so.1

# C89, as upstream builds it
CFLAGS = -O2
ALL_CFLAGS = -std=c89 -fPIC -Wall $(CFLAGS)

all: $(CJSON_SONAME) $(UTILS_SONAME) libcjson.a libcjson_utils.a cJSON_test

cJSON.o: cJSON.c cJSON.h
	$(CC) $(ALL_CFLAGS) -c cJSON.c -o $@

cJSON_Utils.o: cJSON_Utils.c cJSON_Utils.h cJSON.h
	$(CC) $(ALL_CFLAGS) -c cJSON_Utils.c -o $@

$(CJSON_SONAME): cJSON.o
	$(CC) -shared -Wl,-soname=$(CJSON_SONAME) -o $@ cJSON.o -lm

$(UTILS_SONAME): cJSON_Utils.o $(CJSON_SONAME)
	$(CC) -shared -Wl,-soname=$(UTILS_SONAME) -o $@ cJSON_Utils.o $(CJSON_SONAME) -lm

libcjson.a: cJSON.o
	$(AR) rcs $@ cJSON.o

libcjson_utils.a: cJSON_Utils.o
	$(AR) rcs $@ cJSON_Utils.o

# Upstream's small example program (test.c), linked statically: a quick check
# on Syllable that parsing and printing work.  Not installed.
cJSON_test: test.c libcjson.a
	$(CC) $(ALL_CFLAGS) -I. -o $@ test.c libcjson.a -lm

# No libcjson.so link here: Syllable's /usr/index/lib/libcjson.so is made by
# the port's LINKS, the build tree links the static libraries
install: all
	mkdir -p $(DESTDIR)$(prefix)/lib $(DESTDIR)$(prefix)/include/cjson
	cp $(CJSON_SONAME) $(UTILS_SONAME) libcjson.a libcjson_utils.a $(DESTDIR)$(prefix)/lib/
	cp cJSON.h cJSON_Utils.h $(DESTDIR)$(prefix)/include/cjson/

.PHONY: all install
