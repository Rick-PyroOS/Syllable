#!/bin/sh
# A port's source tree, unpacked and patched again if a finished build emptied
# it (SYLLABLE_PORTS_KEEP=OFF): its configure, build or install command has
# changed since, so it is rebuilt from the tarball in the downloads folder.
#   ensure-source.sh SOURCE_DIR EXTRACT_SCRIPT PORT_DIR
set -eu
src=$1 extract=$2 port=$3
[ -n "$(ls -A "$src" 2>/dev/null)" ] && exit 0
tarball=$(sed -n 's/^get_filename_component(filename "\(.*\)" ABSOLUTE)$/\1/p' "$extract")
[ -f "$tarball" ] || { echo "ensure-source.sh: no tarball ($tarball) for $src" >&2; exit 1; }
echo "-- unpacking $tarball again"
mkdir -p "$src"
tmp=$src.unpack
rm -rf "$tmp"; mkdir -p "$tmp"
tar -C "$tmp" -xf "$tarball"
# as ExternalProject does: a single top folder is the source tree itself
top=$tmp
if [ "$(ls -A "$tmp" | wc -l)" -eq 1 ]; then
  only=$tmp/$(ls -A "$tmp")
  [ -d "$only" ] && top=$only
fi
(cd "$top" && tar -cf - .) | (cd "$src" && tar -xf -)
rm -rf "$tmp"
"$(dirname "$0")/apply-port.sh" "$src" "$port"
