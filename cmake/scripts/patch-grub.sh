#!/bin/sh
# Patch GRUB's source tree (run in it by ExternalProject):
#   patch-grub.sh PATCH...
# Safe to run again on a tree that is already patched (CMake reruns the
# patch step when the build directory is reconfigured).
set -eu
for p in "$@"; do
  if patch -p1 -R --dry-run -s -f < "$p" >/dev/null 2>&1; then
    echo "-- already applied: $(basename "$p")"
    continue
  fi
  patch -p1 -N < "$p"
done
# The 2.12 tarball lacks grub-core/extra_deps.lst (fixed upstream after the
# release); without it the build stops.
[ -f grub-core/extra_deps.lst ] || echo 'depends bli part_gpt' > grub-core/extra_deps.lst
