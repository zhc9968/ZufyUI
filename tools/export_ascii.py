#!/usr/bin/env python3
"""Export an ASCII-only copy of the ZufyUI source tree.

Why: some toolchains, editors, CI images and review tools mishandle UTF-8 BOM
or non-ASCII comments. The exported tree is plain ASCII, has no BOM, and still
keeps the runtime text by escaping non-ASCII string/char literals.

Usage (from the repository root):
    python tools/export_ascii.py                 # write dist/ascii
    python tools/export_ascii.py --force         # replace an existing export
    python tools/export_ascii.py --out <dir>
    python tools/export_ascii.py --strings ascii # unmapped text -> '?'
    python tools/export_ascii.py --map tools/ascii_map.json
    python tools/export_ascii.py --docs none     # omit English docs

The tool is intentionally ASCII-only itself, mirroring tools/*.ps1.
"""

import argparse
import json
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUT = os.path.join(ROOT, "dist", "ascii")
MARKER = ".ascii_export"

# Typographic / symbol folding used in comments and Markdown.
PUNCT = {
    "\u2014": "-", "\u2013": "-", "\u2015": "-",
    "\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"',
    "\u2026": "...", "\u00b7": ".", "\u2022": "*", "\u00a0": " ",
    "\u2192": "->", "\u2190": "<-", "\u2191": "^", "\u2193": "v",
    "\u2265": ">=", "\u2264": "<=", "\u2260": "!=", "\u2248": "~",
    "\u00d7": "x", "\u00f7": "/", "\u2229": "&", "\u00b1": "+/-",
    "\u00b0": "deg", "\u03b1": "alpha", "\u03b2": "beta",
    "\u03c0": "pi", "\u03c4": "tau", "\u0394": "delta",
    "\u2460": "(1)", "\u2461": "(2)", "\u2462": "(3)",
    "\u2463": "(4)", "\u2464": "(5)", "\u2465": "(6)",
    "\u2466": "(7)", "\u2467": "(8)", "\u2468": "(9)",
    "\u26a0": "!", "\u2705": "[x]", "\u274c": "[ ]",
    "\u2b50": "*", "\u2605": "*", "\u2606": "*",
    "\u00a9": "(c)", "\u00ae": "(R)", "\u2122": "(TM)",
    "\u00bd": "1/2", "\u00bc": "1/4", "\u00be": "3/4",
    "\u3000": " ",
}

SRC_CODE = [
    "ZufyUI.h", "ZufyUIWidgets.h", "ZDataViewer.h", "ZufyUICharts.h",
    "ZufyUIAcrylic.h", "ZufyUIImages.h", "ZufyUIIcons.h",
    "ZufyUIWindowTool.h", "ZufyUIDragDrop.h", "ZufyUIDsl.h",
    "ZufyUIHwndView.h", "ZufyUI.cpp", "pch.h", "pch.cpp", "resource.h",
]
SRC_PROJECT = [
    "ZufyUI.slnx", "ZufyUI.vcxproj", "ZufyUI.rc",
]
SRC_XML = ["ZufyUI.vcxproj.filters"]
SRC_TOOLS = [
    "tools/build.ps1", "tools/ensure_bom.ps1", "tools/run_dsl_tests.ps1",
    "tools/vs_tools.ps1",
]
SRC_TESTS = ["tests/dsl_tests.cpp"]
SRC_DEBUGGER = [
    "ZufyUIDebugger/pch.h", "ZufyUIDebugger/pch.cpp",
    "ZufyUIDebugger/ZufyUIDebugger.cpp", "ZufyUIDebugger/ZufyUIDebugger.vcxproj",
]
SRC_TEXT = ["LICENSE", ".gitattributes", ".gitignore"]
SRC_BIN = ["small.ico", "ZufyUI.ico"]
DOC_EN = [
    ("CHANGELOG.en.md", "CHANGELOG.md"),
    ("docs/src/API.en.md", "docs/API.md"),
]

