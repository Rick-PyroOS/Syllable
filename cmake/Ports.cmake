# Third-party ports on the boot volume (see SyllablePorts.cmake)

syllable_port(bash
  PORT BASh-4.0
  URL https://ftp.gnu.org/gnu/bash/bash-4.0.tar.gz
  SHA256 9793d394f640a95030c77d5ac989724afe196921956db741bcaf141801c50518
  PREFIX /system/resources/BASh/4.0
  CONFIGURE --without-bash-malloc
  CACHE ${PROJECT_SOURCE_DIR}/cmake/ports/bash.cache
  MAKE_ARGS -j1                        # its readline Makefile races in parallel
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

# ---- Webster's libraries: static, build-time only --------------------------------

syllable_port(openssl
  PORT openssl-0.9.8e
  URL https://www.openssl.org/source/old/0.9.x/openssl-0.9.8zh.tar.gz
  SHA256 f1d9f3ed1b85a82ecf80d0e2d389e1fda3fca9a4dba0bf07adbf231e1a5e2fd6
  STYLE openssl DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libssl.a ${SYLLABLE_STAGING}/lib/libcrypto.a)

# cURL (Transferrer, Webster).  The release has it as a shared system package;
# its libtool does not know Syllable, so it is static, build-time only here.
syllable_port(curl
  PORT cURL-7.18.2
  URL https://curl.se/download/archeology/curl-7.18.2.tar.gz
  SHA256 55a711ad1d703f8b9c3ca80e5b859b5222f64566f9a3b82ac6773055ef8cee71
  CONFIGURE --with-ssl=${SYLLABLE_STAGING} --without-libssh2 --disable-ldap --disable-manual
            --with-zlib=${SYLLABLE_STAGING} --without-ca-bundle
            ac_cv_file___dev_urandom_=yes   # a cross build cannot check; Syllable has it
            LIBS=-ldl                       # static libcrypto needs dlopen()
  DEST sysroot DEPENDS zlib openssl BYPRODUCTS ${SYLLABLE_STAGING}/lib/libcurl.a)   # and libdl (CMakeLists.txt)

syllable_port(libxml2
  PORT libxml2-2.6.29
  URL http://xmlsoft.org/sources/old/libxml2-2.6.29.tar.gz
  SHA256 e34a19e0b9d7ac03874acffd624f181c85a98fb976dc84411d6cd1bdda965992
  CONFIGURE --without-threads --without-python --with-zlib=${SYLLABLE_STAGING}
  DEST sysroot DEPENDS zlib BYPRODUCTS ${SYLLABLE_STAGING}/lib/libxml2.a)

syllable_port(sqlite
  PORT sqlite-3.5.5
  URL https://www.sqlite.org/sqlite-3.5.5.tar.gz
  SHA256 c430ec6dca870d7c4af35b07cbdf90a20b8a857cc90c7c93c31c6896a8b5270d
  CONFIGURE --disable-threadsafe --disable-tcl
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libsqlite3.a)

syllable_port(icu
  PORT icu-3.6
  URL https://sourceforge.net/projects/icu/files/ICU4C/3.6/icu4c-3_6-src.tgz/download
  SHA256 5135e8d69d6206d320515df7aeee7027711ab1aef9d8dbf29571a97a9746b041
  STYLE icu CONFIGURE --disable-threads --disable-64bit-libs --disable-extras
                      --disable-samples --disable-tests
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libsicuuc.a ${SYLLABLE_STAGING}/lib/libsicudata.a)

# Image libraries for the translators, static like FreeType and LibPNG
syllable_port(libjpeg
  PORT LibJPEG-8a
  URL https://www.ijg.org/files/jpegsrc.v8a.tar.gz
  SHA256 bf86a5eaafa79b5d60feb27bee08df2db4c9ca2fb40d453e7a2080f7663a713b
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libjpeg.a)
syllable_port(giflib
  PORT GIFLib-4.1.6
  URL https://downloads.sourceforge.net/project/giflib/giflib-4.x/giflib-4.1.6/giflib-4.1.6.tar.bz2
  SHA256 e1c1ced9c5bc8f93ef0faf0a8c7717abf784d10a7b270d2285e8e1f3b93f2bed
  DEST sysroot BYPRODUCTS ${SYLLABLE_STAGING}/lib/libgif.a)
syllable_port(libtiff
  PORT libtiff-3.8.2
  URL https://download.osgeo.org/libtiff/old/tiff-3.8.2.tar.gz
  SHA256 be88f037080b93ce0a337b2b3ca5e8195f76885deb6c558cc3aa872ee848fc76
  CONFIGURE --without-x --disable-cxx
  MAKE_ARGS "SUBDIRS=port libtiff"
  DEST sysroot DEPENDS zlib libjpeg BYPRODUCTS ${SYLLABLE_STAGING}/lib/libtiff.a)

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

foreach(lib jpeg gif tiff)
  add_library(syllable_${lib} INTERFACE)
  target_include_directories(syllable_${lib} SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
  target_link_libraries(syllable_${lib} INTERFACE ${SYLLABLE_STAGING}/lib/lib${lib}.a)
  add_library(syllable::${lib} ALIAS syllable_${lib})
endforeach()
add_dependencies(syllable_jpeg libjpeg)
add_dependencies(syllable_gif giflib)
add_dependencies(syllable_tiff libtiff)
target_link_libraries(syllable_tiff INTERFACE syllable_jpeg syllable_z)

add_library(syllable_curl INTERFACE)
target_include_directories(syllable_curl SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
# librt: clock_gettime (glibc 2.7)
target_link_libraries(syllable_curl INTERFACE ${SYLLABLE_STAGING}/lib/libcurl.a syllable_z rt)
add_dependencies(syllable_curl curl)
add_library(syllable::curl ALIAS syllable_curl)

foreach(lib ssl crypto xml2 sqlite3)
  add_library(syllable_${lib} INTERFACE)
  target_include_directories(syllable_${lib} SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
  target_link_libraries(syllable_${lib} INTERFACE ${SYLLABLE_STAGING}/lib/lib${lib}.a)
  add_library(syllable::${lib} ALIAS syllable_${lib})
endforeach()
add_dependencies(syllable_ssl openssl)
add_dependencies(syllable_crypto openssl)
add_dependencies(syllable_xml2 libxml2)
add_dependencies(syllable_sqlite3 sqlite)
target_link_libraries(syllable_curl INTERFACE syllable_ssl syllable_crypto libdl)

# ---- Development ------------------------------------------------------------------

# gdbserver (gdb 18.1 with the Syllable target, Builder/packages/gdbserver-18.1):
# lets another machine's gdb debug programs here.  It needs C++17, so it is
# built with GCC 9.5.0 whichever compiler builds the system, and links its C++
# runtime statically (the system's is GCC 4.1.2's).  It only runs on a debug
# image (./sb image --debug: system/index/tasks/start/gdbserver).
set(_gcc95 ${SYLLABLE_TOOLCHAIN_ROOT}/bin/i586-pc-syllable)
syllable_port(gdbserver
  PORT gdbserver-18.1
  URL https://ftp.gnu.org/gnu/gdb/gdb-18.1.tar.xz
  SHA256 cd9fc3fe2b47743840e42c1592d3d87f8302eb18639c0b8b4ba0898002e2348f
  PREFIX /system/resources/gdbserver/18.1
  CONFIGURE --disable-gdb --disable-sim --disable-binutils --disable-ld --disable-gas
            --disable-gprof --disable-gold --disable-libctf --disable-readline
            --without-zstd --without-xxhash --enable-gdbserver
            CC=${_gcc95}-gcc CXX=${_gcc95}-g++
            "LDFLAGS=-static-libstdc++ -static-libgcc"
  CACHE ${PROJECT_SOURCE_DIR}/cmake/ports/gdbserver.cache   # libc's getcwd (see there)
  BUILD_TARGETS all-gdbserver INSTALL_TARGETS install-gdbserver
  LINKS bin/gdbserver:/system/development/bin/gdbserver)
