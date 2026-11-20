#!/usr/bin/env python3
"""Builds the Google Play phone screenshots (1920x1080) for Tunnel Clash:
a character line-up plus one captioned gameplay shot per look, each with the
character drawn large so players see it is not only worms.

Inputs: art/source/**/worm_sheet.png (4x3 grid on magenta) and the CI
gameplay screenshots (desktop/1920x1080/*.png from ci-screenshots-store),
passed with --shots DIR.  Output: docs/store/screenshots/*.png
"""
import argparse, os
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageEnhance
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
W, H = 1920, 1080
SHEETS = {
    "worm": "art/source/worm_sheet.png",
    "mole": "art/source/skins/mole/worm_sheet.png",
    "army": "art/source/skins/army/worm_sheet.png",
    "bot": "art/source/skins/bot/worm_sheet.png",
}


def key(img):
    """Magenta chroma key -> RGBA with soft edges and de-spill."""
    a = np.asarray(img.convert("RGB")).astype(np.float32)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mag = np.minimum(r, b) - g          # high on magenta
    alpha = np.clip((150 - mag) / 70.0, 0, 1)
    spill = np.clip(np.minimum(r, b) - g - 20, 0, None) * (alpha < 1)
    r2 = r - spill * 0.6
    b2 = b - spill * 0.6
    out = np.dstack([r2, g, b2, alpha * 255]).clip(0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def frame(name, col, row):
    sheet = Image.open(os.path.join(ROOT, SHEETS[name]))
    cw, ch = sheet.width / 4, sheet.height / 3
    cell = sheet.crop((int(col * cw), int(row * ch), int((col + 1) * cw), int((row + 1) * ch)))
    rgba = key(cell)
    return rgba.crop(rgba.getbbox())


def fit_h(img, h):
    return img.resize((max(1, round(img.width * h / img.height)), h), Image.LANCZOS)


def shadowed(img, blur=18, off=(10, 14), alpha=150):
    pad = blur * 3
    canvas = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    sh = Image.new("RGBA", img.size, (0, 0, 0, alpha))
    sh.putalpha(img.getchannel("A").point(lambda v: v * alpha // 255))
    canvas.alpha_composite(sh, (pad + off[0], pad + off[1]))
    canvas = canvas.filter(ImageFilter.GaussianBlur(blur))
    canvas.alpha_composite(img, (pad, pad))
    return canvas, pad


def text(d, xy, s, size, fill=(255, 255, 255), anchor="mm", stroke=8):
    f = ImageFont.truetype(FONT, size)
    d.text(xy, s, font=f, fill=fill, anchor=anchor, stroke_width=stroke, stroke_fill=(30, 12, 45))


def backdrop(path, dim=0.55, blur=0):
    bg = Image.open(path).convert("RGB")
    s = max(W / bg.width, H / bg.height)
    bg = bg.resize((round(bg.width * s), round(bg.height * s)), Image.LANCZOS)
    bg = bg.crop(((bg.width - W) // 2, (bg.height - H) // 2, (bg.width - W) // 2 + W, (bg.height - H) // 2 + H))
    if blur:
        bg = bg.filter(ImageFilter.GaussianBlur(blur))
    return ImageEnhance.Brightness(bg).enhance(dim).convert("RGBA")


def lineup(out):
    im = backdrop(os.path.join(ROOT, "art/source/cave_background.png"), 0.45)
    d = ImageDraw.Draw(im)
    text(d, (W // 2, 120), "PICK YOUR FIGHTER", 104, (255, 214, 90))
    text(d, (W // 2, 215), "4 hand-drawn looks + the classic pixel style", 44, stroke=6)
    names = [("worm", "WIGGLER"), ("mole", "MOLE"), ("army", "SOLDIER"), ("bot", "ROBOT")]
    slot = W // 4
    for i, (k, label) in enumerate(names):
        f = fit_h(frame(k, 0, 0), 470)
        s, pad = shadowed(f)
        cx = slot * i + slot // 2
        im.alpha_composite(s, (cx - f.width // 2 - pad, 860 - f.height - pad))
        # pedestal
        d.ellipse((cx - 170, 845, cx + 170, 885), fill=(0, 0, 0, 90))
        text(d, (cx, 950), label, 64, (255, 255, 255))
    im.convert("RGB").save(out, quality=92)


def gameplay(shot, char, pose, title, sub, out, side="right"):
    g = Image.open(shot).convert("RGBA")
    im = backdrop(shot, 0.35, blur=24)
    # framed gameplay, slightly smaller, under the caption
    gw = 1340
    gs = g.resize((gw, round(g.height * gw / g.width)), Image.LANCZOS)
    gx = 70 if side == "right" else W - gw - 70
    gy = H - gs.height - 50
    border = Image.new("RGBA", (gs.width + 16, gs.height + 16), (255, 214, 90, 255))
    bs, pad = shadowed(border, 20, (0, 12), 170)
    im.alpha_composite(bs, (gx - 8 - pad, gy - 8 - pad))
    im.alpha_composite(gs, (gx, gy))
    d = ImageDraw.Draw(im)
    tx = gx + gw // 2
    text(d, (tx, 92), title, 92, (255, 214, 90))
    text(d, (tx, 178), sub, 40, stroke=6)
    f = fit_h(frame(char, *pose), 640)
    s, pad = shadowed(f, 22, (14, 18), 170)
    fx = W - f.width - 30 if side == "right" else 30
    im.alpha_composite(s, (fx - pad, H - f.height - 20 - pad))
    im.convert("RGB").save(out, quality=92)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shots", required=True, help="dir with CI 1920x1080 gameplay screenshots")
    ap.add_argument("--out", default=os.path.join(ROOT, "docs/store/screenshots"))
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    S = lambda n: os.path.join(a.shots, n + ".png")
    lineup(os.path.join(a.out, "01_fighters.jpg"))
    gameplay(S("04_firing"), "worm", (1, 1), "40 WEAPONS. ALL CHAOS.", "Bazookas, mines, lasers, nukes and the banana bomb",
             os.path.join(a.out, "02_worms.jpg"), "right")
    gameplay(S("13_skin_toupeiras"), "mole", (1, 0), "MOLES DIG DEEPER", "Tunnel through fully destructible maps",
             os.path.join(a.out, "03_moles.jpg"), "left")
    gameplay(S("14_skin_soldados"), "army", (2, 0), "SOLDIERS ON THE MOVE", "Fast 2D tunnel deathmatch against the CPU",
             os.path.join(a.out, "04_soldiers.jpg"), "right")
    gameplay(S("15_skin_robos"), "bot", (1, 1), "ROBOTS NEVER QUIT", "10 map themes, touch or gamepad controls",
             os.path.join(a.out, "05_robots.jpg"), "left")
    gameplay(S("06_ninja_rope"), "worm", (3, 1), "SWING, DIG, SURVIVE", "Ninja rope and fully destructible terrain",
             os.path.join(a.out, "06_rope.jpg"), "right")


if __name__ == "__main__":
    main()