README_TEMPLATE = """# ZufyUI - ASCII export

This directory is a generated, **ASCII-only** copy of ZufyUI {version}.
It is produced by `tools/export_ascii.py` from the UTF-8/BOM source tree.

## Why

Some toolchains, editors, CI images and code-review systems mishandle a UTF-8
BOM or non-ASCII comments. Every source file here is plain ASCII and has **no
BOM**, so it can be opened, diffed and compiled anywhere.

## What was changed

- C++ / RC / project / tool files: ASCII only.
- String and character literals: non-ASCII characters are preserved with C++
  universal-character-name escapes (`\\uXXXX` / `\\UXXXXXXXX`) for wide literals,
  and UTF-8 octal byte escapes for narrow / `u8` literals. Runtime text and
  font names are therefore unchanged.
- Comments: non-ASCII prose is removed (replaced by spaces); ASCII identifiers
  and English fragments remain.
- Documentation: English versions only (`docs/API.md`, `CHANGELOG.md`).
  Chinese docs are not included.
- `tools/ensure_bom.ps1` will not add BOMs: every file is pure ASCII.

## Build

Same layout as the main repository:

```powershell
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

Or open `ZufyUI.slnx` in Visual Studio. The export includes the in-repo
`ZufyUIDebugger` project.

## Regenerate

From the main repository:

```powershell
python tools/export_ascii.py --force
```

Options:
- `--strings ascii` replaces unmapped non-ASCII UI text with `?`.
- `--map tools/ascii_map.json` applies a JSON translation map first.
- `--docs none` omits the English docs.

## Report

See `EXPORT_REPORT.txt` for per-file statistics and the list of string
literals that were escaped. Add entries to the map to translate them.

## Packaging

The generated tree contains no build outputs. If you build inside it, delete
`x64/`, `Release/`, `*.pdb`, `*.obj`, `*.iobj`, `*.ipdb`, `*.user` and
`.vs/` before zipping or publishing it.

## Limitations

- Comments lose their Chinese prose; API signatures and English comments remain.
- UI strings keep their original language unless a translation map is given.
- Markdown docs are the English versions, ASCII-folded.
"""


class Stats:
    def __init__(self):
        self.files = 0
        self.escaped_strings = 0
        self.escaped_chars = 0
        self.comment_chars = 0
        self.code_chars = 0
        self.markdown_chars = 0
        self.unmapped = {}
        self.warnings = []

    def add_unmapped(self, text):
        text = re.sub(r"\s+", " ", text).strip()
        if not text:
            return
        if len(text) > 160:
            text = text[:157] + "..."
        self.unmapped[text] = self.unmapped.get(text, 0) + 1


def read_text(path):
    with open(path, "r", encoding="utf-8-sig", newline="") as f:
        return f.read()


def write_text(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="ascii", newline="") as f:
        f.write(text)


def load_map(path):
    if not path:
        path = os.path.join(ROOT, "tools", "ascii_map.json")
    if not os.path.exists(path):
        return {}
    with open(path, "r", encoding="utf-8-sig") as f:
        obj = json.load(f)
    if not isinstance(obj, dict):
        raise SystemExit("ascii map must be a JSON object")
    return {str(k): str(v) for k, v in obj.items()}


def apply_map(text, mapping):
    for key in sorted(mapping, key=len, reverse=True):
        if key:
            text = text.replace(key, mapping[key])
    return text


def wide_escape(cp):
    if cp <= 0xFFFF:
        return "\\u%04X" % cp
    return "\\U%08X" % cp


def narrow_escape(cp):
    # Octal is exactly three digits and cannot swallow the next character.
    return "".join("\\%03o" % b for b in chr(cp).encode("utf-8"))


def escape_body(body, prefix, mapping, strings_mode, stats):
    original = body
    body = apply_map(body, mapping)
    if any(ord(c) > 127 for c in body):
        stats.add_unmapped(original)
    wide = prefix in ("L", "u", "U")
    out = []
    for c in body:
        if ord(c) <= 127:
            out.append(c)
            continue
        stats.escaped_chars += 1
        if strings_mode == "ascii":
            out.append("?")
        elif wide:
            out.append(wide_escape(ord(c)))
        else:
            out.append(narrow_escape(ord(c)))
    return "".join(out)


def fold_comment(text, stats):
    out = []
    for c in text:
        if ord(c) <= 127:
            out.append(c)
        elif c in PUNCT:
            out.append(PUNCT[c])
        else:
            out.append(" ")
            stats.comment_chars += 1
    return "".join(out)


def fold_markdown(text, stats):
    out = []
    for c in text:
        if ord(c) <= 127:
            out.append(c)
        elif c in PUNCT:
            out.append(PUNCT[c])
        else:
            out.append("?")
            stats.markdown_chars += 1
    return "".join(out)


