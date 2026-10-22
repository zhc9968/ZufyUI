#!/usr/bin/env python3
"""Generate the draft ZufyUI icon (PNG + multi-size ICO + SVG).

This is a proposal, not the final brand asset. Run from the repository root:

    python tools/make_icon.py

Outputs docs/icon.png, docs/icon.ico, docs/icon.svg and the application icons
ZufyUI.ico / small.ico.
"""

import os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs")

S = 1024


def lerp(a, b, t):
    return int(a + (b - a) * t)


def check_connected(im):
    """Fail if the white Z is not a single connected component."""
    from collections import deque

    w, h = im.size
    rgb = im.convert("RGB").load()
    mask = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b = rgb[x, y]
            mask[y][x] = r >= 235 and g >= 235 and b >= 235

    seen = set()
    best = None
    for y in range(h):
        for x in range(w):
            if not mask[y][x] or (x, y) in seen:
                continue
            q = deque([(x, y)])
            seen.add((x, y))
            n = 0
            minx = maxx = x
            miny = maxy = y
            while q:
                cx, cy = q.popleft()
                n += 1
                minx = min(minx, cx)
                maxx = max(maxx, cx)
                miny = min(miny, cy)
                maxy = max(maxy, cy)
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < w and 0 <= ny < h and mask[ny][nx] and (nx, ny) not in seen:
                        seen.add((nx, ny))
                        q.append((nx, ny))
            if best is None or n > best[0]:
                best = (n, minx, miny, maxx, maxy)

    if not best:
        raise AssertionError("no white Z pixels found")
    n, minx, miny, maxx, maxy = best
    # The Z is the largest white component and must span both horizontal bars.
    if miny > 120 or maxy < 150 or minx > 90 or maxx < 160:
        raise AssertionError(
            "Z strokes are not connected: bbox=(%d,%d)-(%d,%d), pixels=%d"
            % (minx, miny, maxx, maxy, n))
    print("icon check: Z strokes connected, bbox=(%d,%d)-(%d,%d), pixels=%d"
          % (minx, miny, maxx, maxy, n))


def main():
    os.makedirs(OUT, exist_ok=True)

    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))

    # Soft drop shadow.
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle([80, 92, 944, 956], radius=210, fill=(15, 23, 42, 95))
    shadow = shadow.filter(ImageFilter.GaussianBlur(28))
    img = Image.alpha_composite(img, shadow)

    # Vertical blue -> violet gradient, clipped to a squircle.
    grad = Image.new("RGBA", (S, S))
    gd = ImageDraw.Draw(grad)
    for y in range(S):
        t = y / (S - 1)
        gd.line([(0, y), (S, y)], fill=(lerp(37, 124, t), lerp(99, 58, t), lerp(235, 237, t), 255))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([96, 96, 928, 928], radius=200, fill=255)
    img.paste(grad, (0, 0), mask)

    d = ImageDraw.Draw(img)
    # Inner highlight.
    d.rounded_rectangle([112, 112, 912, 912], radius=184,
                        outline=(255, 255, 255, 45), width=6)

    # Geometric Z on its own layer: the diagonal overlaps both bars (no gap
    # at the rounded corner) and the whole glyph tilts 4 degrees clockwise.
    glyph = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glyph)
    white = (255, 255, 255, 255)
    gd.rounded_rectangle([260, 270, 704, 380], radius=34, fill=white)
    gd.rounded_rectangle([322, 644, 764, 754], radius=34, fill=white)
    # Diagonal stroke: same thickness as the original, with round caps tucked
    # under the bar ends (no gap, no end overhang).
    p1 = (672, 340)
    p2 = (354, 684)
    diag_w = 65
    gd.line([p1, p2], fill=white, width=diag_w)
    r = diag_w // 2
    for p in (p1, p2):
        gd.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=white)

    # Geometry check: both cap centres must sit inside their bars, so the
    # union is connected (no gap at either joint) before and after rotation.
    assert 260 + r <= p1[0] <= 704 - r, p1
    assert 270 + r <= p1[1] <= 380 - r, p1
    assert 322 + r <= p2[0] <= 764 - r, p2
    assert 644 + r <= p2[1] <= 754 - r, p2
    for p in (p1, p2, ((p1[0] + p2[0]) // 2, (p1[1] + p2[1]) // 2)):
        assert glyph.getpixel(p)[3] == 255, p

    glyph = glyph.rotate(-2, resample=Image.BICUBIC, center=(512, 512))
    img = Image.alpha_composite(img, glyph)

    # Accent dot (the "UI" status light); kept upright.
    d = ImageDraw.Draw(img)
    d.ellipse([700, 170, 790, 260], fill=(34, 211, 238, 255))

    img = img.resize((256, 256), Image.LANCZOS)
    check_connected(img)
    img.save(os.path.join(OUT, "icon.png"))
    img.save(os.path.join(OUT, "icon.ico"), sizes=[
        (16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)
    ])
    # Application icons embedded by ZufyUI.rc (ZufyUI.ico) and listed in the
    # project (small.ico).
    img.save(os.path.join(ROOT, "ZufyUI.ico"), sizes=[
        (16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)
    ])
    img.save(os.path.join(ROOT, "small.ico"), sizes=[
        (16, 16), (24, 24), (32, 32), (48, 48)
    ])

    svg = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2563EB"/>
      <stop offset="1" stop-color="#7C3AED"/>
    </linearGradient>
  </defs>
  <rect x="24" y="24" width="208" height="208" rx="50" fill="url(#g)"/>
  <rect x="28" y="28" width="200" height="200" rx="46" fill="none"
        stroke="#FFFFFF" stroke-opacity=".18" stroke-width="1.5"/>
  <g transform="rotate(2 128 128)">
    <rect x="65" y="67.5" width="111" height="27.5" rx="8.5" fill="#FFFFFF"/>
    <rect x="80.5" y="161" width="110.5" height="27.5" rx="8.5" fill="#FFFFFF"/>
    <line x1="168" y1="85" x2="88.5" y2="171"
          stroke="#FFFFFF" stroke-width="16.25" stroke-linecap="round"/>
  </g>
  <circle cx="186" cy="54" r="11" fill="#22D3EE"/>
</svg>
"""
    with open(os.path.join(OUT, "icon.svg"), "w", encoding="ascii", newline="") as f:
        f.write(svg)

    print("wrote", os.path.join(OUT, "icon.png"))
    print("wrote", os.path.join(OUT, "icon.ico"))
    print("wrote", os.path.join(OUT, "icon.svg"))
    print("wrote", os.path.join(ROOT, "ZufyUI.ico"))
    print("wrote", os.path.join(ROOT, "small.ico"))


if __name__ == "__main__":
    main()
