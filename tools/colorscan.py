#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
colorscan.py —— 扫描源码里的硬编码颜色，按“色差”聚类，供主题令牌化参考。

用法:
    python tools/colorscan.py [根目录] [--thresh 12]

识别:
    Color::FromArgb(a, r, g, b)        # 0..255 / 0..255
    D2D1::ColorF(r, g, b, a)           # 0..1 浮点（也识别 D2D1::ColorF(0xRRGGBB, a)）
输出:
    每个基色簇: 代表色(#RRGGBB) + 出现次数 + 若干出处(file:line)
"""
import os
import re
import sys
import glob
from collections import defaultdict

ROOT = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "."
THRESH = 12.0
if "--thresh" in sys.argv:
    THRESH = float(sys.argv[sys.argv.index("--thresh") + 1])

RE_FROMARGB = re.compile(r"Color::FromArgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)")
RE_COLORF_F = re.compile(r"D2D1::ColorF\s*\(\s*([\d.]+)f?\s*,\s*([\d.]+)f?\s*,\s*([\d.]+)f?\s*(?:,\s*([\d.]+)f?\s*)?\)")
RE_HEX = re.compile(r"0x([0-9A-Fa-f]{8})\b")


def clamp255(x):
    return 0 if x < 0 else (255 if x > 255 else int(round(x)))


def add(samples, rgb, a, loc):
    key = (clamp255(rgb[0]), clamp255(rgb[1]), clamp255(rgb[2]), clamp255(a))
    samples[key].append(loc)


def scan_file(path, samples):
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except OSError:
        return
    for i, line in enumerate(lines, 1):
        for m in RE_FROMARGB.finditer(line):
            a, r, g, b = (int(m.group(k)) for k in (1, 2, 3, 4))
            add(samples, (r, g, b), a, f"{path}:{i}")
        for m in RE_COLORF_F.finditer(line):
            r, g, b = (float(m.group(k)) for k in (1, 2, 3))
            a = float(m.group(4)) if m.group(4) else 1.0
            add(samples, (r * 255, g * 255, b * 255), a * 255, f"{path}:{i}")
        for m in RE_HEX.finditer(line):
            v = int(m.group(1), 16)
            add(samples, ((v >> 16) & 255, (v >> 8) & 255, v & 255), (v >> 24) & 255, f"{path}:{i}")


def dist2(c1, c2):
    dr, dg, db = c1[0] - c2[0], c1[1] - c2[1], c1[2] - c2[2]
    return (dr * dr + dg * dg + db * db) ** 0.5


def hexs(rgb):
    return "#{:02X}{:02X}{:02X}".format(int(rgb[0]), int(rgb[1]), int(rgb[2]))


def main():
    samples = defaultdict(list)
    exts = ("*.h", "*.hpp", "*.cpp", "*.cc")
    for ext in exts:
        for p in glob.glob(os.path.join(ROOT, "**", ext), recursive=True):
            if os.sep + "tools" + os.sep in p:
                continue
            scan_file(p, samples)

    # 按 alpha 分成两组（不透明 / 半透明），组内按 RGB 距离聚类
    groups = {">=250": [], "<250": []}
    for key, locs in samples.items():
        groups[">=250" if key[3] >= 250 else "<250"].append((key, locs))

    total = sum(len(v) for _, v in samples.items())
    print(f"扫描到硬编码颜色出现 {total} 次，唯一色 {len(samples)} 个（阈值 ΔRGB={THRESH}）\n")

    for gname, items in groups.items():
        if not items:
            continue
        items.sort(key=lambda kv: -len(kv[1]))
        clusters = []  # [ [repKey, count, locs], ... ]
        for key, locs in items:
            for cl in clusters:
                if dist2(key, cl[0]) <= THRESH:
                    cl[1] += len(locs)
                    cl[2].extend(locs[:3] if len(cl[2]) < 6 else [])
                    break
            else:
                clusters.append([key, len(locs), list(locs[:3])])
        clusters.sort(key=lambda c: -c[1])
        print(f"== alpha {gname}：{len(items)} 色 → {len(clusters)} 簇 ==")
        for rep, cnt, locs in clusters:
            print(f"  {hexs(rep)}  x{cnt}   {', '.join(locs)}")
        print()

    print("提示：把高频簇映射到 ThemeRole（见 ZufyUI.h 的 ThemeRole），其余可用 ThemeManager::SetColorMap 做色差重映射。")


if __name__ == "__main__":
    main()
