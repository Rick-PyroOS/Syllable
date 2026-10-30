# OpenSSL and cURL, current versions (next-steps item 7).
#
# Shared system libraries in /usr/openssl/3.5.9 and /usr/curl/8.23.0, linked
# from /usr/index/{bin,lib,include,lib/pkgconfig}, for programs built on
# Syllable and for later ports.  Webster keeps its static OpenSSL 0.9.8zh and
# cURL 7.18.2 (Ports.cmake, the staging sysroot): these two never go into the
# staging sysroot, so the headers never meet.  Built with GCC 9.5.0 (OpenSSL 3
# needs C99), with its libgcc linked in (-static-libgcc): only the C ABI is
# shared with programs built by 4.1.2.

set(SYLLABLE_OPENSSL3_PREFIX /usr/openssl/3.5.9)
set(_ossl_dest ${CMAKE_BINARY_DIR}/ports/openssl3/dest)

# The CA certificates both use: curl's extract of Mozilla's, pinned by date
# (the release's 2009 bundle can't verify today's sites).  OpenSSL's default
# CA file, ssl/cert.pem.
ExternalProject_Add(cacert
  PREFIX ${CMAKE_BINARY_DIR}/ports/cacert
  URL https://curl.se/ca/cacert-2026-10-20.pem
  URL_HASH SHA256=796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_NO_EXTRACT ON
  CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")
install(FILES ${CMAKE_BINARY_DIR}/downloads/cacert-2026-10-20.pem
        DESTINATION .${SYLLABLE_OPENSSL3_PREFIX}/ssl RENAME cert.pem)

# OpenSSL has its own Configure (not autoconf): its own port, with
# cmake/ports/openssl-syllable.conf's target
set(_ossl_env ${CMAKE_COMMAND} -E env --unset=TARGET
    PATH=${SYLLABLE_TOOLCHAIN_ROOT}/bin:$ENV{PATH})
ExternalProject_Add(openssl3
  PREFIX ${CMAKE_BINARY_DIR}/ports/openssl3
  URL https://github.com/openssl/openssl/releases/download/openssl-3.5.9/openssl-3.5.9.tar.gz
  URL_HASH SHA256=603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  # no-asm: its x86 assembler is generated for Linux; no-module: the
  # providers built into libcrypto (no dlopen); no-secure-memory: no mlock
  CONFIGURE_COMMAND ${_ossl_env} perl <SOURCE_DIR>/Configure
    --config=${CMAKE_CURRENT_LIST_DIR}/openssl-syllable.conf syllable-x86
    shared no-asm no-threads no-dso no-module no-async no-afalgeng no-devcryptoeng
    no-ktls no-secure-memory no-tests no-docs
    --prefix=${SYLLABLE_OPENSSL3_PREFIX} --openssldir=${SYLLABLE_OPENSSL3_PREFIX}/ssl --libdir=lib
    CC=${_gcc95}-gcc AR=${_gcc95}-ar RANLIB=${_gcc95}-ranlib LDFLAGS=-static-libgcc
  BUILD_COMMAND ${_ossl_env} make -j${SYLLABLE_JOBS} build_sw
  INSTALL_COMMAND ${CMAKE_COMMAND} -E rm -rf ${_ossl_dest}
          COMMAND ${_ossl_env} make install_sw install_ssldirs DESTDIR=${_ossl_dest}
          COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/strip-tree.sh ${CMAKE_STRIP} ${_ossl_dest}
  DEPENDS cacert
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
install(DIRECTORY ${_ossl_dest}/ DESTINATION . USE_SOURCE_PERMISSIONS)
_syllable_links(${SYLLABLE_OPENSSL3_PREFIX} @bin:/usr/index/bin @lib:/usr/index/lib
                @lib/pkgconfig:/usr/index/lib/pkgconfig @include:/usr/index/include)

# cURL against it: its configure finds OpenSSL by path (PKG_CONFIG=false: the
# .pc files name the boot volume's prefix).  The synchronous resolver (the
# threaded one needs pthreads), IPv4 only, as Syllable's network stack.
syllable_port(curl8
  PORT cURL--current
  URL https://curl.se/download/curl-8.23.0.tar.gz
  SHA256 e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49
  PREFIX /usr/curl/8.23.0
  CONFIGURE --with-openssl=${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}
            --with-ca-bundle=${SYLLABLE_OPENSSL3_PREFIX}/ssl/cert.pem --without-ca-path
            --with-zlib=${SYLLABLE_STAGING} --without-libpsl --without-libssh2
            --without-brotli --without-zstd --without-nghttp2 --without-libidn2
            --disable-ldap --disable-threaded-resolver --disable-ipv6 --disable-docs
            --disable-manual
            ac_cv_file___dev_urandom_=yes
            # no pthreads: its failed check still leaves -pthread (= -lpthread,
            # the tree's libpthread, not in the staging sysroot) on the link line
            ac_cv_header_pthread_h=no
            PKG_CONFIG=false
            # OpenSSL 3 ahead of Webster's 0.9.8 in the staging sysroot (zlib is there too)
            CC=${_gcc95}-gcc
            "CPPFLAGS=-I${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}/include -I${SYLLABLE_STAGING}/include"
            "LDFLAGS=-static-libgcc -L${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}/lib -L${SYLLABLE_STAGING}/lib"
  SHARED DEPENDS openssl3 zlib
  LINKS @bin:/usr/index/bin @lib:/usr/index/lib @lib/pkgconfig:/usr/index/lib/pkgconfig
        @include:/usr/index/include)

# OpenSSH and rsync, current versions, against OpenSSL 3 and zlib 1.3.1, with
# GCC 9.5.0 (both need C99), as cURL.  The release's (OpenSSH 5.2, rsync
# 2.6.9) can't talk to today's servers.  Their config.sub doesn't know
# Syllable; apply-port.sh replaces it.

# ssh, scp, sftp, ssh-keygen, ssh-agent and sshd (not started: sshd needs host
# keys, ssh-keygen -A, and the privilege separation user "sshd" and /var/empty).
# install-nokeys: make install would run the new ssh-keygen to make host keys.
# No PAM, no PKCS#11 or FIDO keys (they need dlopen() at configure time).
syllable_port(openssh
  PORT OpenSSH-5.2p1
  URL https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-10.6p1.tar.gz
  SHA256 a9dc9565dffe8640f64d863cd29a32bc4a3dbdec0566a7fc44c5d6ee767d5f39
  PREFIX /usr/openssh/10.6p1
  CONFIGURE --sysconfdir=/etc/ssh --with-privsep-path=/var/empty
            --with-ssl-dir=${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX} --with-zlib=${SYLLABLE_STAGING}
            --without-pam --without-hardening --disable-strip
            "CPPFLAGS=-I${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}/include -I${SYLLABLE_STAGING}/include"
            "LDFLAGS=-static-libgcc -L${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}/lib -L${SYLLABLE_STAGING}/lib"
  INSTALL_TARGETS install-nokeys
  COMPILER 9.5.0 DEPENDS openssl3 zlib
  LINKS @bin:/usr/index/bin @sbin:/usr/index/sbin)

# O_CLOEXEC: glibc 2.7 for Syllable has none (rsync falls back to 0 itself,
# but only in one file).  No xxhash, zstd, lz4 or IDN (not ported), no IPv6
# (Syllable's network stack is IPv4).
syllable_port(rsync
  PORT rsync-2.6.9
  URL https://download.samba.org/pub/rsync/src/rsync-3.5.1.tar.gz
  SHA256 c55f9c9dc10fb8bec397b399a0fdded53cc9a2d8e30891bb0d63724d25c37bef
  PREFIX /usr/rsync/3.5.1
  CONFIGURE --disable-xxhash --disable-zstd --disable-lz4 --disable-md2man --disable-idn
            --disable-ipv6 --with-included-popt --with-included-zlib=no
            "CPPFLAGS=-DO_CLOEXEC=0 -I${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}/include -I${SYLLABLE_STAGING}/include"
            "LDFLAGS=-static-libgcc -L${_ossl_dest}${SYLLABLE_OPENSSL3_PREFIX}/lib -L${SYLLABLE_STAGING}/lib"
  COMPILER 9.5.0 DEPENDS openssl3 zlib
  LINKS @bin:/usr/index/bin)
