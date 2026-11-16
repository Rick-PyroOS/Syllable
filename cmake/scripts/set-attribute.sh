#!/bin/sh
# Set a Syllable string attribute on an installed file or directory:
#   set-attribute.sh ADDATTRIB PATH NAME VALUE
# Files go through the host addattrib; it cannot open directories, so those
# get the user.* xattrs it would write (the value, and its type: 5, a string),
# which afstool carries into AFS.
set -e
addattrib=$1 path=$2 name=$3 value=$4
if [ -d "$path" ]; then
  setfattr -n "user.$name" -v "$value" "$path"
  setfattr -n "user.syllable.type.$name" -v 0sBQAAAA== "$path"
else
  exec "$addattrib" "$path" "$name" "$value"
fi
