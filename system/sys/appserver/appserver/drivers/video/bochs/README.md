# bochs: QEMU standard VGA driver

The appserver video driver for the virtual display cards that implement the
Bochs VBE "DISPI" registers:

| QEMU option | Proxmox `vga:` | PCI ID | Registers |
|---|---|---|---|
| `-vga std` (QEMU's default), or `-device VGA,vgamem_mb=N` (what `sb run` uses: 256 MB, `--vga-mem N`) | `std` (Proxmox's default) | `1234:1111` | MMIO (BAR2), or the I/O ports if there's no BAR2 |
| `-device bochs-display` | none | `1234:1111`, class 0x0380 | MMIO only |
| `-vga qxl` | `qxl` ("SPICE") | `1b36:0100` | I/O ports `0x1ce`/`0x1cf`, while QXL is in its VGA mode |

Compared with the built-in VESA driver it gives:
- any resolution up to what the video memory holds, including 16:9 and
  16:10 sizes, set directly without the VGA BIOS. A custom size in the
  appserver config works too, as long as the width is a multiple of 8;
- the preferred size from QEMU's EDID, if present (`-device VGA,xres=1728,yres=972`);
- per-window backbuffers in video memory (mode B in `docs/appserver-review.md`)
  when there's enough of it (see below).

QEMU notices which framebuffer pages change, so screen updates reach noVNC,
VNC and SPICE viewers without help from the driver. The driver has no drawing
acceleration and no hardware cursor; the appserver's software paths do all of it.

## Pieces

- Kernel: `system/sys/kernel/drivers/graphics/bochs/driver.c` claims the PCI
  device and creates `/dev/graphics/bochs_*`, which names this driver.
- Appserver: `bochs.cpp`, installed as `/system/drivers/appserver/video/bochs`.

If the driver can't find the DISPI registers or map the framebuffer,
`init_gfx_driver()` returns NULL and the appserver falls back to VESA as before.

## Video memory and backbuffers

The memory manager is set up once, so the driver keeps room at the start of
video memory for the largest mode it lists and hands the rest to window
backbuffers:

- **16 MB** (QEMU's and Proxmox's default): every mode that fits is listed, up
  to 2560x1600 at 32 bpp, and there's too little left for backbuffers. Windows
  stay single-buffered, as under VESA.
- **32 MB or more**: only modes that fit in half of video memory are listed,
  and the other half is used for backbuffers. 64 MB gives up to 3840x2160 at
  32 bpp and about 32 MB of backbuffers. Above 64 MB the largest listed mode
  stays 3840x2160 (unless EDID asks for more), so nearly all the extra memory
  goes to backbuffers: 128 MB leaves about 96 MB, 512 MB about 480 MB.

The driver maps all of the device's video memory up to 1 GB
(`BOCHS_MAX_MAPPED_VRAM`); QEMU's std VGA gives at most 512 MB.

To raise it: QEMU `-device VGA,vgamem_mb=64` (instead of `-vga std`), Proxmox
`vga: std,memory=64`. For QXL, `vgamem_mb` is the part that counts here.

Build with `-DBOCHS_BACKBUFFERS=0` to keep every window single-buffered
whatever the memory size.

## Testing

A host-side test emulates QEMU's DISPI registers (from QEMU's
`hw/display/vga.c`) and runs the driver against std VGA with MMIO and with
I/O ports, bochs-display, QXL in VGA mode, a card without DISPI, and an EDID
preferred mode. It needs only mock Syllable headers, so it isn't part of the
build. Not yet run on Syllable itself.
