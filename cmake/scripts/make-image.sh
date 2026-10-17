#!/bin/bash
# Build a bootable disk image from the boot volume tree (`cmake --install`'s
# stage), from scratch.
#
#   make-image.sh AFSTOOL REFERENCE_DISK STAGE OVERLAY OUT_DIR SIZE
#
# REFERENCE_DISK is a raw Syllable disk (syllable.qcow2 converted) whose first
# 63 sectors (MBR + GRUB stage1.5, which loads /boot/grub/stage2 and
# /boot/grub/menu.lst by path) and GRUB stage files are reused.  OVERLAY (may
# be empty) is copied over the stage, e.g. a menu.lst for the boot test.
set -euo pipefail
afstool=$1 ref=$2 stage=$3 overlay=$4 out=$5 size=$6
offset=32256
img=$out/syllable.img
work=$out/image-root

mkdir -p "$out"
rm -rf "$work"
cp -a "$stage" "$work"
[ -z "$overlay" ] || cp -a "$overlay/." "$work/"

mkdir -p "$work/boot/grub"
for f in stage1 stage2 afs_stage1_5 stage2_eltorito; do
  AFSTOOL_OFFSET=$offset "$afstool" "$ref" cat "/boot/grub/$f" > "$work/boot/grub/$f"
done
[ -f "$work/boot/grub/menu.lst" ] || { echo "no boot/grub/menu.lst" >&2; exit 1; }

rm -f "$img"
head -c $offset "$ref" > "$img"
python3 - "$img" "$size" <<'PY'
import struct, sys
img, size = sys.argv[1], sys.argv[2]
mult = {'K': 1 << 10, 'M': 1 << 20, 'G': 1 << 30}
n = int(size[:-1]) * mult[size[-1].upper()] if size[-1].isalpha() else int(size)
with open(img, 'r+b') as f:
    f.seek(446 + 12)                       # partition 0: sector count
    f.write(struct.pack('<I', n // 512))
PY
AFSTOOL_OFFSET=$offset "$afstool" "$img" mkfs "$size" Pyro
AFSTOOL_OFFSET=$offset "$afstool" "$img" import --owner 0:0 "$work" /
AFSTOOL_OFFSET=$offset "$afstool" "$img" fsck --strict
AFSTOOL_OFFSET=$offset "$afstool" "$img" manifest / > "$out/syllable.manifest"
rm -rf "$work"
echo "image: $img"
