# Boot splash

What the screen shows between the GRUB menu and the desktop. The kernel code
is `system/sys/kernel/kernel/bootsplash.c`.

1. The kernel's Multiboot header (`kernel/intel.s`) asks for a 1024x768x32
   graphics mode, and GRUB 2 starts the kernel in it, or in another
   graphics mode if that one isn't available. GRUB legacy ignores the request
   and starts the kernel in text mode.
2. As soon as it can map the framebuffer (after `init_areas()`), the kernel
   draws `splash.img`, a boot module (`module /boot/grub/splash.img
   /boot/grub/splash.img` in grub.cfg), centred on a background colour, with
   a progress bar. The bar moves with the boot modules (to 20%), the root
   file system (25%) and each driver (towards 90%). It fills up when init
   starts, and from then on the kernel leaves the screen to the appserver.
3. `splash=false`, or no splash module: the kernel log is drawn as text in
   the framebuffer instead (8x16 font, `kernel/bootsplash_font.c`).
4. A panic always puts the kernel log on screen.
5. In text mode the old 80x25 console is used, now light grey on black, not
   white on blue. `quiet=true` keeps the log off it (except for a panic).

The log always goes to the serial port (with `debug_port=...`) and the
kernel log buffer, whatever the screen shows.

| File | What it is |
|---|---|
| `make-splash.py` | Makes `splash.img` from a PNG at build time (`cmake/Grub.cmake`, from `../grub2/pyro-boot.png`): the picture as runs of pixels (about 290 KB for the Pyro picture), and where the progress bar goes and its colours. Needs only python3 |
| `make-font.py` | Made `kernel/bootsplash_font.c` from DejaVu Sans Mono Bold with Pillow. Run it again only to change the font |

To use another picture, make it 1024x768 (smaller works too: it's centred),
and give `make-splash.py` the bar's position and size if the default (centred,
62% down, 320x10) doesn't suit it.
