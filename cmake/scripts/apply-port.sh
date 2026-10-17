#!/bin/sh
# Apply a Builder port to an unpacked source tree, the way Builder does:
# <port>/<name>.patch with -p1, then the <port>/patches/ tree copied over the
# source (CVS directories skipped).
#   apply-port.sh SOURCE_DIR PORT_DIR
set -e
src=$1 port=$2 name=$(basename "$2")
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
