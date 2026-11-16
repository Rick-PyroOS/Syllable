#!/bin/sh
# Teach a configured package's libtool scripts about Syllable's shared
# libraries, as Builder's libtool patch (Builder/packages/libtool-2.2.6) does
# for libtool.m4: an ELF system with Linux-style sonames.  libtool otherwise
# doesn't know i586-pc-syllable and builds static libraries only.
#   libtool-syllable.sh BUILD_DIR [static]
# static: only the version scheme, for a static-only port (libtool's
# version_type=none for an unknown system can't take -version-number:
# "CURRENT `' must be a nonnegative integer", libtiff)
# libtool-syllable.sh SOURCE_DIR configure: before configure, in the
# package's configure scripts, libtool's own Linux cases (the
# "linux* | k*bsd*-gnu" ones) also take Syllable, so configure plans a
# shared build (curl then builds its program accordingly)
set -e
if [ "${2:-}" = configure ]; then
  find "$1" -name configure -type f | while read -r cf; do
    grep -q 'linux\* | k\*bsd\*-gnu' "$cf" || continue
    grep -q 'syllable\* | linux\* | k\*bsd\*-gnu' "$cf" && continue
    sed -i 's/^\([[:space:]]*\)linux\* | k\*bsd\*-gnu/\1syllable* | linux* | k*bsd*-gnu/' "$cf"
    echo "libtool-syllable: $cf"
  done
  exit 0
fi
find "$1" -name libtool -type f | while read -r lt; do
  grep -q '^build_libtool_libs=' "$lt" || continue
  if [ "${2:-}" = static ]; then
    sed -i 's/^version_type=none$/version_type=linux/' "$lt"
    continue
  fi
  sed -i \
    -e 's/^build_libtool_libs=no$/build_libtool_libs=yes/' \
    -e 's/^version_type=.*/version_type=linux/' \
    -e 's/^need_lib_prefix=.*/need_lib_prefix=no/' \
    -e 's/^need_version=.*/need_version=no/' \
    -e 's/^library_names_spec=.*/library_names_spec="\\$libname\\$release\\$shared_ext\\$versuffix \\$libname\\$release\\$shared_ext\\$major \\$libname\\$shared_ext"/' \
    -e 's/^soname_spec=.*/soname_spec="\\$libname\\$release\\$shared_ext\\$major"/' \
    -e 's/^shrext_cmds=.*/shrext_cmds=".so"/' \
    -e 's/^dynamic_linker=.*/dynamic_linker="Syllable"/' \
    -e 's/^deplibs_check_method=.*/deplibs_check_method="pass_all"/' \
    -e 's/^ld_shlibs=no$/ld_shlibs=yes/' \
    -e 's/^shlibpath_var=.*/shlibpath_var=LIBRARY_PATH/' \
    -e 's/^runpath_var=.*/runpath_var=DLL_PATH/' \
    "$lt"
  echo "libtool-syllable: $lt"
done
