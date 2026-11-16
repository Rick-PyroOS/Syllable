#!/bin/sh
# Link kernel.so the way the kernel Makefile does: compile version.c (build
# date and time) fresh, then ld with kernel.lnk and the objects in Makefile
# order (intel.o first, syscall.o after the seventh C object), then libgcc.
#
#   link-kernel.sh CC LD KERNEL_LNK VERSION_C OUT INTEL_O SYSCALL_O \
#                  -- CFLAGS... -- C_OBJECTS...
set -e
cc=$1 ld=$2 lnk=$3 version_c=$4 out=$5 intel_o=$6 syscall_o=$7
shift 7
[ "$1" = -- ] && shift
cflags=
while [ $# -gt 0 ] && [ "$1" != -- ]; do cflags="$cflags $1"; shift; done
shift
first=
for i in 1 2 3 4 5 6 7; do first="$first $1"; shift; done
version_o=${out%/*}/version.o
# shellcheck disable=SC2086
$cc -c $cflags "$version_c" -o "$version_o"
# shellcheck disable=SC2086
$ld -m elf_i386_syllable -T"$lnk" "$intel_o" $first "$syscall_o" "$@" "$version_o" \
    -o "$out" "$($cc -kernel -print-libgcc-file-name)"
