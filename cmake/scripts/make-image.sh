#!/bin/bash
# Bring the disk image up to date with the boot volume tree (`cmake
# --install`'s stage).
#
#   make-image.sh MODE AFSTOOL GRUB_DIR STAGE OVERLAY OUT_DIR SIZE ROOT
#
# MODE auto: if the image exists and was made with the same size and boot
# code, sync it (afstool sync writes only what changed: new, changed and
# removed files, attributes, modes); otherwise build it from scratch.
# MODE full: always from scratch (partition table, boot code, mkfs, import).
#
# The image's contents are kept as a mirror tree (ROOT: the stage plus
# OVERLAY, which may be empty) whose files keep their mtimes and user.* xattrs
# (Syllable attributes), so sync can tell what changed by size and mtime.
# ROOT must be on a filesystem with xattrs.
#
# Disk layout (GRUB 2, BIOS):
#   sector 0          MBR: GRUB_DIR/boot.img's code + one partition entry
#   sectors 1..2047   GRUB_DIR/core.img (finds /boot/grub by searching for it)
#   sector 2048 on    partition 1, type 0x2a: the AFS boot volume, SIZE bytes
# The partition starts at 1 MiB because core.img (about 155 KiB) does not fit
# in the 31 KiB that the old sector-63 layout left.
set -euo pipefail
mode=$1 afstool=$2 grub=$3 stage=$4 overlay=$5 out=$6 size=$7 root=$8
offset=1048576
img=$out/syllable.img
state=$out/syllable.image-state
afs(){ AFSTOOL_OFFSET=$offset "$afstool" "$@"; }

for f in boot.img core.img; do
  [ -f "$grub/$f" ] || { echo "make-image.sh: no $grub/$f (build the grub target)" >&2; exit 1; }
done
mkdir -p "$out" "$root"

# ---- the mirror tree ------------------------------------------------------------
rsync -aX --delete "$stage/" "$root/"
[ -z "$overlay" ] || rsync -aX "$overlay/" "$root/"
[ -f "$root/boot/grub/grub.cfg" ] || { echo "no boot/grub/grub.cfg" >&2; exit 1; }

# ---- boot code ------------------------------------------------------------------
# Rewritten on every run (it lives outside the file system, so sync can't see
# it); a changed partition size still means a new image.
write_boot() {
  python3 - "$img" "$size" "$offset" "$grub/boot.img" "$grub/core.img" <<'PY'
import struct, sys, os
img, size, offset, boot, core = sys.argv[1:]
mult = {'K': 1 << 10, 'M': 1 << 20, 'G': 1 << 30}
n = int(size[:-1]) * mult[size[-1].upper()] if size[-1].isalpha() else int(size)
start = int(offset) // 512
code = open(boot, 'rb').read()
core = open(core, 'rb').read()
nsect = (len(core) + 511) // 512
if 1 + nsect > start:
    sys.exit("core.img (%d sectors) does not fit before the partition" % nsect)
with open(img, 'r+b') as f:
    f.write(code[:440])                       # boot code; disk signature and table follow
    f.seek(446)                               # partition 1: bootable, AtheOS/Syllable, LBA only
    f.write(struct.pack('<B3sB3sII', 0x80, b'\xfe\xff\xff', 0x2a, b'\xfe\xff\xff', start, n // 512))
    f.write(b'\0' * 48)
    f.write(b'\x55\xaa')
    f.seek(0x5c)                              # boot.img: where core.img starts
    f.write(struct.pack('<Q', 1))
    f.seek(0x64)                              # boot.img: boot drive = the BIOS's
    f.write(b'\xff')
    f.seek(512)
    f.write(core)
    f.seek(512 + 512 - 12)                    # diskboot's block list: the rest of core.img
    f.write(struct.pack('<QHH', 2, nsect - 1, 0x820))
PY
}

# ---- sync or rebuild ------------------------------------------------------------
want="size=$size offset=$offset layout=grub2"
if [ "$mode" = auto ] && [ -f "$img" ] && [ "$(cat "$state" 2>/dev/null)" = "$want" ]; then
  echo "image: syncing $img"
  write_boot
  afs "$img" sync --owner 0:0 "$root" /
else
  echo "image: building $img from scratch"
  rm -f "$img" "$state"
  truncate -s "$offset" "$img"
  write_boot
  afs "$img" mkfs "$size" Pyro
  afs "$img" import --owner 0:0 "$root" /
fi
afs "$img" fsck --strict
# replaced, not overwritten: the old ones may belong to another user
rm -f "$out/syllable.manifest" "$state"
afs "$img" manifest / > "$out/syllable.manifest"
echo "$want" > "$state"
echo "image: $img"
