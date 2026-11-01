#!/bin/sh
# Unpack packages that each hold one top folder (4.1.2/, or gcc-9.5.0/ and
# binutils-2.35/) and merge those folders' contents into DEST.
#   unpack-merged.sh DEST TARBALL...
set -eu
dest=$1; shift
[ -n "$dest" ] && [ "$dest" != / ] || { echo "unpack-merged.sh: bad DEST '$dest'" >&2; exit 1; }
tmp=$dest.unpack
rm -rf "$dest" "$tmp"
mkdir -p "$dest"
for pkg in "$@"; do
  rm -rf "$tmp"; mkdir -p "$tmp"
  tar -C "$tmp" -xzf "$pkg"
  set -- "$tmp"/*
  [ $# -eq 1 ] && [ -d "$1" ] || { echo "unpack-merged.sh: $pkg must hold a single folder" >&2; exit 1; }
  cp -a "$1"/. "$dest"/
done
rm -rf "$tmp"
