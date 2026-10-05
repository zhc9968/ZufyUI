#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_api_parity.py — 校验中英两版 API 文档的“代码块”是否一致。

两重校验（都按 `###chapter:` 分章对比）：
  1) 方法名集合一致      —— 免疫换行/合并/注释差异（抓“漏抄某函数”）。
  2) 代码块非空行数相同  —— “行数不匹配就是漏了”（抓多行声明块：枚举/重载/字段块）。

用法：
    python tools/check_api_parity.py [docs/API.md] [docs/API.en.md]
退出码：0 = 一致；1 = 有差异。
"""
import re
import sys
import os
import io

CH_ZH = os.path.join("docs", "API.md")
CH_EN = os.path.join("docs", "API.en.md")


def read(path):
    with io.open(path, "r", encoding="utf-8") as f:
        return f.read()


def split_chapters(text):
    return re.split(r"^###chapter:.*$", text, flags=re.M)


def chapter_names(text):
    return re.findall(r"^###chapter:\s*(.*)$", text, flags=re.M)


def code_blocks(text):
    return re.findall(r"```[^\n]*\n(.*?)```", text, re.S)


def code_methods(text):
    toks = set()
    for body in code_blocks(text):
        body = re.sub(r"//[^\n]*", "", body)
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        for t in re.finditer(r"([A-Za-z_]\w+)\s*\(", body):
            toks.add(t.group(1))
    return toks


def code_line_count(text):
    n = 0
    for body in code_blocks(text):
        n += sum(1 for ln in body.splitlines() if ln.strip())
    return n


def main():
    zh_path = sys.argv[1] if len(sys.argv) > 1 else CH_ZH
    en_path = sys.argv[2] if len(sys.argv) > 2 else CH_EN
    zh_text, en_text = read(zh_path), read(en_path)

    zh_ch, en_ch = split_chapters(zh_text), split_chapters(en_text)
    zh_names, en_names = chapter_names(zh_text), chapter_names(en_text)

    n = min(len(zh_ch), len(en_ch))
    problems = 0
    for i in range(1, n):
        title = zh_names[i - 1] if i - 1 < len(zh_names) else f"chapter#{i}"
        a, b = code_methods(zh_ch[i]), code_methods(en_ch[i])
        only_zh, only_en = sorted(a - b), sorted(b - a)
        la, lb = code_line_count(zh_ch[i]), code_line_count(en_ch[i])
        if only_zh or only_en or la != lb:
            problems += 1
            print(f"[!] chapter {i}: {title}")
            if only_zh:
                print("    only in zh (missing in en): " + ", ".join(only_zh))
            if only_en:
                print("    only in en (missing in zh): " + ", ".join(only_en))
            if la != lb:
                print(f"    code-block line count differs: zh={la} en={lb}")

    if problems == 0:
        print("OK: API code blocks match across zh/en chapters (methods + line counts).")
        return 0
    print(f"\n{problems} chapter(s) differ.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
