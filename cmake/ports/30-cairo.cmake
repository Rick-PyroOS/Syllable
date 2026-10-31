# Libraries for the cairo appserver (next-steps item 8): installed, not used yet
# (the appserver and libsyllable stay as they are).
#
# Shared libraries in /usr/<package>/<version>, linked from /usr/index/{lib,
# include,lib/pkgconfig,bin}, with pkg-config files and freetype-config /
# xml2-config, so `-lcairo -lfontconfig $(freetype-config --libs)` resolve on
# Syllable.  The release's versions, which fit together: GLib 2.14 bundles
# PCRE and needs no libffi (that came with 2.29); cairo 1.4 bundles pixman.
# Order: libpng and FreeType (shared, too), Expat, fontconfig, gettext, GLib,
# cairo, Pango; libxml2 2.7.8 on its own.
#
# The ones later ports compile against are SYSROOT: their headers, libraries
# and (fixed) .pc files go into the staging sysroot.  libxml2 2.7.8 isn't:
# Webster compiles against the staging sysroot's static libxml2 2.6.29.

# All built with GCC 9.5.0 (COMPILER), its libgcc linked in: their users
# share only the C ABI.  -fgnu89-inline: these releases' "extern inline"
# (GLib's G_INLINE_FUNC, gnulib) means GNU89's, not C99's (GCC 5's default).
set(_cairo_cflags "CFLAGS=-O2 -fgnu89-inline")

set(_lib_links @lib:/usr/index/lib @lib/pkgconfig:/usr/index/lib/pkgconfig @include:/usr/index/include)

# The static FreeType and LibPNG the appserver and translators link stay as
# they are (Ports.cmake); these add the shared libraries next to them
syllable_port(freetype_shared
  PORT FreeType-2.3.9
  URL https://download.savannah.gnu.org/releases/freetype/freetype-old/freetype-2.3.9.tar.bz2
  SHA256 51a033bce9904d73e0821e8e2bba24bd319619d7d1b2e9eeccac611580242ab6
  PREFIX /usr/freetype/2.3.9
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags}
  SHARED SYSROOT DEPENDS zlib freetype
  LINKS @bin:/usr/index/bin ${_lib_links})

syllable_port(libpng_shared
  PORT LibPNG-1.2.37
  URL https://download.sourceforge.net/libpng/libpng-1.2.37.tar.bz2
  SHA256 682960b55527b54bada90e959c2d42679444a1db43677c77eb645a29645f86d1
  PREFIX /usr/libpng/1.2.37
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags}
  SHARED SYSROOT DEPENDS zlib libpng
  LINKS @bin:/usr/index/bin ${_lib_links})

syllable_port(expat
  PORT expat-2.0.0
  URL https://downloads.sourceforge.net/project/expat/expat/2.0.0/expat-2.0.0.tar.gz
  SHA256 c4280eb80fa8e14b3f204b1d0ec67af66fa04c4f9dae9c78a1a8ee4c6f485ddd
  PREFIX /usr/expat/2.0.0
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags}
  SHARED SYSROOT
  LINKS @bin:/usr/index/bin ${_lib_links})

# Its build makes tables with programs for the build host (CC_FOR_BUILD), and
# can't detect the architecture when cross compiling.  The font cache is made
# on Syllable (fc-cache), not here.
syllable_port(fontconfig
  PORT fontconfig-2.5.0
  URL https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.5.0.tar.gz
  SHA256 34c6badb4cb7eb496779d30616903de400fa244838ea39c2a7f63eaa5fdf0efe
  PREFIX /usr/fontconfig/2.5.0
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags} --with-arch=x86 CC_FOR_BUILD=gcc
            --with-default-fonts=/system/fonts --with-cache-dir=/var/cache/fontconfig
            --with-expat-includes=${SYLLABLE_STAGING}/include --with-expat-lib=${SYLLABLE_STAGING}/lib
            --with-freetype-config=${SYLLABLE_STAGING}/bin/freetype-config
            --disable-docs
  SHARED SYSROOT DEPENDS freetype_shared expat
  LINKS @bin:/usr/index/bin ${_lib_links})

