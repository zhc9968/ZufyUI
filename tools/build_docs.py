#!/usr/bin/env python3
"""Build the ZufyUI API HTML site.

This is the ZufyUI-specific wrapper around the generic md2html converter:
- reads the version from ZufyUI.h and passes it as --var ZUFYUI_VERSION=...
- supplies the project favicon
- maps docs/src/API(.en).md to docs/API(.en).html

The generic converter lives outside this repository and knows nothing about
ZufyUI. Run from the repository root:

    python tools/build_docs.py
    python tools/build_docs.py --md2html ../md2html/mdapi2html.py
    python tools/build_docs.py --lang zh

md2html lookup order: --md2html, %MD2HTML%, ../md2html/mdapi2html.py.
"""

import argparse
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_version():
    path = os.path.join(ROOT, "ZufyUI.h")
    with open(path, "r", encoding="utf-8-sig") as f:
        m = re.search(r'ZufyUI_VERSION_STRING\s+L"([^"]+)"', f.read())
    if not m:
        raise SystemExit("ZufyUI_VERSION_STRING not found in ZufyUI.h")
    return m.group(1)


def find_md2html(explicit):
    candidates = [
        explicit,
        os.environ.get("MD2HTML", ""),
        os.path.join(ROOT, "..", "md2html", "mdapi2html.py"),
    ]
    for path in candidates:
        if path and os.path.isfile(path):
            return os.path.abspath(path)
    raise SystemExit(
        "md2html not found. Pass --md2html <path>, set %MD2HTML%, "
        "or place it at ../md2html/mdapi2html.py"
    )


def main():
    ap = argparse.ArgumentParser(description="Build the ZufyUI API HTML site.")
    ap.add_argument("--md2html", default="", help="path to mdapi2html.py")
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "docs"),
                    help="output directory (default: docs)")
    ap.add_argument("--icon", default=os.path.join(ROOT, "docs", "icon.ico"),
                    help="favicon for the generated pages")
    ap.add_argument("--lang", choices=("zh", "en", "both"), default="both")
    args = ap.parse_args()

    md2html = find_md2html(args.md2html)
    version = read_version()
    jobs = []
    if args.lang in ("zh", "both"):
        jobs.append(("docs/src/API.md", "API.html", "zh"))
    if args.lang in ("en", "both"):
        jobs.append(("docs/src/API.en.md", "API.en.html", "en"))

    icon = os.path.abspath(args.icon)
    if not os.path.isfile(icon):
        print("warning: icon not found, generating without favicon: %s" % icon,
              file=sys.stderr)
        icon = ""

    out_dir = os.path.abspath(args.out_dir)
    os.makedirs(out_dir, exist_ok=True)

    for src_rel, out_name, lang in jobs:
        src = os.path.join(ROOT, src_rel)
        if not os.path.isfile(src):
            raise SystemExit("missing source: %s" % src)
        dst = os.path.join(out_dir, out_name)
        cmd = [
            sys.executable, md2html, src, dst,
            "--toc-depth", "3", "--lang", lang,
            "--var", "ZUFYUI_VERSION=" + version,
        ]
        if icon:
            cmd += ["--icon", icon]
        print(" ".join(cmd))
        subprocess.check_call(cmd)

    print("docs built: version %s -> %s" % (version, out_dir))


if __name__ == "__main__":
    main()