def fold_plain(text, stats):
    out = []
    for c in text:
        if ord(c) <= 127:
            out.append(c)
        elif c in PUNCT:
            out.append(PUNCT[c])
        else:
            out.append("?")
            stats.markdown_chars += 1
    return "".join(out)


def process_source(text, mapping, strings_mode, stats):
    out = []
    i = 0
    n = len(text)
    while i < n:
        # Line comment
        if text.startswith("//", i):
            j = text.find("\n", i)
            if j < 0:
                j = n
            out.append(fold_comment(text[i:j], stats))
            i = j
            continue

        # Block comment
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            if j < 0:
                j = n
                out.append(fold_comment(text[i:j], stats))
                i = n
            else:
                out.append(fold_comment(text[i:j + 2], stats))
                i = j + 2
            continue

        # Raw string literal
        m = re.match(r'(u8|L|u|U)?R"', text[i:])
        if m:
            prefix = m.group(1) or ""
            after = i + len(m.group(0))
            k = text.find("(", after)
            if k < 0:
                out.append(text[i])
                i += 1
                continue
            delim = text[after:k]
            close = ")" + delim + '"'
            end = text.find(close, k + 1)
            if end < 0:
                end = n
            out.append(text[i:k + 1])
            body = text[k + 1:end]
            stats.warnings.append("raw string contains non-ASCII; replaced with '?'")
            out.append("".join(c if ord(c) <= 127 else "?" for c in body))
            if end < n:
                out.append(close)
                i = end + len(close)
            else:
                i = n
            continue

        # Normal string literal
        m = re.match(r'(u8|L|u|U)?"', text[i:])
        if m:
            prefix = m.group(1) or ""
            head = m.group(0)
            j = i + len(head)
            body = []
            while j < n:
                c = text[j]
                if c == "\\":
                    body.append(text[j:j + 2])
                    j += 2
                    continue
                if c == '"':
                    break
                body.append(c)
                j += 1
            out.append(head)
            out.append(escape_body("".join(body), prefix, mapping, strings_mode, stats))
            if j < n:
                out.append('"')
                j += 1
            i = j
            continue

        # Wide char literal (L'x', u'x', U'x'); digit separators are untouched.
        m = re.match(r"(L|u|U)'", text[i:])
        if m:
            prefix = m.group(1)
            head = m.group(0)
            j = i + len(head)
            body = []
            while j < n:
                c = text[j]
                if c == "\\":
                    body.append(text[j:j + 2])
                    j += 2
                    continue
                if c == "'":
                    break
                body.append(c)
                j += 1
            out.append(head)
            out.append(escape_body("".join(body), prefix, mapping, strings_mode, stats))
            if j < n:
                out.append("'")
                j += 1
            i = j
            continue

        c = text[i]
        if ord(c) > 127:
            stats.code_chars += 1
            out.append("?")
        else:
            out.append(c)
        i += 1
    return "".join(out)


def read_version():
    path = os.path.join(ROOT, "ZufyUI.h")
    try:
        with open(path, "r", encoding="utf-8-sig") as f:
            m = re.search(r'ZufyUI_VERSION_STRING\s+L"([^"]+)"', f.read())
            if m:
                return m.group(1)
    except OSError:
        pass
    return "unknown"


def prepare_out(out, force):
    if os.path.exists(out) and os.listdir(out):
        if not force and not os.path.exists(os.path.join(out, MARKER)):
            raise SystemExit(
                "output directory is not empty and was not created by this tool:\n"
                "  %s\npass --force to replace it" % out)
        shutil.rmtree(out)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, MARKER), "w", encoding="ascii") as f:
        f.write("generated by tools/export_ascii.py\n")


def copy_binary(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)


def copy_source(rel, out, mapping, strings_mode, stats):
    src = os.path.join(ROOT, rel)
    if not os.path.exists(src):
        return False
    dst = os.path.join(out, rel)
    text = read_text(src)
    text = process_source(text, mapping, strings_mode, stats)
    write_text(dst, text)
    stats.files += 1
    return True


def copy_text_mapped(rel, out, mapping, stats):
    src = os.path.join(ROOT, rel)
    if not os.path.exists(src):
        return False
    dst = os.path.join(out, rel)
    text = apply_map(read_text(src), mapping)
    text = fold_plain(text, stats)
    write_text(dst, text)
    stats.files += 1
    return True