# glibc has gettext (libintl) built in, so this is gettext-runtime's programs
# (gettext, ngettext, envsubst); the translation tools aren't needed on Syllable
syllable_port(gettext
  PORT gettext-0.17
  URL ${_gnu}/gettext/gettext-0.17.tar.gz ${_gnu2}/gettext/gettext-0.17.tar.gz
  SHA256 209638bb8e162f22c281145a34e220c66f1f6e9ff5e4c50c6f2ef2ded59537ba
  PREFIX /usr/gettext/0.17
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags} --disable-java --disable-csharp --disable-openmp --without-emacs
            --disable-libasprintf --without-git --without-cvs
  # Only gettext-runtime is built and installed (-C: a SUBDIRS= override
  # would reach every sub-make).  The port's patch: gnulib's wchar.h vs glibc 2.7.
  MAKE_ARGS -C gettext-runtime
  SHARED
  LINKS @bin:/usr/index/bin)

# No threads (GLib 2.14 runs without; Syllable's libpthread is its own), and
# glibc's iconv.  --disable-rebuilds: the generated sources the tarball ships
# (gmarshal.[ch]...) are used as they are, not made again with the build
# host's glib-genmarshal (glib.cache)
syllable_port(glib
  PORT glib-2.14.4
  URL https://download.gnome.org/sources/glib/2.14/glib-2.14.4.tar.bz2
  SHA256 10e6ebecc2cbd07f193a5d26b88c3bf2107e32b2a4d024f10f77f59a98d579ff
  PREFIX /usr/glib/2.14.4
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags} --with-threads=none --with-libiconv=no --disable-gtk-doc --disable-fam
            --disable-rebuilds
  CACHE ${CMAKE_CURRENT_LIST_DIR}/glib.cache
  SHARED SYSROOT DEPENDS gettext
  LINKS @bin:/usr/index/bin ${_lib_links})

# The image surface with FreeType/fontconfig fonts, and PNG, PostScript, PDF
# and SVG output.  No X, no glitz.
syllable_port(cairo
  PORT cairo-1.4.14
  URL https://www.cairographics.org/releases/cairo-1.4.14.tar.gz
  SHA256 8914f9c34a77bf9d908b0c617c2d7ffebdd6d1e855cfd367d2a1b3d677f22e3c
  PREFIX /usr/cairo/1.4.14
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags} --disable-xlib --disable-xcb --disable-quartz --disable-glitz --disable-directfb
            --disable-beos --disable-win32 --enable-ft --enable-png --enable-ps --enable-pdf
            --enable-svg --disable-gtk-doc
  SHARED SYSROOT DEPENDS libpng_shared fontconfig
  LINKS ${_lib_links})

# pangocairo and pangoft2, with the shaping modules built in (no module
# loading through GModule)
syllable_port(pango
  PORT Pango--current
  URL https://download.gnome.org/sources/pango/1.20/pango-1.20.2.tar.gz
  SHA256 437129e6b7c809092d20bd12a425decbb1b826c6084cdb88f5c76d050c7e945e
  PREFIX /usr/pango/1.20.2
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags} --without-x --with-included-modules=yes --with-dynamic-modules=no
            --disable-gtk-doc --disable-man
  SHARED SYSROOT DEPENDS glib cairo
  LINKS @bin:/usr/index/bin ${_lib_links})

# The release's system libxml2, shared (Webster keeps its static 2.6.29)
syllable_port(libxml2_shared
  PORT LibraryXML2-2.7.8
  URL http://xmlsoft.org/sources/libxml2-2.7.8.tar.gz
  SHA256 cda23bc9ebd26474ca8f3d67e7d1c4a1f1e7106364b690d822e009fdc3c417ec
  PREFIX /usr/libxml2/2.7.8
  COMPILER 9.5.0
  CONFIGURE ${_cairo_cflags} --without-python --without-threads --with-zlib=${SYLLABLE_STAGING}
  SHARED DEPENDS zlib
  LINKS @bin:/usr/index/bin ${_lib_links})
