# Third-party ports on the boot volume (see SyllablePorts.cmake)

syllable_port(bash
  PORT BASh-4.0
  URL https://ftp.gnu.org/gnu/bash/bash-4.0.tar.gz
  SHA256 9793d394f640a95030c77d5ac989724afe196921956db741bcaf141801c50518
  PREFIX /system/resources/BASh/4.0
  CONFIGURE --without-bash-malloc
  CACHE ${PROJECT_SOURCE_DIR}/cmake/ports/bash.cache
  LINKS bin/bash:/bin/bash bin/bash:/bin/sh)

# Only lib and src: the man pages are made by running the built programs,
# which are Syllable programs
syllable_port(coreutils
  PORT coreutils-6.4
  URL https://ftp.gnu.org/gnu/coreutils/coreutils-6.4.tar.bz2
  SHA256 75a6b82bb7e3d07bec5b9bd0fcdfdfdfb88f32728e5089972001b50e4148b87b
  PREFIX /system/resources/coreutils/6.4
  MAKE_ARGS "SUBDIRS=lib src"
  LINKS @bin:/bin)

# ---- Libraries for the appserver, translators and media ------------------------

syllable_port(zlib
  PORT zlib-1.2.3
  URL https://zlib.net/fossils/zlib-1.2.3.tar.gz
  SHA256 1795c7d067a43174113fdf03447532f373e1c6c57c08d61d9e4e9be5e244b05e
  PREFIX /system/resources/zlib/1.2.3
  STYLE zlib CONFIGURE --shared SYSROOT BYPRODUCTS ${SYLLABLE_STAGING}/lib/libz.so
  LINKS lib/libz.so.1:/system/libraries/libz.so.1)

# FreeType and LibPNG are linked in statically, as in the release (Builder's
# "stage" packages): the staging sysroot only, not the boot volume
syllable_port(freetype
  PORT FreeType-2.3.9
  URL https://download.savannah.gnu.org/releases/freetype/freetype-old/freetype-2.3.9.tar.bz2
  SHA256 51a033bce9904d73e0821e8e2bba24bd319619d7d1b2e9eeccac611580242ab6
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libfreetype.a)
syllable_port(libpng
  PORT LibPNG-1.2.37
  URL https://download.sourceforge.net/libpng/libpng-1.2.37.tar.bz2
  SHA256 682960b55527b54bada90e959c2d42679444a1db43677c77eb645a29645f86d1
  DEST sysroot DEPENDS zlib BYPRODUCTS ${SYLLABLE_STAGING}/lib/libpng.a)

# Targets to link against
add_library(syllable_z INTERFACE)
target_link_libraries(syllable_z INTERFACE ${SYLLABLE_STAGING}/lib/libz.so)
add_dependencies(syllable_z zlib)
add_library(syllable::z ALIAS syllable_z)

add_library(syllable_freetype INTERFACE)
target_include_directories(syllable_freetype SYSTEM INTERFACE
  ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include)
target_link_libraries(syllable_freetype INTERFACE ${SYLLABLE_STAGING}/lib/libfreetype.a syllable_z)
add_dependencies(syllable_freetype freetype)
add_library(syllable::freetype ALIAS syllable_freetype)

add_library(syllable_png INTERFACE)
target_include_directories(syllable_png SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
target_link_libraries(syllable_png INTERFACE ${SYLLABLE_STAGING}/lib/libpng.a syllable_z)
add_dependencies(syllable_png libpng)
add_library(syllable::png ALIAS syllable_png)
