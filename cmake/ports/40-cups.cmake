# CUPS (next-steps item 5): the release's 1.3.4 with its Syllable backends
# (Builder/packages/cups-1.3.4: usb, parallel, ieee1284).  Its release
# tarballs are gone, so the source is the OpenPrinting/cups tag, which has no
# configure: autoconf makes it.  Shared libcups in /usr/cups/1.3.4, config in
# /etc/cups, spool and logs under /var.  Printers and PrintQueue build once
# syllable::cups exists (below); cupsd starts from
# /system/index/tasks/start/cupsd.  No SSL, D-Bus, LDAP, SLP, PAM or image
# filters (those need shared image libraries), and no domain socket: clients
# talk to localhost:631.

set(SYLLABLE_CUPS_PREFIX /usr/cups/1.3.4)
set(_cups_dest ${CMAKE_BINARY_DIR}/ports/cups/dest)
set(_cups_env ${CMAKE_COMMAND} -E env --unset=TARGET
    PATH=${_cross_bin}:${SYLLABLE_STAGING}/bin:$ENV{PATH}
    CC=i586-pc-syllable-gcc CXX=i586-pc-syllable-g++ AR=i586-pc-syllable-ar
    RANLIB=i586-pc-syllable-ranlib STRIP=i586-pc-syllable-strip
    CPPFLAGS=-I${SYLLABLE_STAGING}/include LDFLAGS=-L${SYLLABLE_STAGING}/lib
    # libcups.so is linked without LDFLAGS: its -lz needs the staging sysroot here
    DSOFLAGS=-L${SYLLABLE_STAGING}/lib)
# What's built: not the man pages (made by running a built program), the
# tests or pdftops (PostScript printing needs GhostScript, not ported yet)
set(_cups_dirs "DIRS=cups backend berkeley cgi-bin filter locale monitor notifier scheduler systemv conf data doc fonts ppd templates")

ExternalProject_Add(cups
  PREFIX ${CMAKE_BINARY_DIR}/ports/cups
  URL https://github.com/OpenPrinting/cups/archive/refs/tags/release-1.3.4.tar.gz
  URL_HASH SHA256=150ea43e42290beebc4f6337b4d6e57a0951c1dcfebb835e2003237e938aa215
  DOWNLOAD_NAME cups-1.3.4.tar.gz
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  BUILD_IN_SOURCE ON
  PATCH_COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/apply-port.sh <SOURCE_DIR> ${SYLLABLE_BUILDER_PORTS}/cups-1.3.4
  CONFIGURE_COMMAND ${_cups_env} autoconf
            COMMAND ${_cups_env} ./configure --build=x86_64-pc-linux-gnu --host=i586-pc-syllable
              --prefix=${SYLLABLE_CUPS_PREFIX} --sysconfdir=/etc --localstatedir=/var
              --libdir=${SYLLABLE_CUPS_PREFIX}/lib   # else lib64, from the build host's uname
              --without-rcdir --without-xinetd --without-domainsocket
              --disable-ssl --disable-gnutls --disable-openssl --disable-cdsassl
              --disable-dbus --disable-ldap --disable-slp --disable-pam --disable-gssapi
              --disable-launchd --disable-dnssd --disable-image --disable-jpeg --disable-png
              --disable-tiff --disable-pdftops --disable-threads
              --without-java --without-perl --without-php --without-python
              --with-cups-user=root --with-cups-group=root --with-system-groups=root
  BUILD_COMMAND ${_cups_env} make -j${SYLLABLE_JOBS} ${_cups_dirs}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_cups_dest}
          COMMAND ${_cups_env} make install DSTROOT=${_cups_dest} ${_cups_dirs}
          COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_cups_dest}
          # for Printers and PrintQueue: the headers and libcups
          COMMAND ${CMAKE_COMMAND} -E copy_directory ${_cups_dest}${SYLLABLE_CUPS_PREFIX}/include ${SYLLABLE_STAGING}/include
          COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/copy-matching.sh ${_cups_dest}${SYLLABLE_CUPS_PREFIX}/lib "libcups.so*" ${SYLLABLE_STAGING}/lib
  DEPENDS zlib
  BUILD_BYPRODUCTS ${SYLLABLE_STAGING}/lib/libcups.so
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
install(DIRECTORY ${_cups_dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
_syllable_links(${SYLLABLE_CUPS_PREFIX} @bin:/usr/index/bin @sbin:/usr/index/sbin
                @lib:/usr/index/lib @include:/usr/index/include)

add_library(syllable_cups INTERFACE)
target_include_directories(syllable_cups SYSTEM INTERFACE ${SYLLABLE_STAGING}/include)
target_link_libraries(syllable_cups INTERFACE ${SYLLABLE_STAGING}/lib/libcups.so syllable_z)
add_dependencies(syllable_cups cups)
add_library(syllable::cups ALIAS syllable_cups)
