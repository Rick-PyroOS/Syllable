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
# Builder's patch-config: config.sub/config.guess that predate Syllable are
# replaced with automake's, which know i586-pc-syllable
new=/opt/syllable/host/share/automake-1.15
find . -name config.sub | while read -r sub; do
  if ! sh "$sub" i586-pc-syllable >/dev/null 2>&1; then
    cp "$new/config.sub" "$sub"
    cp "$new/config.guess" "$(dirname "$sub")/config.guess"
  fi
done
