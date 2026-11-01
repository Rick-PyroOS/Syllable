#!/bin/sh
# Strip a port's installed files as Builder does: programs fully, shared
# libraries of debug info only.
#   strip-tree.sh STRIP DIR
set -e
strip=$1 dir=$2
find "$dir" -type f | while read -r f; do
  [ "$(head -c4 "$f" | tail -c3)" = ELF ] || continue
  case $f in
    */lib/*.so*|*.so|*.so.*) "$strip" --strip-debug "$f" ;;
    */bin/*|*/sbin/*|*/libexec/*) "$strip" "$f" ;;
  esac
done
