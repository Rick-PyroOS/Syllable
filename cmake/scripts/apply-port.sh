#!/bin/sh
# Apply a Builder port to an unpacked source tree, the way Builder does:
# <port>/<name>.patch with -p1, then the <port>/patches/ tree copied over the
# source (CVS directories skipped).
#   apply-port.sh SOURCE_DIR PORT_DIR
#
# The tree remembers which version of the port it has (.syllable-port, a
# checksum of the port's files).  Run again on a tree that has an older
# version (the port's patch changed; the patch step depends on its files),
# the tree is emptied and unpacked afresh (ensure-source.sh), then patched.
set -e
src=$1 port=$2 name=$(basename "$2")
# (a port without a Builder folder, zlib 1.3.1: nothing to apply, the sum of nothing)
sum=$(if [ -d "$port" ]; then cd "$port" && find . -name CVS -prune -o -type f -print | LC_ALL=C sort | xargs cat 2>/dev/null; fi | cksum)
if [ -f "$src/.syllable-port" ]; then
  [ "$(cat "$src/.syllable-port")" = "$sum" ] && exit 0
  # src is <ports>/NAME/src/NAME; ExternalProject's extract script names the tarball
  pname=$(basename "$src")
  extract=$(dirname "$src")/$pname-stamp/extract-$pname.cmake
  echo "-- the port $name changed: unpacking $pname again"
  find "$src" -mindepth 1 -delete
  exec "$(dirname "$0")/ensure-source.sh" "$src" "$extract" "$port"
fi
cd "$src"
if [ -f "$port/$name.patch" ]; then
  patch -p1 -s < "$port/$name.patch"
fi
if [ -d "$port/patches" ]; then
  (cd "$port/patches" && find . -name CVS -prune -o -type f -print) |
    while read -r f; do
      mkdir -p "$(dirname "$f")"
      cp "$port/patches/$f" "$f"
    done
fi
# Builder's patch-config: config.sub/config.guess that predate Syllable are
# replaced with automake's, which know i586-pc-syllable
new=/opt/syllable/host/share/automake-1.15
find . -name config.sub | while read -r sub; do
  if ! sh "$sub" i586-pc-syllable >/dev/null 2>&1; then
    cp "$new/config.sub" "$sub"
    cp "$new/config.guess" "$(dirname "$sub")/config.guess"
  fi
done
echo "$sum" > "$src/.syllable-port"
