#!/usr/bin/env python3
# make-font.py TTF OUT.c: the kernel's 8x16 console font (ASCII 32-127) for
# bootsplash.c, rendered from a monospaced TrueType font with Pillow.
# Made once and kept in the tree as kernel/kernel/bootsplash_font.c:
#   python3 make-font.py /usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf \
#       ../../kernel/kernel/bootsplash_font.c
import sys
from PIL import Image, ImageDraw, ImageFont

ttf, out = sys.argv[1:3]
font = ImageFont.truetype(ttf, 14)
rows = []
for c in range(32, 128):
    im = Image.new('L', (8, 16), 0)
    if c < 127:
        ImageDraw.Draw(im).text((0, 0), chr(c), font=font, fill=255)
    px = im.load()
    rows.append([sum(0x80 >> x for x in range(8) if px[x, y] >= 110) for y in range(16)])

with open(out, 'w') as f:
    f.write('/* The kernel\'s 8x16 console font, ASCII 32-127, for bootsplash.c.\n'
            ' * Made by system/sys/boot/splash/make-font.py from %s\n'
            ' * (Bitstream Vera / DejaVu licence). */\n\n' % ttf.rsplit('/', 1)[-1])
    f.write('#include <kernel/types.h>\n\nconst uint8 g_anBootFont8x16[96][16] = {\n')
    for c, r in zip(range(32, 128), rows):
        name = chr(c) if 32 < c < 127 and chr(c) not in '\\*/' else '%d' % c
        f.write('\t{ %s },\t/* %s */\n' % (', '.join('0x%02x' % v for v in r), name))
    f.write('};\n')
