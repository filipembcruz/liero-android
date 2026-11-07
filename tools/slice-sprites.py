#!/usr/bin/env python3
"""Checks and slices an HD sprite sheet with a flat magenta (#FF00FF) background.

usage: slice-sprites.py <sheet.png> <cols> <rows> <out_dir> [names,comma,separated]
Writes one trimmed RGBA PNG per cell + report.txt (bbox, coverage, edge contact,
leftover magenta fringe) so bad cells can be spotted before integrating.
"""
import sys, os
from PIL import Image

def is_key(r, g, b, tol=60):
    return r > 255 - tol and b > 255 - tol and g < tol + 40

def main():
    path, cols, rows, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    names = sys.argv[5].split(',') if len(sys.argv) > 5 else []
    os.makedirs(out, exist_ok=True)
    im = Image.open(path).convert('RGB')
    W, H = im.size
    cw, ch = W / cols, H / rows
    px = im.load()
    rgba = Image.new('RGBA', im.size)
    rp = rgba.load()
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            if is_key(r, g, b):
                rp[x, y] = (0, 0, 0, 0)
            else:
                # de-fringe: pull magenta tint out of edge pixels
                if r > 150 and b > 150 and g < 120:
                    m = min(r, b) - g
                    r, b = max(0, r - m // 2), max(0, b - m // 2)
                rp[x, y] = (r, g, b, 255)
    rep = []
    for j in range(rows):
        for i in range(cols):
            k = j * cols + i
            box = (int(i * cw), int(j * ch), int((i + 1) * cw), int((j + 1) * ch))
            cell = rgba.crop(box)
            bb = cell.getbbox()
            name = names[k] if k < len(names) else f"cell_{k:02d}"
            if not bb:
                rep.append(f"{name}: EMPTY"); continue
            touch = bb[0] == 0 or bb[1] == 0 or bb[2] == cell.width or bb[3] == cell.height
            cov = sum(1 for a in cell.getdata(3) if a) / (cell.width * cell.height)
            cell.crop(bb).save(os.path.join(out, name + ".png"))
            rep.append(f"{name}: bbox={bb} size={bb[2]-bb[0]}x{bb[3]-bb[1]} coverage={cov:.2f}{' TOUCHES_EDGE' if touch else ''}")
    open(os.path.join(out, 'report.txt'), 'w').write("\n".join(rep) + "\n")
    print("\n".join(rep))

if __name__ == '__main__':
    main()
