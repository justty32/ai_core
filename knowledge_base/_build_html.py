#!/usr/bin/env python3
"""把 knowledge_base/ 內所有 .md 渲染成共用樣式的 HTML。

一次性建置腳本。需 .venv（markdown-it-py）；產出為自包含靜態頁（僅依賴同層 kb.css）。
用法：.venv/bin/python knowledge_base/_build_html.py
"""
from __future__ import annotations
import re
import html
from pathlib import Path
from markdown_it import MarkdownIt

KB = Path(__file__).resolve().parent

# 權威層 / 分類查表（與 index.html 的 DATA 一致）
LEVEL = {
    "00_INDEX": 1,
    "note_01": 1, "note_02": 3, "note_03": 3, "note_04": 3, "note_05": 3,
    "note_06": 1, "note_07": 5, "note_08": 5, "note_09": 5,
    "doc_01": 1, "doc_02": 1, "doc_03": 1, "doc_04": 1, "doc_05": 1,
    "doc_06": 1, "doc_07": 1, "doc_08": 1,
    "doc_10": 4, "doc_11": 4, "doc_12": 4, "doc_13": 4, "doc_14": 4,
    "doc_15": 4, "doc_16": 4, "doc_17": 4, "doc_20": 2, "doc_21": 2, "doc_22": 1, "doc_30": 4,
    "code_01": 1, "code_02": 2, "code_03": 2, "code_04": 2,
}
LVL_LABEL = {1: "① 定案", 2: "② 原型", 3: "③ 歷史", 4: "④ 舊架構", 5: "⑤ 舊世系"}
LVL_COLOR = {1: "--l1", 2: "--l2", 3: "--l3", 4: "--l4", 5: "--l5"}
CAT_LABEL = {"note": "note 筆記想法", "doc": "doc 文檔規範", "code": "code 程式成果", "00": "INDEX 總索引"}


def stem_key(name: str) -> str:
    """note_01_vision_and_origin -> note_01 ; 00_INDEX -> 00_INDEX"""
    m = re.match(r"((?:note|doc|code)_\d+)", name)
    if m:
        return m.group(1)
    return name


def category(name: str) -> str:
    if name.startswith("note_"):
        return "note"
    if name.startswith("doc_"):
        return "doc"
    if name.startswith("code_"):
        return "code"
    return "00"


def first_h1(text: str, fallback: str) -> str:
    for line in text.splitlines():
        if line.startswith("# "):
            return line[2:].strip()
    return fallback


def rewrite_links(rendered: str, md_names: set[str]) -> str:
    """把指向同層 .md（已渲染）的 href 改成 .html；../ 等其他連結保留。"""
    def repl(m):
        href = m.group(1)
        # 只處理「無斜線、以 .md 結尾（可帶 #anchor）」且在渲染集合內者
        mm = re.match(r"^([^/#]+)\.md(#.*)?$", href)
        if mm and mm.group(1) in md_names:
            return f'href="{mm.group(1)}.html{mm.group(2) or ""}"'
        return m.group(0)
    return re.sub(r'href="([^"]+)"', repl, rendered)


PAGE = """<!DOCTYPE html>
<html lang="zh-Hant">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>{title} · ai_core 知識庫</title>
<link rel="stylesheet" href="kb.css">
</head>
<body>
<nav>
  <a class="brand" href="index.html">ai_core KB</a>
  <a href="index.html">← 導覽首頁</a>
  <a href="00_INDEX.html">總索引</a>
  <span class="spacer"></span>
  <span class="tag cat-{cat}">{cat_label}</span>
  <span class="lvl-tag" style="color:var({lcolor});background:rgba(0,0,0,.25)">{llabel}</span>
</nav>
<main class="md">
  <div class="filemeta">{fname} · {lines} 行</div>
{body}
</main>
<footer class="pager">
{prev}
{next}
</footer>
<a class="backtop" href="#">↑ 回頂端</a>
</body>
</html>
"""


def main():
    md = MarkdownIt("gfm-like").disable("linkify")
    files = sorted(p for p in KB.glob("*.md") if p.name != "MEMORY.md")
    names = {p.stem for p in files}

    order = [p.stem for p in files]  # 排序後的閱讀序，供 prev/next

    for i, p in enumerate(files):
        text = p.read_text(encoding="utf-8")
        lines = text.count("\n") + 1
        key = stem_key(p.stem)
        lvl = LEVEL.get(key, 1)
        cat = category(p.stem)
        title = first_h1(text, p.stem)

        body = md.render(text)
        body = rewrite_links(body, names)

        def pager_link(idx, direction):
            if idx < 0 or idx >= len(files):
                return '<span class="ph"></span>'
            nb = files[idx].stem
            cls = "next" if direction == "next" else "prev"
            arrow = "下一篇 →" if direction == "next" else "← 上一篇"
            return (f'<a class="{cls}" href="{nb}.html">'
                    f'<span class="dir">{arrow}</span>'
                    f'<span class="fn">{html.escape(nb)}</span></a>')

        page = PAGE.format(
            title=html.escape(title),
            cat=cat,
            cat_label=CAT_LABEL[cat],
            lcolor=LVL_COLOR[lvl],
            llabel=LVL_LABEL[lvl],
            fname=html.escape(p.name),
            lines=lines,
            body=body,
            prev=pager_link(i - 1, "prev"),
            next=pager_link(i + 1, "next"),
        )
        out = p.with_suffix(".html")
        out.write_text(page, encoding="utf-8")
        print(f"  ✓ {out.name}")

    print(f"\n渲染完成：{len(files)} 個頁面 → {KB}")


if __name__ == "__main__":
    main()
