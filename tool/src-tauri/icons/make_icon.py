"""Draws the TetherLRS app icon (icon-source.png, 1024 px); `npx tauri icon icons/icon-source.png` makes the sizes.
A rounded square in the app's accent gradient with a mast and radio arcs, as the Device icon in the app."""
from PIL import Image, ImageDraw

S = 4096  # drawn at 4x and scaled down, for smooth edges
img = Image.new('RGBA', (S, S), (0, 0, 0, 0))

grad = Image.new('RGBA', (S, S))
g = ImageDraw.Draw(grad)
a, b = (0, 95, 184), (106, 60, 255)  # accent, then towards violet
for i in range(2 * S):
    t = i / (2 * S)
    c = tuple(round(a[k] + (b[k] - a[k]) * t) for k in range(3)) + (255,)
    g.line([(i, 0), (0, i)], fill=c, width=2)
mask = Image.new('L', (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([int(S * 0.06), int(S * 0.06), int(S * 0.94), int(S * 0.94)], radius=int(S * 0.2), fill=255)
img.paste(grad, (0, 0), mask)

d = ImageDraw.Draw(img)
white = (255, 255, 255, 255)
w = int(S * 0.045)
cx, cy = S // 2, int(S * 0.44)
d.line([(cx, cy), (cx, int(S * 0.76))], fill=white, width=w)  # mast
r = int(S * 0.045)
d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=white)
for k, rad in enumerate((0.13, 0.24)):  # two arcs each side
    R = int(S * rad)
    box = [cx - R, cy - R, cx + R, cy + R]
    d.arc(box, start=145, end=215, fill=white, width=w)
    d.arc(box, start=-35, end=35, fill=white, width=w)

img.resize((1024, 1024), Image.LANCZOS).save(__file__.replace('make_icon.py', 'icon-source.png'))
print('icon-source.png')
