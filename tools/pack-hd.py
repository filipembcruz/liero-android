#!/usr/bin/env python3
"""Builds game/ui/hd.bin (HD sprites, background and terrain textures) from the
source sheets in art/source/ (flat magenta #FF00FF background).

  python3 tools/pack-hd.py [--preview out.png]

hd.bin format (little endian):
  'LHD1' u16 count
  count x { u8 nameLen, name, u16 w, u16 h, i16 ax, i16 ay, u32 zlen, zlib(RGBA w*h) }
(ax, ay) = anchor/pivot in pixels (feet for worm frames, grip for guns, centre otherwise).
"""
import struct, sys, zlib, os
from collections import deque
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "art", "source")
OUT = os.path.join(ROOT, "game", "ui", "hd.bin")


def key_mask(im, shadows=False):
    """Returns RGBA with magenta (and optionally magenta-tinted shadows) removed.
    Only pixels touching the removed background are de-fringed, so purple/pink
    artwork keeps its colours."""
    im = im.convert("RGB")
    W, H = im.size
    src = im.load()
    out = Image.new("RGBA", (W, H))
    dst = out.load()
    keyed = bytearray(W * H)
    for y in range(H):
        for x in range(W):
            r, g, b = src[x, y]
            mn = min(r, b)
            pure = r > 190 and b > 190 and g < 90
            shadow = shadows and abs(r - b) < 45 and mn > 40 and g < mn * 0.45
            if pure or shadow:
                keyed[y * W + x] = 1
                dst[x, y] = (0, 0, 0, 0)
            else:
                dst[x, y] = (r, g, b, 255)
    for y in range(H):
        for x in range(W):
            if keyed[y * W + x]:
                continue
            edge = any(0 <= nx < W and 0 <= ny < H and keyed[ny * W + nx]
                       for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
            if not edge:
                continue
            r, g, b, a = dst[x, y]
            m = min(r, b) - g
            if m > 40 and abs(r - b) < 60:
                k = min(1.0, m / 200.0)
                dst[x, y] = (max(0, int(r - m * 0.8)), g, max(0, int(b - m * 0.8)), int(255 * (1 - k * 0.85)))
    return out


def components(img, min_area=1):
    """Connected components of opaque pixels: list of (area, bbox, pixels set)."""
    W, H = img.size
    a = img.getchannel("A").load()
    seen = bytearray(W * H)
    comps = []
    for y in range(H):
        for x in range(W):
            if a[x, y] < 40 or seen[y * W + x]:
                continue
            q = deque([(x, y)]); seen[y * W + x] = 1
            pts = []
            while q:
                cx, cy = q.popleft(); pts.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < W and 0 <= ny < H and not seen[ny * W + nx] and a[nx, ny] >= 40:
                        seen[ny * W + nx] = 1; q.append((nx, ny))
            if len(pts) >= min_area:
                xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
                comps.append((len(pts), (min(xs), min(ys), max(xs) + 1, max(ys) + 1), pts))
    return comps


def keep_only(img, keep):
    """Clear every opaque pixel not in the given component list."""
    W, H = img.size
    allowed = set()
    for c in keep:
        allowed.update(c[2])
    px = img.load()
    for y in range(H):
        for x in range(W):
            if px[x, y][3] and (x, y) not in allowed:
                px[x, y] = (0, 0, 0, 0)
    return img


def grid_cells(img, cols, rows):
    W, H = img.size
    cw, ch = W / cols, H / rows
    return [img.crop((int(i * cw), int(j * ch), int((i + 1) * cw), int((j + 1) * ch)))
            for j in range(rows) for i in range(cols)]


def by_centroid(img, cols, rows, min_area=40):
    """Groups components by the grid cell containing their centroid (objects may
    overflow their cell)."""
    W, H = img.size
    cw, ch = W / cols, H / rows
    groups = [[] for _ in range(cols * rows)]
    for c in components(img, min_area):
        x0, y0, x1, y1 = c[1]
        cx = sum(p[0] for p in c[2]) / c[0]; cy = sum(p[1] for p in c[2]) / c[0]
        i = min(cols - 1, int(cx / cw)); j = min(rows - 1, int(cy / ch))
        groups[j * cols + i].append(c)
    cells = []
    for g in groups:
        if not g:
            cells.append(None); continue
        x0 = min(c[1][0] for c in g); y0 = min(c[1][1] for c in g)
        x1 = max(c[1][2] for c in g); y1 = max(c[1][3] for c in g)
        sub = Image.new("RGBA", (x1 - x0, y1 - y0))
        sp = sub.load(); ip = img.load()
        for c in g:
            for (x, y) in c[2]:
                sp[x - x0, y - y0] = ip[x, y]
        cells.append(sub)
    return cells


def trim(img):
    bb = img.getbbox()
    return img.crop(bb) if bb else img


def fit(img, maxdim):
    w, h = img.size
    k = min(1.0, maxdim / max(w, h))
    if k < 1.0:
        img = img.resize((max(1, round(w * k)), max(1, round(h * k))), Image.LANCZOS)
    return img


def seamless(tex, size):
    """Make a texture tile seamlessly (offset + cross-fade) and resize."""
    tex = tex.convert("RGB").resize((size, size), Image.LANCZOS)
    w, h = tex.size
    off = Image.new("RGB", (w, h))
    off.paste(tex.crop((w // 2, 0, w, h)), (0, 0)); off.paste(tex.crop((0, 0, w // 2, h)), (w - w // 2, 0))
    off2 = Image.new("RGB", (w, h))
    off2.paste(off.crop((0, h // 2, w, h)), (0, 0)); off2.paste(off.crop((0, 0, w, h // 2)), (0, h - h // 2))
    # mask: 1 near the original centre, 0 near original edges
    mask = Image.new("L", (w, h))
    mp = mask.load()
    for y in range(h):
        for x in range(w):
            dx = min(x, w - 1 - x) / (w / 2); dy = min(y, h - 1 - y) / (h / 2)
            mp[x, y] = int(255 * max(0.0, min(1.0, min(dx, dy) * 3.0)))
    # original edges -> offset image (whose seams are in the middle and covered)
    return Image.composite(tex, off2, mask).convert("RGBA")


def pack(src, out, skin=False):
    entries = []

    def add(name, img, anchor=None):
        img = img.convert("RGBA")
        w, h = img.size
        ax, ay = anchor if anchor else (w // 2, h // 2)
        entries.append((name, img, int(ax), int(ay)))

    # --- worm (4x3 grid): body frames + guns + hand
    worm = key_mask(Image.open(os.path.join(src, "worm_sheet.png")), shadows=True)
    cells = grid_cells(worm, 4, 3)
    frames = []
    names = ["worm_idle", "worm_walk1", "worm_walk2", "worm_walk3", "worm_walk4", "worm_jump", "worm_hurt", "worm_rope",
             "gun_bazooka", "gun_mgun", "worm_hand", None]
    for c, n in zip(cells, names):
        if not n or (skin and n in ("gun_bazooka", "gun_mgun")):
            continue
        comps = components(c, 30)
        if not comps:
            continue
        comps.sort(key=lambda t: -t[0])
        if n == "worm_hurt":
            keep = [k for k in comps if k[0] > 150]
        else:
            keep = [comps[0]] + [k for k in comps[1:] if k[0] > comps[0][0] * 0.15]
        c = trim(keep_only(c, keep))
        if n.startswith("worm_"):
            frames.append((n, c))
        else:
            c = fit(c, 180)
            w, h = c.size
            add(n, c, (w * 0.33, h * 0.55))

    # one scale for every character frame (from the idle pose), so the
    # character never changes size between animation frames
    idle = dict(frames).get("worm_idle")
    ks = min(1.0, 170 / max(idle.size)) if idle else 1.0
    for n, c in frames:
        w, h = c.size
        c = c.resize((max(1, round(w * ks)), max(1, round(h * ks))), Image.LANCZOS)
        w, h = c.size
        add(n, c, (w * 0.5, h - 2))

    # --- guns (4x2)
    guns = key_mask(Image.open(os.path.join(src, "guns_sheet.png")))
    gnames = ["gun_pistol", "gun_rifle", "gun_shotgun", "gun_minigun", "gun_flamer", "gun_glauncher", "gun_laser", "gun_cannon"]
    for c, n in zip(by_centroid(guns, 4, 2, 60), gnames):
        if c is None:
            continue
        c = fit(trim(c), 180)
        w, h = c.size
        add(n, c, (w * 0.32, h * 0.6))
        # skins: the worm sheet's weapon cells may show the character holding
        # them, so bazooka / machine gun come from this sheet instead
        if skin and n == "gun_cannon":
            add("gun_bazooka", c, (w * 0.32, h * 0.6))
        if skin and n == "gun_minigun":
            add("gun_mgun", c, (w * 0.32, h * 0.6))

    # --- fx (6x3)
    fx = key_mask(Image.open(os.path.join(src, "fx_sheet.png")))
    fnames = ["p_rocket", "p_grenade", "p_mine", "p_mine_lit", "p_cluster", "p_bignuke",
              "exp1", "exp2", "exp3", "exp4", "exp5", "exp6",
              "smoke1", "smoke2", "smoke3", "bonus_health", "bonus_weapon", "spark"]
    for c, n in zip(by_centroid(fx, 6, 3, 20), fnames):
        if c is None:
            continue
        c = trim(c)
        c = fit(c, 200 if n.startswith(("exp", "smoke")) else 140)
        add(n, c)

    # --- projectiles (6x4)
    pr = key_mask(Image.open(os.path.join(src, "projectiles_sheet.png")))
    pnames = ["p_missile", "p_minirocket", "p_hellraider", "p_cannonball", "p_dart", "p_dynamite",
              "p_banana", "p_spikeball", "p_greenball", "p_larpa", "p_zimm", "p_crackler",
              "p_floatmine", "p_bouncymine", "p_boobytrap", "p_grasshopper", "p_mininuke", "p_doomsday",
              "p_fireball", "p_flame1", "p_flame2", "p_dirt", "p_blaster", "p_shell"]
    for c, n in zip(by_centroid(pr, 6, 4, 20), pnames):
        if c is None:
            print("missing", n); continue
        add(n, fit(trim(c), 140))

    # --- touch action buttons (3x2), base set only
    if not skin:
        bt = key_mask(Image.open(os.path.join(src, "buttons_sheet.png")))
        for c, n in zip(by_centroid(bt, 3, 2, 60), ["btn_aim", "btn_swap", "btn_dig", "btn_jump", "btn_fire", "btn_rope"]):
            if c is not None:
                add(n, fit(trim(c), 256))

    # --- background + textures (opaque)
    bg = Image.open(os.path.join(src, "background.png" if skin else "cave_background.png")).convert("RGB")
    bg = bg.resize((1280, int(1280 * bg.height / bg.width)), Image.LANCZOS)
    add("bg_cave", bg.convert("RGBA"))
    tx = Image.open(os.path.join(src, "textures.png")).convert("RGB")
    W, H = tx.size
    add("tex_dirt", seamless(tx.crop((0, 0, W // 2, H)), 384))
    add("tex_rock", seamless(tx.crop((W // 2, 0, W, H)), 384))

    with open(out, "wb") as f:
        f.write(b"LHD1" + struct.pack("<H", len(entries)))
        for name, img, ax, ay in entries:
            raw = img.tobytes()
            z = zlib.compress(raw, 9)
            nb = name.encode()
            f.write(struct.pack("<B", len(nb)) + nb + struct.pack("<HHhhI", img.width, img.height, ax, ay, len(z)) + z)
    print(f"{len(entries)} entries -> {out} ({os.path.getsize(out) // 1024} KB)")

    if "--preview" in sys.argv:
        pv = sys.argv[sys.argv.index("--preview") + 1]
        pv = pv if not skin else pv.replace(".png", "_" + os.path.basename(src) + ".png")
        cols = 10; cell = 200
        rows = (len(entries) + cols - 1) // cols
        sheet = Image.new("RGBA", (cols * cell, rows * cell), (40, 30, 70, 255))
        for k, (name, img, ax, ay) in enumerate(entries):
            t = img.copy(); t.thumbnail((cell - 10, cell - 10))
            sheet.alpha_composite(t, ((k % cols) * cell + 5, (k // cols) * cell + 5))
        sheet.convert("RGB").save(pv)


SKINS = ("mole", "army", "bot")  # Toupeiras, Soldados, Robôs -> game/ui/hd_<id>.bin
SKIN_FILES = ("worm_sheet.png", "guns_sheet.png", "projectiles_sheet.png", "fx_sheet.png", "background.png", "textures.png")


def main():
    only = [a for a in sys.argv[1:] if a in ("base",) + SKINS]
    if not only or "base" in only:
        pack(SRC, OUT)
    for sk in SKINS:
        if only and sk not in only:
            continue
        d = os.path.join(SRC, "skins", sk)
        missing = [f for f in SKIN_FILES if not os.path.exists(os.path.join(d, f))]
        if missing:
            print("skin", sk, "missing:", ", ".join(missing)); continue
        pack(d, os.path.join(ROOT, "game", "ui", "hd_" + sk + ".bin"), skin=True)


if __name__ == "__main__":
    main()
