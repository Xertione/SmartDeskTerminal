# -*- coding: utf-8 -*-
"""把 doc/ 下的 PDF 全文导出为 txt，并把图片型 PDF 渲染为 PNG。"""
import os, sys, io, json
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
import pymupdf

ROOT = r"C:\Users\Jodio\Desktop\codexproject\workbuddyproject\SmartDeskTerminal\doc"
OUT = r"C:\Users\Jodio\Desktop\codexproject\workbuddyproject\SmartDeskTerminal\.workbuddy\tmp\dump"
os.makedirs(OUT, exist_ok=True)

targets = []
for dirpath, dirnames, filenames in os.walk(ROOT):
    for fn in filenames:
        if fn.lower().endswith('.pdf'):
            targets.append(os.path.join(dirpath, fn))

def safe(name):
    return "".join(c if (c.isalnum() or c in '._-') else '_' for c in name)

for p in targets:
    rel = os.path.relpath(p, ROOT)
    base = safe(os.path.splitext(os.path.basename(p))[0]) + "_" + str(abs(hash(rel)) % 100000)
    doc = pymupdf.open(p)
    # 1) 全文文本
    txt_path = os.path.join(OUT, base + ".txt")
    with open(txt_path, 'w', encoding='utf-8') as f:
        for i in range(doc.page_count):
            t = doc.load_page(i).get_text()
            f.write(f"\n<<<PAGE {i+1}>>>\n{t}")
    # 2) 若为图片型 PDF（平均字符<200），渲染每页为 PNG
    total = sum(len(doc.load_page(i).get_text().strip()) for i in range(doc.page_count))
    avg = total / max(doc.page_count, 1)
    rendered = []
    if avg < 200 and doc.page_count <= 30:
        for i in range(doc.page_count):
            pg = doc.load_page(i)
            pix = pg.get_pixmap(matrix=pymupdf.Matrix(3, 3))
            pp = os.path.join(OUT, f"{base}_p{i+1}.png")
            pix.save(pp)
            rendered.append(pp)
    print(f"{'RENDER' if rendered else 'TEXT  '}  avg={avg:7.1f}  {rel}")
    print(f"          -> {txt_path}")
    for r in rendered:
        print(f"          -> {r}")
    doc.close()
print("\nDONE ->", OUT)
