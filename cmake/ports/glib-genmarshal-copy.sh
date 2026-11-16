#!/bin/sh
# Replacement for glib-genmarshal during cross-compilation of GLib 2.14.4.
#
# The source tarball ships pre-generated marshal files in gobject/ and
# tests/gobject/.  When cross compiling, the target's glib-genmarshal cannot
# run (it is a Syllable binary), and the build host's version (2.64.6) produces
# output in a newer format incompatible with GLib 2.14 source.
#
# This script is used as GLIB_GENMARSHAL (via glib.cache). The Makefile rule
# for the main gmarshal.h wraps our output with its own header guard:
#   echo "#ifndef __G_MARSHAL_H__" > xgen-gmh \
#   && echo "#define __G_MARSHAL_H__" >> xgen-gmh \
#   && $(glib_genmarshal) ... --header >> xgen-gmh \
#   && echo "#endif" >> xgen-gmh \
#   && mv xgen-gmh gmarshal.h
#
# So for --header mode we output ONLY the inner content (G_BEGIN_DECLS through
# G_END_DECLS), stripping the source file's own header guard.
#
# The tests/gobject/ directory has its own testmarshal.h/testmarshal.c files
# with the same structure.

set -e

# Parse arguments: find the .list file path and the mode
listfile=""
mode=""
for arg in "$@"; do
  case "$arg" in
    --header) mode=header ;;
    --body)   mode=body ;;
    *)
      case "$arg" in
        *.list) listfile="$arg" ;;
      esac
      ;;
  esac
done

if [ -z "$listfile" ]; then
  echo "ERROR: cannot find .list source path" >&2
  exit 1
fi

# Derive the base name (e.g. "gmarshal" from "gmarshal.list",
# or "testmarshal" from "testmarshal.list")
basedir=$(dirname "$listfile")
basename=$(basename "$listfile" .list)

case "$mode" in
  header)
    # Strip the header guard from the source .h file.
    # Output only G_BEGIN_DECLS through G_END_DECLS (inclusive).
    awk '/^G_BEGIN_DECLS/,/^G_END_DECLS/' "$basedir/${basename}.h"
    ;;
  body)
    cat "$basedir/${basename}.c"
    ;;
  *)
    # When called without --header or --body (e.g. --stdin), just pass through
    cat
    ;;
esac
