#!/bin/sh
# Copy the files (and links) in DIR whose names match PATTERN into DEST.
#   copy-matching.sh DIR PATTERN DEST
set -eu
dir=$1 pattern=$2 dest=$3
[ -d "$dir" ] && [ -n "$dest" ] && [ "$dest" != / ] || { echo "copy-matching.sh: bad arguments" >&2; exit 1; }
mkdir -p "$dest"
find "$dir" -maxdepth 1 -name "$pattern" -exec cp -a {} "$dest"/ \;
