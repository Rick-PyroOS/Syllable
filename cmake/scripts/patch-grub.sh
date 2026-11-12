#!/bin/sh
# Patch GRUB's source tree (run in it by ExternalProject):
#   patch-grub.sh PATCH...
set -eu
for p in "$@"; do
  patch -p1 -N < "$p"
done
# The 2.12 tarball lacks grub-core/extra_deps.lst (fixed upstream after the
# release); without it the build stops.
[ -f grub-core/extra_deps.lst ] || echo 'depends bli part_gpt' > grub-core/extra_deps.lst
