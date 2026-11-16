#!/bin/sh
# After a SYSROOT port's headers and libraries are copied into the staging
# sysroot: point its pkg-config files there (they name the boot volume's
# PREFIX), and drop its libtool archives (their dependency_libs name the boot
# volume's paths, which later ports' libtool would look for on the build host).
#   stage-fixup.sh STAGING PREFIX
set -e
staging=$1 prefix=$2
for pc in "$staging"/lib/pkgconfig/*.pc; do
  [ -f "$pc" ] && grep -q "$prefix" "$pc" && sed -i "s|$prefix|$staging|g" "$pc"
done
rm -f "$staging"/lib/*.la
