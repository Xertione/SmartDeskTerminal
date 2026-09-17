# -*- coding: utf-8 -*-
"""体检 doc/ 下所有 PDF：页数、可提取文本量、图片数，判断是否扫描件。"""
import os, sys, io, glob, json
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
import pymupdf

ROOT = r"C:\Users\Jodio\Desktop\codexproject\workbuddyproject\SmartDeskTerminal\doc"

results = []
for dirpath, dirnames, filenames in os.walk(ROOT):
    for fn in filenames:
        if not fn.lower().endswith('.pdf'):
            continue
        p = os.path.join(dirpath, fn)
        rel = os.path.relpath(p, ROOT)
        rec = {'path': rel, 'abs': p, 'size': os.path.getsize(p)}
        try:
            doc = pymupdf.open(p)
            rec['pages'] = doc.page_count
            total_chars = 0
            total_imgs = 0
            pages_with_text = 0
            per_page = []
            for i in range(doc.page_count):
                pg = doc.load_page(i)
                t = pg.get_text().strip()
                total_chars += len(t)
                if len(t) > 20:
                    pages_with_text += 1
                total_imgs += len(pg.get_images(full=True))
                per_page.append(len(t))
            rec['chars'] = total_chars
            rec['pages_with_text'] = pages_with_text
            rec['imgs'] = total_imgs
            rec['chars_per_page_avg'] = round(total_chars / max(doc.page_count, 1), 1)
            rec['per_page'] = per_page
            # 抽一段样本判断语言
            sample = ""
            for i in range(min(doc.page_count, 6)):
                t = doc.load_page(i).get_text().strip()
                if t:
                    sample += t[:1200]
                if len(sample) > 3000:
                    break
            rec['sample'] = sample[:2500]
            rec['verdict'] = ('TEXT' if rec['chars_per_page_avg'] > 200 else
                              ('MIXED' if rec['chars'] > 300 else 'SCAN/IMAGE'))
            doc.close()
        except Exception as e:
            rec['error'] = str(e)
            rec['verdict'] = 'ERROR'
        results.append(rec)

print("=" * 100)
for r in results:
    print(f"[{r['verdict']:10}] {r['size']/1024/1024:6.2f}MB  pages={r.get('pages','?'):>5}  "
          f"chars={r.get('chars',0):>8}  avg/pg={r.get('chars_per_page_avg',0):>8}  imgs={r.get('imgs','?')}")
    print(f"            {r['path']}")
    if r.get('error'):
        print(f"            ERROR: {r['error']}")
print("=" * 100)

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pdf_probe.json'), 'w', encoding='utf-8') as f:
    json.dump(results, f, ensure_ascii=False, indent=1)
print("saved pdf_probe.json")
