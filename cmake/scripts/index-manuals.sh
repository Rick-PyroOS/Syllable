#!/bin/sh
# The manual pages' index: /usr/index/manuals, which /etc/profile's MANPATH
# searches (as /resources/index/manuals).  Run at install time, after every
# component has installed (the end of the top-level CMakeLists.txt).
#   index-manuals.sh ROOT [MAKEWHATIS]
#
# The ports install their pages in their own folders, PREFIX/share/man/manN
# or PREFIX/man/manN, PREFIX being /usr/<package>/<version> or
# /system/resources/<package>/<version>.  Each page is linked into
# ROOT/usr/index/manuals/manN, with a relative link, so that it resolves both
# on Syllable and in the build tree.  When two ports have a page of the same
# name (gcc 4.1.2 and 9.5.0), the first in sorted order is kept: the system
# compiler's.  The index is made afresh every time, so pages of ports that
# are gone leave no links behind.  MAKEWHATIS (the build host's mandoc)
# then writes mandoc.db there, for apropos and whatis.
set -e
root=$1 makewhatis=$2
idx=usr/index/manuals
cd "$root"
mkdir -p "$idx"
find "$idx" -mindepth 1 -maxdepth 1 \( -name 'man*' -o -name mandoc.db \) -exec rm -rf {} +
pages=0
for dir in usr/*/*/share/man usr/*/*/man system/resources/*/*/share/man system/resources/*/*/man; do
  case $dir in usr/index/*) continue ;; esac
  [ -d "$dir" ] || continue
  for sec in "$dir"/man*; do
    [ -d "$sec" ] || continue
    s=${sec##*/}
    for page in "$sec"/*; do
      [ -f "$page" ] || continue                  # (follows links: a broken one is left out)
      link=$idx/$s/${page##*/}
      [ -e "$link" ] || [ -L "$link" ] && continue
      mkdir -p "$idx/$s"
      ln -s "../../../../$page" "$link"
      pages=$((pages + 1))
    done
  done
done
echo "-- Manual pages: $pages linked into /$idx"
if [ -n "$makewhatis" ]; then
  "$makewhatis" "$idx"
  echo "-- Manual pages: /$idx/mandoc.db written"
fi
