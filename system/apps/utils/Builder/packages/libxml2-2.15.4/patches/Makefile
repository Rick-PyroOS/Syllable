# libxml2 for Syllable: the library os::XML (libsyllable) is built on.
# Replaces configure: it builds one static library of PIC objects (so it
# links into libsyllable.so, which keeps its symbols private with
# --exclude-libs) with the features os::XML uses, set in config.h and
# include/libxml/xmlversion.h (both from the port): the parser, output,
# XPath, DTD validation, XML Schemas and Relax-NG; no threads (the tree's
# libpthread is built after the ports; os::XML serializes its calls), iconv,
# zlib, HTML, catalogs, HTTP or modules.  Built as C99 (GCC 4.1.2 defaults to
# GNU89; libxml2 2.15 needs C99's inline and <stdint.h>).
#
#   make CC=i586-pc-syllable-gcc AR=i586-pc-syllable-ar RANLIB=i586-pc-syllable-ranlib
#   make install prefix=/path/to/sysroot/usr
#
# The sources are in this folder (GNOME's release tarball), or in the one
# below gnome/ (the Go module proxy's zip of the tag, which CMake unpacks to
# gnome/libxml2@v2.15.4+incompatible/); config.h and include/libxml/
# xmlversion.h (the port's) are in this one.
#
# Installs $(prefix)/libxml2-2.15/lib/libxml2.a and its headers in
# $(prefix)/libxml2-2.15/include/libxml2/libxml/, apart from Webster's static
# libxml2 2.6.29 in $(prefix)/include/libxml2 and $(prefix)/lib.

CC     = gcc
AR     = ar
RANLIB = ranlib
prefix = /usr/local

VERSION = 2.15.4
DIR     = libxml2-2.15

SRC := $(firstword $(wildcard gnome/libxml2*/) ./)
vpath %.c $(SRC)

CFLAGS = -O2
ALL_CFLAGS = -std=gnu99 -fPIC -DLIBXML_STATIC -I. -Iinclude -I$(SRC) -I$(SRC)include $(CFLAGS)

SOURCES = buf.c chvalid.c dict.c entities.c encoding.c error.c globals.c \
          hash.c list.c parser.c parserInternals.c SAX2.c threads.c tree.c \
          uri.c valid.c xmlIO.c xmlmemory.c xmlstring.c \
          xmlsave.c pattern.c xmlregexp.c relaxng.c xmlschemas.c \
          xmlschemastypes.c xpath.c
OBJECTS = $(SOURCES:.c=.o)

all: libxml2.a

%.o: %.c config.h include/libxml/xmlversion.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

libxml2.a: $(OBJECTS)
	rm -f $@
	$(AR) rc $@ $(OBJECTS)
	$(RANLIB) $@

install: all
	mkdir -p $(DESTDIR)$(prefix)/$(DIR)/lib $(DESTDIR)$(prefix)/$(DIR)/include/libxml2/libxml
	cp libxml2.a $(DESTDIR)$(prefix)/$(DIR)/lib/
	cp $(SRC)include/libxml/*.h $(DESTDIR)$(prefix)/$(DIR)/include/libxml2/libxml/
	cp include/libxml/xmlversion.h $(DESTDIR)$(prefix)/$(DIR)/include/libxml2/libxml/

clean:
	rm -f $(OBJECTS) libxml2.a

.PHONY: all install clean
