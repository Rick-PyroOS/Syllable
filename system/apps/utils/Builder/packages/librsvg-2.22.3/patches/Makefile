# librsvg 2.22.3 for Syllable: the library only (librsvg-2.so), without
# GTK+'s gdk-pixbuf (gdk-pixbuf.[ch] here stand in for it), so without
# autoconf: configure insists on gdk-pixbuf-2.0.  Built by
# cmake/ports/30-cairo.cmake (STYLE plain), which passes CC, PKG_CONFIG (the
# staging sysroot's), XML_CFLAGS/XML_LIBS, prefix and PCPREFIX.
#
#   make; make install prefix=DIR [PCPREFIX=/usr/librsvg/2.22.3]

prefix   ?= /usr/librsvg/2.22.3
PCPREFIX ?= $(prefix)
PKG_CONFIG ?= pkg-config
CC       ?= gcc

VERSION  = 2.22.3
LIBNAME  = librsvg-2.so
SONAME   = $(LIBNAME).2

PKGS       = glib-2.0 gobject-2.0 pangocairo pangoft2 cairo cairo-png fontconfig freetype2
XML_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags libxml-2.0)
XML_LIBS   ?= $(shell $(PKG_CONFIG) --libs libxml-2.0)

CFLAGS   ?= -O2
# -fgnu89-inline: GLib 2.14's G_INLINE_FUNC is GNU89's extern inline
ALL_CFLAGS = $(CFLAGS) -fgnu89-inline -fPIC -DHAVE_CONFIG_H -DG_LOG_DOMAIN=\"librsvg\" \
             -I. $(shell $(PKG_CONFIG) --cflags $(PKGS)) $(XML_CFLAGS) $(CPPFLAGS)
LIBS       = $(shell $(PKG_CONFIG) --libs $(PKGS)) $(XML_LIBS) -lm

# librsvg_2_la_SOURCES of Makefile.am, and the GdkPixbuf stand-in
SRCS = rsvg-affine.c librsvg-features.c rsvg-bpath-util.c rsvg-css.c rsvg-defs.c \
       rsvg-image.c rsvg-paint-server.c rsvg-path.c rsvg-base-file-util.c rsvg-filter.c \
       rsvg-marker.c rsvg-mask.c rsvg-shapes.c rsvg-structure.c rsvg-styles.c rsvg-text.c \
       rsvg-cond.c rsvg-base.c librsvg-enum-types.c rsvg-cairo-draw.c rsvg-cairo-render.c \
       rsvg-cairo-clip.c rsvg.c rsvg-gobject.c rsvg-file-util.c gdk-pixbuf.c

HEADERS = rsvg.h rsvg-cairo.h librsvg-features.h librsvg-enum-types.h gdk-pixbuf.h

# (not objs/: the source archive carries a 2008 build there)
OBJDIR = build-syllable
OBJS   = $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))

all: $(OBJDIR)/$(SONAME) $(OBJDIR)/librsvg-2.0.pc

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(OBJDIR)/$(SONAME): $(OBJS)
	$(CC) -shared -Wl,-soname=$(SONAME) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(OBJDIR)/librsvg-2.0.pc: | $(OBJDIR)
	printf '%s\n' 'prefix=$(PCPREFIX)' 'exec_prefix=$${prefix}' 'libdir=$${prefix}/lib' \
	  'includedir=$${prefix}/include' '' 'Name: librsvg' \
	  'Description: library that renders svg files' 'Version: $(VERSION)' \
	  'Requires: glib-2.0 gobject-2.0 cairo' 'Requires.private: pangocairo pangoft2' \
	  'Libs: -L$${libdir} -lrsvg-2' 'Cflags: -I$${includedir}/librsvg-2' > $@

install: all
	mkdir -p $(prefix)/lib/pkgconfig $(prefix)/include/librsvg-2/librsvg
	cp $(OBJDIR)/$(SONAME) $(prefix)/lib/
	ln -sf $(SONAME) $(prefix)/lib/$(LIBNAME)
	cp $(OBJDIR)/librsvg-2.0.pc $(prefix)/lib/pkgconfig/
	cp $(HEADERS) $(prefix)/include/librsvg-2/librsvg/

clean:
	rm -f $(OBJDIR)/*.o $(OBJDIR)/$(SONAME) $(OBJDIR)/librsvg-2.0.pc

.PHONY: all install clean