def copy_markdown(src_rel, dst_rel, out, stats):
    src = os.path.join(ROOT, src_rel)
    if not os.path.exists(src):
        return False
    dst = os.path.join(out, dst_rel)
    text = fold_markdown(read_text(src), stats)
    text = text.replace("../../README.en.md", "../README.md")
    text = text.replace("../../README.md", "../README.md")
    write_text(dst, text)
    stats.files += 1
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description="Export an ASCII-only ZufyUI tree.")
    ap.add_argument("--out", default=DEFAULT_OUT, help="output directory")
    ap.add_argument("--force", action="store_true", help="replace an existing export")
    ap.add_argument("--strings", choices=("escape", "ascii"), default="escape",
                    help="escape non-ASCII literals (default) or replace with '?'")
    ap.add_argument("--map", default="", help="JSON translation map")
    ap.add_argument("--docs", choices=("english", "none"), default="english",
                    help="include English docs (default) or none")
    args = ap.parse_args(argv)

    out = os.path.abspath(args.out)
    mapping = load_map(args.map)
    stats = Stats()
    prepare_out(out, args.force)

    for rel in SRC_CODE + SRC_PROJECT + SRC_TOOLS + SRC_TESTS + SRC_DEBUGGER:
        copy_source(rel, out, mapping, args.strings, stats)

    for rel in SRC_XML:
        copy_text_mapped(rel, out, mapping, stats)

    for rel in SRC_TEXT:
        src = os.path.join(ROOT, rel)
        if os.path.exists(src):
            dst = os.path.join(out, rel)
            text = read_text(src)
            text = text if all(ord(c) <= 127 for c in text) else fold_plain(text, stats)
            write_text(dst, text)
            stats.files += 1

    for rel in SRC_BIN:
        src = os.path.join(ROOT, rel)
        if os.path.exists(src):
            copy_binary(src, os.path.join(out, rel))
            stats.files += 1

    if args.docs == "english":
        for src_rel, dst_rel in DOC_EN:
            copy_markdown(src_rel, dst_rel, out, stats)
        images = os.path.join(ROOT, "docs", "images")
        if os.path.isdir(images):
            shutil.copytree(images, os.path.join(out, "docs", "images"))
            stats.files += 1

    readme = README_TEMPLATE.format(version=read_version())
    write_text(os.path.join(out, "README.md"), readme)
    stats.files += 1

    # Verify: every text file must be ASCII and BOM-free.
    bad = []
    for dirpath, _, names in os.walk(out):
        for name in names:
            path = os.path.join(dirpath, name)
            if name in (MARKER,) or name.lower().endswith(
                    (".ico", ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp")):
                continue
            with open(path, "rb") as f:
                data = f.read()
            if data.startswith(b"\xef\xbb\xbf"):
                bad.append((path, "BOM"))
            try:
                data.decode("ascii")
            except UnicodeDecodeError:
                bad.append((path, "non-ASCII"))
    if bad:
        for path, why in bad[:20]:
            print("FAIL %s: %s" % (path, why), file=sys.stderr)
        raise SystemExit(1)

    report = []
    report.append("ZufyUI ASCII export report")
    report.append("version: %s" % read_version())
    report.append("output: %s" % out)
    report.append("mode: strings=%s docs=%s" % (args.strings, args.docs))
    report.append("files written: %d" % stats.files)
    report.append("escaped non-ASCII string chars: %d" % stats.escaped_chars)
    report.append("removed non-ASCII comment chars: %d" % stats.comment_chars)
    report.append("folded non-ASCII markdown chars: %d" % stats.markdown_chars)
    report.append("non-ASCII outside literals/comments: %d" % stats.code_chars)
    report.append("")
    report.append("Unmapped string literals (escaped in the output; non-ASCII shown as \\uXXXX):")
    if stats.unmapped:
        for text, count in sorted(stats.unmapped.items(), key=lambda kv: (-kv[1], kv[0])):
            report.append("  [%dx] %s" % (count, text))
    else:
        report.append("  (none)")
    if stats.warnings:
        report.append("")
        report.append("Warnings:")
        report.extend("  " + w for w in stats.warnings)
    report_text = "\n".join(report) + "\n"
    report_text = "".join(c if ord(c) <= 127 else wide_escape(ord(c)) for c in report_text)
    write_text(os.path.join(out, "EXPORT_REPORT.txt"), report_text)

    print("ASCII export written to %s" % out)
    print("  files: %d, escaped chars: %d, unmapped literals: %d"
          % (stats.files, stats.escaped_chars, len(stats.unmapped)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
