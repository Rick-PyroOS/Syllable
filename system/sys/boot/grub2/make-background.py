# mkbg2.py ICON.png OUTDIR : Pyro GRUB background (icon on a dark ember backdrop)
# plus the 9-slice pixmaps for the menu panel and the selected entry.
import sys, math, os
from PIL import Image, ImageDraw, ImageFilter
icon_path, out = sys.argv[1], sys.argv[2]
W, H = 1024, 768
bg = Image.new('RGB', (W, H))
px = bg.load()
for y in range(H):
    for x in range(W):
        # near-black with a warm glow behind the icon and embers at the bottom
        g1 = max(0.0, 1 - math.hypot((x - W/2) / 330, (y - 190) / 260))
        g2 = max(0.0, 1 - math.hypot((x - W/2) / (W*0.7), (y - H*1.08) / 300))
        r = 12 + 95*g1**2 + 40*g2**2
        g = 8 + 28*g1**2 + 12*g2**2
        b = 8 + 10*g1**2 + 4*g2**2
        px[x, y] = (int(min(r,255)), int(min(g,255)), int(min(b,255)))
icon = Image.open(icon_path).convert('RGBA')
bbox = icon.getbbox()
icon = icon.crop(bbox)
s = 300 / icon.height
icon = icon.resize((round(icon.width*s), 300), Image.LANCZOS)
# soft orange halo under the icon
halo = Image.new('RGBA', (W, H), (0,0,0,0))
a = icon.split()[3].point(lambda v: int(v*0.55))
glow = Image.new('RGBA', icon.size, (255,110,30,0)); glow.putalpha(a)
halo.paste(glow, ((W-icon.width)//2, 22), glow)
halo = halo.filter(ImageFilter.GaussianBlur(22))
bg = Image.alpha_composite(bg.convert('RGBA'), halo)
bg.alpha_composite(icon, ((W-icon.width)//2, 22))

# ---- a row of flames along the bottom, in the icon's flat colours ----------
import random
rnd = random.Random(7)
def flame(d, cx, base, w, h, colour, lean, wave):
    # a tongue of flame like the icon's: a round belly near the base that
    # narrows into a curved tip, leaning and swaying
    steps = 40
    def side(sign):
        out = []
        for i in range(steps + 1):
            t = i / steps
            half = w/2 * (1 - t)**1.35 * (0.75 + 1.1*t) / 1.0
            sway = lean*t**2 + wave*math.sin(t*math.pi*1.6)*t
            out.append((cx + sign*half + sway, base - h*t))
        return out
    left = side(-1)
    right = side(1)
    d.polygon(left + right[::-1], fill=colour)
layers = [((178, 40, 38), 95, 150, 58),       # deep red, tallest, at the back
          ((228, 76, 44), 72, 115, 50),       # red-orange
          ((245, 128, 40), 52, 85, 44),       # orange
          ((254, 171, 41), 32, 58, 38),       # yellow
          ((252, 200, 96), 14, 26, 30)]       # gold, the hot base
fire = Image.new('RGBA', (W, H), (0, 0, 0, 0))
fd = ImageDraw.Draw(fire)
for colour, hmin, hmax, spacing in layers:
    x = -spacing/2 + rnd.uniform(0, spacing)
    while x < W + spacing:
        h = rnd.uniform(hmin, hmax) * (0.75 + 0.25*math.sin(x/W*math.pi))
        flame(fd, x, H + 6, spacing*rnd.uniform(1.2, 1.6), h, colour, rnd.uniform(-16, 16), rnd.uniform(-6, 6))
        x += spacing * rnd.uniform(0.7, 1.1)
    fd.rectangle((0, H - hmin*0.35, W, H), fill=colour)
# a few sparks above the flames
for _ in range(12):
    x, y = rnd.uniform(30, W-30), H - rnd.uniform(160, 215)
    r = rnd.uniform(1.5, 3)
    fd.ellipse((x-r, y-r*2.2, x+r, y+r), fill=(254, 171, 41, 120))
# fade the fire: about half opacity at the base, thinning out towards the tips
fa = fire.split()[3]
ramp = Image.new('L', (1, H))
for y in range(H):
    t = min(1.0, max(0.0, (H - y) / 230))       # 0 at the bottom edge, 1 at 230 px up
    ramp.putpixel((0, y), int(255 * 0.55 * (1 - 0.75*t)))
ramp = ramp.resize((W, H))
from PIL import ImageChops
fire.putalpha(ImageChops.multiply(fa, ramp))
bg = Image.alpha_composite(bg, fire)
bg.convert('RGB').save(os.path.join(out, 'pyro-boot.png'), optimize=True)

def nine(prefix, fill, border, radius, bw):
    # 9-slice set: corners radius x radius, edges 1 px long, centre 1x1
    n = radius*2 + 1
    im = Image.new('RGBA', (n, n), (0,0,0,0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, n-1, n-1), radius=radius, fill=fill, outline=border, width=bw)
    r = radius
    parts = {'nw': (0,0,r,r), 'n': (r,0,r+1,r), 'ne': (r+1,0,n,r),
             'w': (0,r,r,r+1), 'c': (r,r,r+1,r+1), 'e': (r+1,r,n,r+1),
             'sw': (0,r+1,r,n), 's': (r,r+1,r+1,n), 'se': (r+1,r+1,n,n)}
    for k, box in parts.items():
        im.crop(box).save(os.path.join(out, 'theme', f'{prefix}_{k}.png'))
os.makedirs(os.path.join(out, 'theme'), exist_ok=True)
nine('menu', (8, 5, 5, 255), (120, 50, 25, 255), 10, 1)          # dark panel (GRUB draws it opaque)
nine('select', (110, 32, 14, 255), (255, 140, 50, 255), 8, 2)    # ember-red bar, orange edge
