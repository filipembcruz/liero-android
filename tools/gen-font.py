#!/usr/bin/env python3
"""Bakes the UI font atlas used by the native HUD/menus (game/ui/font.bin).

Format (little endian):
  'LFNT' u16 atlasW u16 atlasH u16 lineHeight u16 ascent u16 glyphCount
  glyphCount x { u32 codepoint, u16 x, u16 y, u16 w, u16 h, i16 xoff, i16 yoff, u16 advance }
  atlasW*atlasH bytes of 8-bit alpha
Font: Poppins Bold (SIL Open Font License 1.1).
"""
import struct, sys
from PIL import Image, ImageDraw, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
OUT = sys.argv[2] if len(sys.argv) > 2 else "game/ui/font.bin"
SIZE = 56
chars = [chr(c) for c in range(32, 127)] + list("áàâãäéêèëíìîïóòôõöúùûüçñÁÀÂÃÄÉÊÈËÍÌÎÏÓÒÔÕÖÚÙÛÜÇÑ°ºª×·•…–—♥☠←→↑↓")
font = ImageFont.truetype(FONT, SIZE)
asc, desc = font.getmetrics()
pad = 3
glyphs = []
x = y = pad; rowh = 0; W = 1024
items = []
for ch in chars:
    try:
        bbox = font.getbbox(ch)
    except Exception:
        continue
    if font.getmask(ch).getbbox() is None and ch != ' ':
        # glyph missing in font (e.g. symbols) -> skip
        continue
    l, t, r, b = bbox
    w, h = max(1, r - l), max(1, b - t)
    adv = int(round(font.getlength(ch)))
    if x + w + pad > W:
        x = pad; y += rowh + pad; rowh = 0
    items.append((ch, x, y, w, h, l, t, adv))
    x += w + pad; rowh = max(rowh, h)
H = y + rowh + pad
H = 1 << (H - 1).bit_length()
img = Image.new("L", (W, H), 0)
d = ImageDraw.Draw(img)
for ch, gx, gy, w, h, l, t, adv in items:
    d.text((gx - l, gy - t), ch, font=font, fill=255)
with open(OUT, "wb") as f:
    f.write(b"LFNT" + struct.pack("<HHHHH", W, H, asc + desc, asc, len(items)))
    for ch, gx, gy, w, h, l, t, adv in items:
        f.write(struct.pack("<IHHHHhhH", ord(ch), gx, gy, w, h, l, t, adv))
    f.write(img.tobytes())
print(f"{len(items)} glyphs, atlas {W}x{H}, line {asc+desc}")
img.save("/tmp/font_atlas.png")
