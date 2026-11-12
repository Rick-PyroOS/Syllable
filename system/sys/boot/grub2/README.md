# GRUB 2 for Pyro

The boot loader, built for the build host by `cmake/Grub.cmake` from upstream
GRUB 2.12, platform i386-pc (BIOS), with the patches here.
`cmake/scripts/make-image.sh` writes it to the disk image.

| File | What it is |
|---|---|
| `patches/0001-*` | AFS: reads past a file's indirect range failed ("incorrect double-indirect block") |
| `patches/0002-*` | AFS/BFS: any failed lookup (missing file, symlink to `/boot/...`) freed an uninitialized pointer and could crash GRUB |
| `early.cfg` | Built into core.img: finds the volume with `/boot/grub/grub.cfg`, so one core.img works on any disk or partition. No comments in it: core.img runs it before the normal parser is loaded, and the rescue parser reports `#` as an unknown command |
| `grub.cfg` | The menu, installed as `/boot/grub/grub.cfg`. Entries `pyro`, `pyro-serial` (kernel log on COM1) and `pyro-pci`. It ends by reading `/boot/grub/custom.cfg` if there is one, for local settings such as `set default=pyro-serial` or `set pyro_root=...` |
| `theme/theme.txt`, `theme/*.png` | The graphical menu: background, a dark menu panel, an ember-red bar on the selected entry |
| `pyro-icon.png` | The Pyro flame icon the background is made from |
| `pyro-boot.png`, `theme/menu_*.png`, `theme/select_*.png` | Made by `make-background.py pyro-icon.png .` (1024x768 background and the 9-slice panel and bar) |
| `../splash/` | The kernel's boot splash (`splash.img`, installed as `/boot/grub/splash.img` and loaded as a boot module) |
| `fonts/*.pf2` | DejaVu Sans and Sans Mono (Latin-1 and box drawing), made with `grub-mkfont` |

Things GRUB 2 does differently from GRUB legacy, which `grub.cfg` handles:

- `module FILE` passes no file name to the kernel, and the kernel finds boot
  modules by `/drivers/` or `/config/` in that string, so each line repeats
  its path.
- Modules may be put anywhere in memory; `--quirk-modules-after-kernel` keeps
  them below the kernel's 1.5 GiB cap.
- Paths in the menu are relative to the AFS volume, so `/boot/...` symlinks
  (Syllable's own mount point) don't resolve in GRUB.

The disk layout: MBR (boot.img), core.img in sectors 1 to 2047, the AFS
partition from 1 MiB. core.img is about 155 KiB and did not fit before a
partition at sector 63.

`cmake/Grub.cmake` also makes two images for the release CD:

- `core-part1.img` (about 28 KiB): a small core.img that reads `/boot/grub`
  from the first partition of the disk it starts from. It fits before a
  partition at sector 63, the usual layout from DiskManager, so the installer
  can write it.
- `eltorito.img`: the CD's boot image, which reads `/boot/grub` on the CD.
