#!/bin/bash
# Bring the disk image up to date with the boot volume tree (`cmake
# --install`'s stage).
#
#   make-image.sh MODE AFSTOOL REFERENCE_DISK STAGE OVERLAY OUT_DIR SIZE ROOT
#
# MODE auto: if the image exists and was made with the same size and boot
# sectors, sync it (afstool sync writes only what changed: new, changed and
# removed files, attributes, modes); otherwise build it from scratch.
# MODE full: always from scratch (mkfs, boot sectors, import).
#
# The image's contents are kept as a mirror tree (ROOT: the stage plus
# OVERLAY, which may be empty, plus the GRUB files) whose files keep their
# mtimes and user.* xattrs (Syllable attributes), so sync can tell what
# changed by size and mtime.  ROOT must be on a filesystem with xattrs.
#
# REFERENCE_DISK is a raw Syllable disk (syllable.qcow2 converted) whose first
# 63 sectors (MBR + GRUB stage1.5, which loads /boot/grub/stage2 and
# /boot/grub/menu.lst by path) and GRUB stage files are reused.
set -euo pipefail
mode=$1 afstool=$2 ref=$3 stage=$4 overlay=$5 out=$6 size=$7 root=$8
offset=32256
img=$out/syllable.img
state=$out/syllable.image-state
afs(){ AFSTOOL_OFFSET=$offset "$afstool" "$@"; }

mkdir -p "$out" "$root"

# ---- the mirror tree ------------------------------------------------------------
grub="boot/grub/stage1 boot/grub/stage2 boot/grub/afs_stage1_5 boot/grub/stage2_eltorito"
protect=()
for f in $grub; do protect+=(--filter "P /$f"); done
rsync -aX --delete "${protect[@]}" "$stage/" "$root/"
[ -z "$overlay" ] || rsync -aX "$overlay/" "$root/"
mkdir -p "$root/boot/grub"
for f in $grub; do
  if [ ! -f "$root/$f" ] || [ "$ref" -nt "$root/$f" ]; then
    afs "$ref" cat "/$f" > "$root/$f"
    touch -r "$ref" "$root/$f"       # a fixed mtime: unchanged unless the reference is
  fi
done
[ -f "$root/boot/grub/menu.lst" ] || { echo "no boot/grub/menu.lst" >&2; exit 1; }

# ---- sync or rebuild ------------------------------------------------------------
want="size=$size ref=$(stat -c '%s %Y' "$ref") offset=$offset"
if [ "$mode" = auto ] && [ -f "$img" ] && [ "$(cat "$state" 2>/dev/null)" = "$want" ]; then
  echo "image: syncing $img"
  afs "$img" sync --owner 0:0 "$root" /
else
  echo "image: building $img from scratch"
  rm -f "$img" "$state"
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
  afs "$img" mkfs "$size" Pyro
  afs "$img" import --owner 0:0 "$root" /
fi
afs "$img" fsck --strict
# replaced, not overwritten: the old ones may belong to another user
rm -f "$out/syllable.manifest" "$state"
afs "$img" manifest / > "$out/syllable.manifest"
echo "$want" > "$state"
echo "image: $img"
