# -*- coding: utf-8 -*-
"""把 Markdown 项目书转换成语雀文档格式（.lake）。

语雀的单篇 .lake 文件本质是 HTML：文件头固定为 <!doctype lake>，正文由带
data-lake-id 的 HTML 块构成。本脚本按该结构生成，使文件可被语雀编辑器识别。
图片用原生 <img> 标签，其余块级元素带唯一 lake id。
"""
import html
import os
import re

SRC = os.path.join(os.path.dirname(__file__), "..", "docs", "项目书.md")
DST = os.path.join(os.path.dirname(__file__), "..", "docs", "项目书.lake")

HEADER = ('<!doctype lake><meta name="doc-version" content="1" />'
          '<meta name="viewport" content="fixed" />')

_counter = [0]


def lake_id():
    """生成语雀风格的唯一 id：u 加 8 位十六进制。"""
    _counter[0] += 1
    return "u%08x" % (0xA0000000 + _counter[0])


def wrap(tag, inner, attrs=""):
    """给块级元素加上 data-lake-id 与 id。"""
    ident = lake_id()
    return '<%s data-lake-id="%s" id="%s"%s>%s</%s>' % (
        tag, ident, ident, attrs, inner, tag)


def inline(text):
    """处理行内 Markdown：图片、加粗、行内代码、链接。"""
    text = html.escape(text, quote=False)
    # 图片：语雀可接受原生 img，用相对路径指向 docs/screenshots
    text = re.sub(r'!\[([^\]]*)\]\(([^)]+)\)',
                  lambda m: '<img src="%s" alt="%s" />' % (m.group(2), m.group(1)), text)
    text = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', text)
    return text


def is_table_sep(line):
    return bool(re.match(r'^\s*\|?[\s:\-|]+\|?\s*$', line)) and '-' in line


def split_row(line):
    line = line.strip()
    if line.startswith('|'):
        line = line[1:]
    if line.endswith('|'):
        line = line[:-1]
    return [c.strip() for c in line.split('|')]


def convert(md):
    lines = md.splitlines()
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if not stripped:
            i += 1
            continue

        # 标题
        m = re.match(r'^(#{1,6})\s+(.*)$', stripped)
        if m:
            level = len(m.group(1))
            out.append(wrap('h%d' % level, inline(m.group(2))))
            i += 1
            continue

        # 表格
        if stripped.startswith('|') and i + 1 < len(lines) and is_table_sep(lines[i + 1]):
            header = split_row(stripped)
            i += 2
            rows = []
            while i < len(lines) and lines[i].strip().startswith('|'):
                rows.append(split_row(lines[i]))
                i += 1
            thead = ''.join(wrap('th', inline(c)) for c in header)
            body = ''
            for row in rows:
                cells = ''.join(wrap('td', inline(c)) for c in row)
                body += wrap('tr', cells)
            table = ('<table class="lake-table" margin="true">'
                     + wrap('thead', thead) + wrap('tbody', body) + '</table>')
            out.append(table)
            continue

        # 分割线
        if re.match(r'^(-{3,}|\*{3,}|_{3,})$', stripped):
            out.append('<hr data-lake-id="%s" id="%s" />' % (lake_id(), lake_id()))
            i += 1
            continue

        # 代码块
        if stripped.startswith('```'):
            i += 1
            code = []
            while i < len(lines) and not lines[i].strip().startswith('```'):
                code.append(lines[i])
                i += 1
            i += 1
            out.append(wrap('pre', html.escape('\n'.join(code))))
            continue

        # 无序列表
        if re.match(r'^\s*[-*]\s+', line):
            items = []
            while i < len(lines) and re.match(r'^\s*[-*]\s+', lines[i]):
                items.append(wrap('li', inline(re.sub(r'^\s*[-*]\s+', '', lines[i]))))
                i += 1
            out.append('<ul list="%s">%s</ul>' % (lake_id(), ''.join(items)))
            continue

        # 有序列表
        if re.match(r'^\s*\d+\.\s+', line):
            items = []
            while i < len(lines) and re.match(r'^\s*\d+\.\s+', lines[i]):
                items.append(wrap('li', inline(re.sub(r'^\s*\d+\.\s+', '', lines[i]))))
                i += 1
            out.append('<ol list="%s">%s</ol>' % (lake_id(), ''.join(items)))
            continue

        # 引用
        if stripped.startswith('>'):
            quote = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                quote.append(re.sub(r'^\s*>\s?', '', lines[i]))
                i += 1
            out.append(wrap('blockquote', inline(' '.join(quote))))
            continue

        # 只有图片的一行，直接作为块输出，避免被段落包住
        if re.match(r'^!\[[^\]]*\]\([^)]+\)$', stripped):
            out.append(wrap('p', inline(stripped)))
            i += 1
            continue

        # 普通段落：合并到下一个空行或下一个块开始
        para = [stripped]
        i += 1
        while i < len(lines):
            nxt = lines[i].strip()
            if (not nxt or re.match(r'^(#{1,6})\s|^\s*[-*]\s+|^\s*\d+\.\s+|^>|^\|', nxt)
                    or nxt.startswith('```')):
                break
            para.append(nxt)
            i += 1
        out.append(wrap('p', inline(' '.join(para))))

    return HEADER + '\n' + '\n'.join(out) + '\n'


def main():
    with open(SRC, encoding='utf-8') as f:
        md = f.read()
    lake = convert(md)
    with open(DST, 'w', encoding='utf-8', newline='\n') as f:
        f.write(lake)
    print('saved', os.path.abspath(DST))
    print('bytes', len(lake.encode('utf-8')), 'lake-ids', lake.count('data-lake-id'))


if __name__ == '__main__':
    main()
