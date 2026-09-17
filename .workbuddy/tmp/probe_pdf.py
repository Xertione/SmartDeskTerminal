# -*- coding: utf-8 -*-
"""探测 doc/ 下所有 PDF：页数、是否含文本层、每页字符数。不猜测，只报告事实。"""
import os, sys, json
import pymupdf

DOC = r"C:\Users\Jodio\Desktop\codexproject\workbuddyproject\SmartDeskTerminal\doc"

targets = []
for root, dirs, files in os.walk(DOC):
    for f in files:
        if f.lower().endswith(".pdf"):
            targets.append(os.path.join(root, f))
targets.sort()

report = []
for path in targets:
    rel = os.path.relpath(path, DOC)
    item = {"path": rel, "size_kb": round(os.path.getsize(path) / 1024, 1)}
    try:
        doc = pymupdf.open(path)
        item["pages"] = doc.page_count
        item["encrypted"] = doc.is_encrypted
        per_page = []
        total_chars = 0
        img_pages = 0
        for i, page in enumerate(doc):
            txt = page.get_text("text")
            n = len(txt.strip())
            total_chars += n
            per_page.append(n)
            if n < 20 and page.get_images():
                img_pages += 1
        item["total_text_chars"] = total_chars
        item["avg_chars_per_page"] = round(total_chars / max(doc.page_count, 1))
        item["min_page_chars"] = min(per_page) if per_page else 0
        item["max_page_chars"] = max(per_page) if per_page else 0
        item["pages_without_text_but_with_images"] = img_pages
        # 判定
        if total_chars < 100:
            item["verdict"] = "SCANNED_ONLY"      # 纯扫描件，无文本层
        elif item["avg_chars_per_page"] < 50:
            item["verdict"] = "MOSTLY_IMAGE"      # 大部分是图
        else:
            item["verdict"] = "HAS_TEXT"          # 有文本层可提取
        doc.close()
    except Exception as e:
        item["error"] = f"{type(e).__name__}: {e}"
        item["verdict"] = "ERROR"
    report.append(item)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pdf_report.json")
with open(out, "w", encoding="utf-8") as fh:
    json.dump(report, fh, ensure_ascii=False, indent=2)

for it in report:
    print(f"[{it.get('verdict','?'):<15}] pages={it.get('pages','-'):<5} "
          f"chars={it.get('total_text_chars','-'):<8} avg={it.get('avg_chars_per_page','-'):<6} "
          f"{it['path']}")
print("\nJSON ->", out)
