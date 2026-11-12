#!/usr/bin/env python3
# make-splash.py PICTURE.png OUT.img [BAR_X BAR_Y BAR_W BAR_H]
#
# The kernel's boot splash (kernel/kernel/bootsplash.c): the picture, as
# runs of identical pixels, and where to draw the progress bar on it.
# GRUB loads the file as a boot module (grub.cfg: module /boot/grub/splash.img
# /boot/grub/splash.img).  No Pillow needed: the PNG must be 8-bit RGB or
# RGBA, not interlaced.
#
# File layout (little-endian): the header (SplashHeader_s), then 4-byte runs:
# count (1-255), blue, green, red, row by row from the top.
import struct, sys, zlib

def read_png(path):
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        sys.exit('%s: not a PNG' % path)
    pos, idat, w = 8, b'', None
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
            if depth != 8 or ctype not in (2, 6) or interlace:
                sys.exit('%s: needs 8-bit RGB or RGBA, not interlaced' % path)
            bpp = 3 if ctype == 2 else 4
        elif kind == b'IDAT':
            idat += body
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 0xff
            elif f == 2: line[i] = (line[i] + b) & 0xff
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 0xff
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 0xff
        rows.append(line)
        prev = line
    pixels = []
    for line in rows:
        for x in range(w):
            r, g, b = line[x * bpp:x * bpp + 3]
            pixels.append((b, g, r))
    return w, h, pixels

src, out = sys.argv[1:3]
w, h, pixels = read_png(src)
# The progress bar: below the icon, above the flames (for the 1024x768 Pyro
# picture); colours from the GRUB theme (theme/select_*.png)
bar = [int(v) for v in sys.argv[3:7]] if len(sys.argv) >= 7 else [w // 2 - 160, int(h * 0.62), 320, 10]
track, border, left, right = 0x140a08, 0x783219, 0xb02814, 0xffa632

runs = bytearray()
i = 0
while i < len(pixels):
    j = i
    while j < len(pixels) and j - i < 255 and pixels[j] == pixels[i]:
        j += 1
    runs += bytes((j - i,)) + bytes(pixels[i])
    i = j

fmt = '<4sHHHHIHHHHIIIII'
hdr = struct.pack(fmt, b'PSPL', 1, struct.calcsize(fmt), w, h, 0x0c0808,
                  bar[0], bar[1], bar[2], bar[3], track, border, left, right, len(runs))
open(out, 'wb').write(hdr + runs)
print('splash: %dx%d, %d bytes' % (w, h, len(hdr) + len(runs)))
