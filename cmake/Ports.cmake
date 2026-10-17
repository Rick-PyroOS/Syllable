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
