import sys, io
from docx import Document
from docx.table import Table
from docx.text.paragraph import Paragraph

path = sys.argv[1]
out = sys.argv[2]
doc = Document(path)

lines = []
body = doc.element.body
ti = 0
pi = 0
for child in body.iterchildren():
    tag = child.tag.split('}')[-1]
    if tag == 'p':
        p = Paragraph(child, doc)
        txt = p.text
        style = p.style.name if p.style is not None else ''
        # detect images
        imgs = child.findall('.//{http://schemas.openxmlformats.org/drawingml/2006/main}blip')
        has_pic = ' [IMG x%d]' % len(imgs) if imgs else ''
        sizes = []
        for r in p.runs:
            if r.font.size is not None:
                sizes.append(str(int(r.font.size.pt)))
            if r.bold:
                sizes.append('B')
        lines.append('P%03d|%-22s|%s|%s%s' % (pi, style[:22], txt, ','.join(sizes[:6]), has_pic))
        pi += 1
    elif tag == 'tbl':
        t = Table(child, doc)
        lines.append('=== TABLE %d  rows=%d cols=%d style=%s' % (ti, len(t.rows), len(t.columns), t.style.name if t.style else ''))
        for r in t.rows:
            cells = []
            for c in r.cells:
                cells.append(c.text.replace('\n', ' / '))
            lines.append('  | ' + ' | '.join(cells))
        lines.append('=== END TABLE %d' % ti)
        ti += 1

# sections info
for i, s in enumerate(doc.sections):
    lines.append('SECTION %d page=%s x %s margins L%s R%s T%s B%s' % (
        i, s.page_width, s.page_height, s.left_margin, s.right_margin, s.top_margin, s.bottom_margin))

with io.open(out, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
print('paras', pi, 'tables', ti)
