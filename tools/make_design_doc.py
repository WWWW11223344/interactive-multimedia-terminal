# -*- coding: utf-8 -*-
"""Build the design document (设计方案) for the touch terminal project.

Output: docs/交互式多媒体终端项目设计方案.docx
Style follows the course template: cover with document-number table, revision table,
table of contents, then seven numbered chapters with tables and figures.
"""
import os

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.oxml import OxmlElement, parse_xml
from docx.oxml.ns import nsdecls, qn
from docx.shared import Cm, Pt, RGBColor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs")
FIG = os.path.join(DOCS, "figures")
SHOT = os.path.join(DOCS, "screenshots")
OUT = os.path.join(DOCS, "交互式多媒体终端项目设计方案.docx")

INK = RGBColor(0x1A, 0x1A, 0x1A)
GREY = RGBColor(0x59, 0x59, 0x59)
TEAL = RGBColor(0x1D, 0x6B, 0x66)
HEI = "黑体"
SONG = "宋体"
LATIN = "Times New Roman"
TEXT_W = 14.6  # cm, page width minus margins

# OOXML requires a fixed child order inside tblPr / pPr; appending at the end makes Word
# repair the table and silently drop the column widths, so insert before the tail tags.
TBLPR_TAIL = ("w:shd", "w:tblLayout", "w:tblCellMar", "w:tblLook",
              "w:tblCaption", "w:tblDescription")
PPR_TAIL = ("w:shd", "w:tabs", "w:suppressAutoHyphens", "w:kinsoku", "w:wordWrap",
            "w:overflowPunct", "w:topLinePunct", "w:autoSpaceDE", "w:autoSpaceDN",
            "w:bidi", "w:adjustRightInd", "w:snapToGrid", "w:spacing", "w:ind",
            "w:contextualSpacing", "w:mirrorIndents", "w:suppressOverlap", "w:jc",
            "w:textDirection", "w:textAlignment", "w:textboxTightWrap",
            "w:outlineLvl", "w:divId", "w:cnfStyle", "w:rPr", "w:sectPr", "w:pPrChange")


# ---------------------------------------------------------------- low level helpers
def _east_asia(element, name):
    rpr = element.get_or_add_rPr()
    fonts = rpr.find(qn("w:rFonts"))
    if fonts is None:
        fonts = OxmlElement("w:rFonts")
        rpr.append(fonts)
    fonts.set(qn("w:eastAsia"), name)
    fonts.set(qn("w:ascii"), LATIN)
    fonts.set(qn("w:hAnsi"), LATIN)


def shade(cell, hex_color):
    tcpr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), hex_color)
    tcpr.append(shd)


def cell_margins(table, top=60, bottom=60, left=90, right=90):
    tblpr = table._tbl.tblPr
    mar = OxmlElement("w:tblCellMar")
    for tag, val in (("top", top), ("left", left), ("bottom", bottom), ("right", right)):
        node = OxmlElement("w:" + tag)
        node.set(qn("w:w"), str(val))
        node.set(qn("w:type"), "dxa")
        mar.append(node)
    tblpr.insert_element_before(mar, *TBLPR_TAIL[2:])


def borders(table, color="A6A6A6", size=6):
    tblpr = table._tbl.tblPr
    bdr = OxmlElement("w:tblBorders")
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        node = OxmlElement("w:" + edge)
        node.set(qn("w:val"), "single")
        node.set(qn("w:sz"), str(size))
        node.set(qn("w:space"), "0")
        node.set(qn("w:color"), color)
        bdr.append(node)
    tblpr.insert_element_before(bdr, *TBLPR_TAIL)


def repeat_header(row):
    trpr = row._tr.get_or_add_trPr()
    node = OxmlElement("w:tblHeader")
    node.set(qn("w:val"), "true")
    trpr.append(node)


def add_field(paragraph, instr, cache=""):
    run = paragraph.add_run()
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    run._r.append(begin)
    text = OxmlElement("w:instrText")
    text.set(qn("xml:space"), "preserve")
    text.text = instr
    run._r.append(text)
    sep = OxmlElement("w:fldChar")
    sep.set(qn("w:fldCharType"), "separate")
    run._r.append(sep)
    if cache:
        shown = OxmlElement("w:t")
        shown.text = cache
        run._r.append(shown)
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.append(end)
    return run


def hrule(paragraph, color="B7C4BF", size=6):
    ppr = paragraph._p.get_or_add_pPr()
    bdr = OxmlElement("w:pBdr")
    bottom = OxmlElement("w:bottom")
    bottom.set(qn("w:val"), "single")
    bottom.set(qn("w:sz"), str(size))
    bottom.set(qn("w:space"), "4")
    bottom.set(qn("w:color"), color)
    bdr.append(bottom)
    ppr.insert_element_before(bdr, *PPR_TAIL)


# Word creates TOC 1/2/3 on the first field update; predefining them keeps the line
# spacing and hierarchy under our control instead of inheriting 1.5 spacing from Normal.
TOC_STYLE = """<w:style %s w:type="paragraph" w:styleId="%s">
  <w:name w:val="toc %s"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/>
  <w:uiPriority w:val="39"/><w:unhideWhenUsed/>
  <w:pPr><w:spacing w:before="%d" w:after="%d" w:line="240" w:lineRule="auto"/>
  <w:ind w:left="%d" w:firstLine="0"/><w:jc w:val="left"/></w:pPr>
  <w:rPr><w:rFonts w:ascii="Times New Roman" w:hAnsi="Times New Roman" w:eastAsia="%s"/>
  <w:sz w:val="%d"/><w:szCs w:val="%d"/>%s</w:rPr></w:style>"""


def add_toc_styles(doc):
    # 目录保留三级，但把行距压紧：Word 更新域后会在目录末尾补一个空段落，
    # 目录若是正好排满一页，这个空段落就单独占一页，前面会出现一张空白页。
    specs = [
        ("TOC1", "1", 120, 20, 0, HEI, 22, "<w:b/>"),
        ("TOC2", "2", 0, 0, 340, SONG, 19, ""),
        ("TOC3", "3", 0, 0, 700, SONG, 19, ""),
    ]
    for sid, level, before, after, left, font, size, bold in specs:
        xml = TOC_STYLE % (nsdecls("w"), sid, level, before, after, left, font, size, size, bold)
        doc.styles.element.append(parse_xml(xml))


# ---------------------------------------------------------------- document styles
def build_styles(doc):
    normal = doc.styles["Normal"]
    normal.font.name = LATIN
    normal.font.size = Pt(12)
    normal.font.color.rgb = INK
    _east_asia(normal.element, SONG)
    pf = normal.paragraph_format
    pf.line_spacing_rule = WD_LINE_SPACING.MULTIPLE
    pf.line_spacing = 1.5
    pf.space_before = Pt(0)
    pf.space_after = Pt(3)
    pf.first_line_indent = Pt(24)
    pf.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY

    specs = {
        "Heading 1": (16, 18, 12),
        "Heading 2": (13.5, 12, 6),
        "Heading 3": (12, 9, 4),
    }
    for name, (size, before, after) in specs.items():
        st = doc.styles[name]
        st.font.name = LATIN
        st.font.size = Pt(size)
        st.font.bold = True
        st.font.color.rgb = INK
        _east_asia(st.element, HEI)
        st.paragraph_format.space_before = Pt(before)
        st.paragraph_format.space_after = Pt(after)
        st.paragraph_format.first_line_indent = Pt(0)
        st.paragraph_format.line_spacing = 1.25
        st.paragraph_format.keep_with_next = True

    # caption style: figure / table titles
    cap = doc.styles.add_style("图注", 1)
    cap.base_style = doc.styles["Normal"]
    cap.font.name = LATIN
    cap.font.size = Pt(9)
    cap.font.color.rgb = GREY
    _east_asia(cap.element, SONG)
    cap.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
    cap.paragraph_format.first_line_indent = Pt(0)
    cap.paragraph_format.space_before = Pt(4)
    cap.paragraph_format.space_after = Pt(10)
    cap.paragraph_format.line_spacing = 1.0


# ---------------------------------------------------------------- content helpers
class Writer:
    def __init__(self, doc):
        self.doc = doc

    def h1(self, text):
        p = self.doc.add_paragraph(style="Heading 1")
        p.add_run(text)
        p.paragraph_format.page_break_before = True
        return p

    def h2(self, text):
        p = self.doc.add_paragraph(style="Heading 2")
        p.add_run(text)
        return p

    def h3(self, text):
        p = self.doc.add_paragraph(style="Heading 3")
        p.add_run(text)
        return p

    def p(self, text, indent=True, bold_prefix=None):
        par = self.doc.add_paragraph()
        if not indent:
            par.paragraph_format.first_line_indent = Pt(0)
        if bold_prefix:
            run = par.add_run(bold_prefix)
            run.bold = True
        par.add_run(text)
        return par

    def items(self, pairs, numbered=True):
        """pairs: list of (lead, text). Lead is bolded, then the explanation."""
        for i, (lead, text) in enumerate(pairs, 1):
            par = self.doc.add_paragraph()
            pf = par.paragraph_format
            pf.first_line_indent = Pt(-24)
            pf.left_indent = Pt(48)
            pf.space_after = Pt(2)
            mark = "%d. " % i if numbered else ""
            if mark:
                par.add_run(mark)
            run = par.add_run(lead)
            run.bold = True
            par.add_run(text)

    def bullets(self, texts):
        for text in texts:
            par = self.doc.add_paragraph()
            pf = par.paragraph_format
            pf.first_line_indent = Pt(-14)
            pf.left_indent = Pt(24)
            pf.space_after = Pt(2)
            par.add_run("· " + text)

    def figure(self, path, width_cm, caption):
        par = self.doc.add_paragraph()
        par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        par.paragraph_format.first_line_indent = Pt(0)
        par.paragraph_format.space_before = Pt(6)
        par.paragraph_format.space_after = Pt(0)
        par.paragraph_format.keep_with_next = True
        par.add_run().add_picture(path, width=Cm(width_cm))
        cap = self.doc.add_paragraph(style="图注")
        cap.add_run(caption)

    def table_caption(self, caption, new_page=False):
        par = self.doc.add_paragraph()
        if new_page:
            par.paragraph_format.page_break_before = True
        par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        par.paragraph_format.first_line_indent = Pt(0)
        par.paragraph_format.space_before = Pt(8)
        par.paragraph_format.space_after = Pt(3)
        par.paragraph_format.keep_with_next = True
        run = par.add_run(caption)
        run.font.size = Pt(9)
        run.font.color.rgb = GREY

    def table(self, headers, rows, widths, font=10.5, note=None, align_center=None, tight=False):
        table = self.doc.add_table(rows=1, cols=len(headers))
        table.alignment = WD_TABLE_ALIGNMENT.CENTER
        table.autofit = False
        table.style = "Table Grid"
        for i, width in enumerate(widths):
            table.columns[i].width = Cm(width)
        borders(table)
        cell_margins(table, 30, 30, 80, 80) if tight else cell_margins(table)
        align_center = align_center or []
        header = table.rows[0]
        repeat_header(header)
        for i, text in enumerate(headers):
            cell = header.cells[i]
            cell.width = Cm(widths[i])
            shade(cell, "E8EDE9")
            par = cell.paragraphs[0]
            par.paragraph_format.first_line_indent = Pt(0)
            par.paragraph_format.space_after = Pt(0)
            par.paragraph_format.line_spacing = 1.0
            par.alignment = WD_ALIGN_PARAGRAPH.CENTER
            run = par.add_run(text)
            run.bold = True
            run.font.size = Pt(font)
            _east_asia(run._r, HEI)
        for row in rows:
            new_row = table.add_row()
            trpr = new_row._tr.get_or_add_trPr()
            cant = OxmlElement("w:cantSplit")
            cant.set(qn("w:val"), "true")
            trpr.append(cant)
            cells = new_row.cells
            bold_row = isinstance(row, list) and len(row) == 2 and row[0] == "__bold__"
            values = row[1] if bold_row else row
            # a section banner such as 1、标准配置 spans the whole table
            if bold_row and len(values) > 1 and len(set(str(v) for v in values)) == 1:
                merged = cells[0].merge(cells[-1])
                par = merged.paragraphs[0]
                par.paragraph_format.first_line_indent = Pt(0)
                par.paragraph_format.space_after = Pt(0)
                par.paragraph_format.line_spacing = 1.0
                par.alignment = WD_ALIGN_PARAGRAPH.LEFT
                shade(merged, "F1F3F1")
                run = par.add_run(str(values[0]))
                run.bold = True
                run.font.size = Pt(font)
                continue
            for i, text in enumerate(values):
                cell = cells[i]
                cell.width = Cm(widths[i])
                par = cell.paragraphs[0]
                par.paragraph_format.first_line_indent = Pt(0)
                par.paragraph_format.space_after = Pt(0)
                par.paragraph_format.line_spacing = 1.0
                if i in align_center:
                    par.alignment = WD_ALIGN_PARAGRAPH.CENTER
                run = par.add_run(str(text))
                run.font.size = Pt(font)
                if bold_row:
                    run.bold = True
        if note:
            par = self.doc.add_paragraph()
            par.paragraph_format.first_line_indent = Pt(0)
            par.paragraph_format.space_before = Pt(3)
            par.paragraph_format.space_after = Pt(8)
            run = par.add_run(note)
            run.font.size = Pt(9)
            run.font.color.rgb = GREY
        else:
            spacer = self.doc.add_paragraph()
            spacer.paragraph_format.space_before = Pt(0)
            spacer.paragraph_format.space_after = Pt(4)
            spacer.paragraph_format.line_spacing = 0.6
        return table


# ---------------------------------------------------------------- front matter
def cover(doc):
    # document number block
    t = doc.add_table(rows=3, cols=2)
    t.alignment = WD_TABLE_ALIGNMENT.RIGHT
    t.autofit = False
    t.style = "Table Grid"
    borders(t, color="BFBFBF")
    cell_margins(t, 40, 40, 80, 80)
    info = [("文档编号", "GZ2418-MT-DD-001"), ("归档日期", "2026.11.20"), ("密级", "内部")]
    for row, (key, value) in zip(t.rows, info):
        for i, text in enumerate((key, value)):
            cell = row.cells[i]
            cell.width = Cm(2.6 if i == 0 else 4.2)
            shade(cell, "F2F2F2") if i == 0 else None
            par = cell.paragraphs[0]
            par.paragraph_format.first_line_indent = Pt(0)
            par.paragraph_format.space_after = Pt(0)
            par.paragraph_format.line_spacing = 1.0
            par.alignment = WD_ALIGN_PARAGRAPH.CENTER if i == 0 else WD_ALIGN_PARAGRAPH.LEFT
            run = par.add_run(text)
            run.font.size = Pt(9)

    for _ in range(5):
        doc.add_paragraph()

    def big(text, size, space=6):
        par = doc.add_paragraph()
        par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
        par.paragraph_format.first_line_indent = Pt(0)
        par.paragraph_format.space_after = Pt(space)
        par.paragraph_format.line_spacing = 1.3
        run = par.add_run(text)
        run.font.size = Pt(size)
        run.bold = True
        _east_asia(run._r, HEI)
        return par

    big("交互式多媒体终端项目", 34, 2)
    big("总体设计方案", 34, 16)

    par = doc.add_paragraph()
    par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
    par.paragraph_format.first_line_indent = Pt(0)
    run = par.add_run("GEC6818 平台 · 嵌入式 Linux · 800 × 480 电容触摸")
    run.font.size = Pt(11)
    run.font.color.rgb = GREY

    for _ in range(6):
        doc.add_paragraph()

    for label in ("版本：            V1.0", "审核：", "批准："):
        par = doc.add_paragraph()
        par.paragraph_format.left_indent = Cm(4.6)
        par.paragraph_format.first_line_indent = Pt(0)
        par.paragraph_format.space_after = Pt(8)
        run = par.add_run(label)
        run.font.size = Pt(15)

    for _ in range(3):
        doc.add_paragraph()

    par = doc.add_paragraph()
    par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
    par.paragraph_format.first_line_indent = Pt(0)
    run = par.add_run("二〇二六年十一月")
    run.font.size = Pt(15)
    _east_asia(run._r, HEI)


def revision(doc):
    doc.add_paragraph()
    par = doc.add_paragraph()
    par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
    par.paragraph_format.first_line_indent = Pt(0)
    par.paragraph_format.space_after = Pt(24)
    run = par.add_run("修订情况记录")
    run.font.size = Pt(22)
    run.bold = True
    _east_asia(run._r, HEI)

    w = Writer(doc)
    rows = [
        ["V1.0", "首次发布。确定页面划分、触摸命中模型、各模块功能与验收标准。", "", "2026-11-20", ""],
        ["", "", "", "", ""],
        ["", "", "", "", ""],
    ]
    w.table(["版本号", "修改描述", "修改人", "归档日期", "签收人"], rows,
            [1.7, 6.6, 2.1, 2.6, 1.6], font=9, align_center=[0, 2, 3, 4])

    doc.add_paragraph()
    par = doc.add_paragraph()
    par.paragraph_format.first_line_indent = Pt(0)
    run = par.add_run("说明：本方案描述目标形态与验收方法，实际实现以交付源码为准；"
                      "主机侧验证结果见第五章表 5-2，上板记录见《部署与验收》。")
    run.font.size = Pt(10.5)
    run.font.color.rgb = GREY


def contents(doc):
    par = doc.add_paragraph()
    par.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
    par.paragraph_format.first_line_indent = Pt(0)
    par.paragraph_format.space_after = Pt(12)
    run = par.add_run("目　　录")
    run.font.size = Pt(24)
    run.bold = True
    _east_asia(run._r, HEI)

    toc = doc.add_paragraph()
    toc.paragraph_format.first_line_indent = Pt(0)
    add_field(toc, 'TOC \\o "1-3" \\h \\z \\u', "打开文档后在此处右键，选择「更新域」生成目录")


# ---------------------------------------------------------------- chapters
def chapter1(w):
    w.h1("一、引言")

    w.h2("1.1 项目背景及目标")
    w.p("嵌入式触控终端把显示、输入、存储与应用逻辑放在同一块板卡上，是练习 Linux 设备访问与"
        "模块化 C 设计的合适载体。GEC6818 教学平台提供 S5P6818 应用处理器、7 寸 800×480 显示屏和"
        "电容触摸屏，内核已带 framebuffer 与 evdev 驱动，应用层无需改动内核即可完成图形显示与触控交互。")
    w.p("本项目在该平台上实现一台交互式多媒体终端：上电后先以账号登录，进入带日期时间的主界面，"
        "再由三张功能卡片进入电子相册、小游戏与创意画板。小游戏页集中了 2048、五子棋、井字棋与中国象棋"
        "四款，项目不使用音视频解码、网络服务和数据库，"
        "使实现重点落在页面组织、触摸命中和文件联动上，也便于在答辩现场逐行讲清代码。")
    w.p("项目目标如下：")
    w.items([
        ("统一入口与页面组织。", "登录成功后一屏可达三个功能；除登录页外各页左上角统一返回，位置固定，减少寻找成本。"),
        ("触摸输入可靠。", "单点触摸在 800×480 全屏范围内稳定命中，最小可点区域不小于 56×56 像素，并在界面回显坐标以便定位偏差。"),
        ("模块联动。", "画板保存的作品在下次进入相册时自动出现，断电重启后仍可浏览。"),
        ("规则与界面分离。", "2048 的压缩合并算法、棋类的连子与走子规则都独立成模块，可脱离绘图与设备做穷举验证。"),
        ("可构建、可交付。", "提供 Makefile 与 CMake 两套构建、Windows 桌面预览程序和 ARM 静态可执行程序。"),
    ])
    w.p("设计指标为：冷启动到登录页不超过 2 秒；一次触摸从报点到画面更新不超过 100 毫秒；"
        "连续操作 30 分钟无崩溃、无持续内存增长；画板在快速滑动时不出现断线；相册单页 6 张缩略图"
        "加载不超过 300 毫秒。以上指标在开发板上实测后回填，桌面模拟结果不作为硬件性能结论。")

    w.h2("1.2 设计参考文档")
    w.bullets([
        "《Linux 环境编程图文指南》",
        "《C Primer Plus（第 6 版）》",
        "S5P6818 用户手册、GEC6818 开发板实验指导书",
        "Linux 内核文档：framebuffer（fbdev）子系统与 input/evdev 事件接口",
        "谢东宏《多媒体娱乐终端》课程工程：显示、相册、白板与字体的实现思路",
        "肖林祥《多媒体娱乐终端》课程工程：2048 指数表示、棋盘与数字素材",
        "课程《项目要求》与项目验收评分表",
    ])

    w.h2("1.3 术语与缩写")
    w.p("同一个词在代码、文档与答辩里容易被理解成不同意思，先把本方案反复用到的几个术语固定下来，见表 1-1。")
    w.table_caption("表 1-1　术语与缩写")
    w.table(["术语", "在本方案中的含义"], [
        ["framebuffer", "内核提供的显存映射，应用把像素写进去即上屏，本方案通过 /dev/fb0 访问"],
        ["evdev", "内核输入事件接口，触摸的坐标与按下、抬起都从这里读出，本方案通过 /dev/input/event0 访问"],
        ["命中", "把一次触摸的坐标判定为某个按钮、卡片或棋盘落点"],
        ["就近吸附", "落点不在控件矩形内时按最近中心归属，避免控件之间的缝隙吞掉点击"],
        ["脏矩形", "本帧发生变化的屏幕区域，只把这一块提交到显存，而不是整屏重推"],
        ["页面状态", "Page 枚举的一个取值，决定当前由哪个模块绘制、由谁处理输入"],
        ["规则层", "只回答「这样走合不合法」的代码，不引用图形接口与设备节点"],
        ["静态链接", "可执行文件自带运行库，板端不需要另外安装库即可运行"],
    ], [3.2, 11.4], font=10, tight=True)


def chapter2(w):
    w.h1("二、软件设计要求")

    w.h2("2.1 开发调试环境")
    w.items([
        ("开发环境：", "Windows 11 + MinGW GCC 16 + CMake 4.3，用于桌面预览与自动回归；工程同时提供 Makefile，可在 Ubuntu 20.04 下用 ARM GCC 构建。"),
        ("交叉编译：", "Zig 0.13.0，命令 zig cc -target arm-linux-musleabi -mcpu=cortex_a9 -static，产物为 ARMv7-A、EABI5 软浮点、静态链接 musl 的可执行文件。"),
        ("运行环境：", "GEC6818 开发板，嵌入式 Linux；显示节点 /dev/fb0，触摸节点 /dev/input/event0，均可通过启动参数改写。"),
        ("调试工具：", "MobaXterm 或 SecureCRT 连接调试串口（115200-8-N-1）；Tftpd32 或 python -m http.server 传输程序；md5sum 校验板上文件与主机是否一致。"),
        ("版本管理：", "工程内本地 Git 仓库，改动可按提交记录回溯，不向外部仓库推送。"),
    ], numbered=False)

    w.h2("2.2 开发资源需求")
    w.items([
        ("硬件资源：", "GEC6818 开发板一套（含 7 寸电容触摸屏、5V/2A 电源、USB 转串口线、网线），另备 8 GB TF 卡一张用于备份运行包。"),
        ("软件资源：", "内置 TrueType 字体、示例照片与 2048 数字素材，沿用课程参考工程并保留来源清单。"),
        ("人力投入：", "1 人，需具备 C 语言、Linux 文件与设备访问、基本图形绘制能力。"),
        ("时间投入：", "约 5 个工作日，阶段划分见表 2-1。"),
    ], numbered=False)

    w.h2("2.3 程序设计周期")
    w.p("从需求梳理到交付共 5 个工作日，各阶段按依赖顺序推进，前一阶段的产物是后一阶段的输入。")
    w.table_caption("表 2-1　设计周期与阶段交付物")
    w.table(["阶段", "工作内容", "交付物", "工期"], [
        ["需求与框架", "页面划分、事件模型、目录分层与接口约定", "框架说明、公共头文件", "0.5 天"],
        ["显示与输入", "framebuffer 映射、evdev 读取、坐标归一化", "可上屏的空白页与触摸日志", "0.5 天"],
        ["页面实现", "登录注册、主界面、相册、小游戏、画板", "十一个页面与公用控件", "2 天"],
        ["联调与适配", "命中判定、坐标校正、增量刷新", "稳定交互版本", "1 天"],
        ["测试与交付", "回归用例、交叉编译、文档与打包", "运行包与设计文档", "1 天"],
        ["__bold__", ["合计", "—", "—", "5 天"]],
    ], [3.0, 6.2, 3.6, 1.8], align_center=[0, 3])

    w.h2("2.4 生产工具")
    w.items([
        ("硬件工具：", "GEC6818 开发板、7 寸电容屏、5V/2A 电源适配器、USB 转串口模块、网线、防静电垫。"),
        ("软件工具：", "VS Code、CMake、MinGW GCC 16、Zig 0.13.0、MobaXterm、Python 3.13（http.server 传文件、生成文档插图）。"),
        ("系统环境：", "Windows 11 用于开发与桌面预览，嵌入式 Linux 用于板端运行。"),
        ("测量工具：", "串口日志、md5sum 校验、秒表（记录启动与响应时间）。"),
    ], numbered=False)

    w.h2("2.5 程序设计流程图")
    w.p("程序为单线程事件循环。启动阶段依次完成参数解析、设备打开、资源加载与状态初始化，随后进入登录页；"
        "登录成功后进入主界面，主循环按「取事件—分发—按需重绘—提交显存」的顺序反复执行，"
        "直到收到退出事件再统一释放资源，整体流程见图 2-1。")
    w.figure(os.path.join(FIG, "fig2-1-flow.png"), 13.2, "图 2-1　程序总体流程图")

    w.p("把等待、分发和重绘串在同一个线程，是为了避免多个页面同时读写屏幕：任何时刻只有当前页面在绘制，"
        "输入也只有一处读取，不需要加锁。绘制先在内存图像上完成，再整块或按矩形提交到显存，"
        "所以局部重绘只是少提交一部分像素，绘制逻辑本身不变。")

    w.h3("2.5.1 页面状态与事件流")
    w.p("页面状态共 11 个：登录、注册、主界面、电子相册、大图浏览、小游戏列表、2048、五子棋、井字棋、"
        "中国象棋、创意画板。其中大图浏览是相册的子页面；小游戏列表与四款游戏构成两级结构，"
        "返回键因此按层级回退——棋局回列表、列表回主界面。主界面只负责分发，不保存业务数据，"
        "页面之间的跳转关系见图 2-2。")
    w.figure(os.path.join(FIG, "fig2-2-state.png"), 13.6, "图 2-2　页面状态与跳转关系")

    w.p("触摸事件由设备层统一转换为 DOWN、MOVE、UP、CANCEL 与 QUIT 五种。判断一个控件是否被点中时，"
        "不只看松开位置：按下点、按下期间最后一个采样点、松开点三点中任意一点落在控件矩形内即视为命中；"
        "若三点都未命中，再按控件自身规则兜底——工具栏在 40 像素半径内吸附到最近的中心，"
        "九宫格键盘则整块铺满，落在键缝里或比键帽高出一点的点击归属于最近的那个键。这两种兜底都针对"
        "电容屏报点与视觉位置存在数十像素偏差的实际情况，使相邻小控件之间的缝隙不会吞掉点击。")
    w.p("2048 页的滑动判定优先于按钮判定：按下点位于棋盘内且横向或纵向位移超过 22 像素时判为一次滑动，"
        "否则按方向卡片判定。相册在缩略图页与照片页之间用点击切换：点缩略图进入大图，点大图返回缩略图页。")
    w.p("三个棋盘页不做「矩形命中」而是「就近吸附」：整块棋盘就是一个目标，落点换算成最近的格点，"
        "换算失败再用按下点算一次，宁可落到旁边的格，也不让一次点击凭空消失。")

    w.h3("2.5.2 触摸映射：三个锚点、两段直线")

    w.p("登录页第一行 1/2/3 键点不动、4~9 正常，是最初暴露出来的问题。三行键的矩形只差 68 像素，"
        "不存在「半个屏幕」的分界，因此先怀疑坐标映射。程序内置一套只读自检来解决「到底是哪一步错」："
        "「--probe」把屏幕分成六条横带，实时显示每个样本的原始值与本次运行见过的 rawY 上下界。")

    w.p("自检给出第一组数据：手指从上沿划到下沿，rawY 依次为 87、322、580，对应屏幕 y 69、257、463，"
        "三段比例都是 1.25，正是声明量程与屏高的比值 600/480。也就是说驱动声明的 X 0..1024、Y 0..600 "
        "在跨度上是准的，整屏也没有死区。但它同时说明另一件事：比例正确不等于位置正确，"
        "探针画出的横线本身就是 raw 换算来的，绝对位置只能靠「按在一个已知图案上」来定。")

    w.p("于是用九个十字实测。屏幕 y = 70、250、430 三行分别报回 raw 86、330、518："
        "上段斜率 1.36、下段斜率 1.04。若用一条直线穿过两端（要求中部报 302），实测的 330 就偏了 28 个"
        "原始单位，折合屏幕 23 像素——正好半行键高。面板的响应是弯的，一条直线怎么拟合都会在屏幕中部"
        "留下半行键的误差，这就是标定做完之后 1/2/3 仍然落成 4/5/6 的原因。")

    w.p("因此映射改为每轴三个锚点、两段直线：九个十字按 3×3 铺开（x 60/400/740，y 70/250/430），"
        "每一列、每一行的三次采样取平均得到一个锚点，两段直线在中间锚点处接上，不会产生台阶；"
        "屏幕边缘由各自那一段的斜率外推，不再用一个平均斜率硬拉。某一次采样偏离超过 40 像素即判定失败，"
        "并在屏上列出九组原始值，而不是悄悄存下一份错的标定。")
    w.figure(os.path.join(SHOT, "10-calibration.png"), 13.4, "图 2-3　九点标定页（按 --calibrate 触发，十字按 3×3 顺序出现）")

    w.p("标定结果写入 data/touch.cal（swap 与六个锚点），启动时读取；读取前会与声明范围比对，"
        "跨度相差超过七分之一即判定为坏记录并忽略，并在终端说明原因。默认不标定，直接采用声明范围；"
        "需要时加「--calibrate」触发。登录页左下角实时显示按下点、抬起点与映射前的原始值，"
        "并把落点画成红色十字：按下 1 却看见十字落在 4 上，就是「映射把整行推低了一行」的直接证据。"
        "如需看到驱动公布的每一条坐标轴，可用「--trace-abs」启动。")

    w.p("拟合与映射判定集中在 touch_cal.c，只有整数运算，不引用屏幕、字库或设备接口，"
        "因此可以在宿主机上验证：tests/test_touch_cal.c 用一块「中部下陷 28 原始单位」的假面板生成采样，"
        "断言一条直线会偏 23 像素、而两段直线能把任意点送回原像素；轴向交换、镜像、边缘外推、"
        "锚点坍缩与单点跑偏各有断言。")

    w.h2("2.6 功能要求")
    w.p("功能划分为六个模块，前五个对应用户可见的页面，第六个是被各页面共用的图形与字体支撑。"
        "各模块的功能要求如下。")

    w.h3("2.6.1 登录与注册模块")
    w.p("登录页采用纯数值账号，账号与密码各 6 位，右侧为 3×4 九宫格键盘（1-9、删除、0、确认）。"
        "输入满 6 位自动跳到下一字段，删除键在当前字段回退一位；某字段为空时给出提示，"
        "不会把空输入当成登录失败，避免用户反复尝试却不知原因。")
    w.p("注册页提供账号、密码、确认三个输入框，校验规则为：三项均满 6 位、两次密码一致、账号未被占用。"
        "注册成功后写回 data/accounts.txt，返回登录页即可用新账号登录。演示账号为 678678 / 678678"
        "（只用 6/7/8 三个键，避开本机面板读不出坐标的最上一行）。")
    w.p("登录成功后播放约 1 秒的成功动画再进入主界面，动画由帧计数驱动，不使用阻塞延时，"
        "播放期间仍能响应退出事件。界面右下角显示程序构建号，串口首行也打印同一构建号，"
        "上板调试时先用它确认板上运行的是刚拷贝的那一版程序。")
    w.figure(os.path.join(SHOT, "00-login.png"), 10.0, "图 2-4　登录界面（数字九宫格）")

    w.h3("2.6.2 主界面与时钟模块")
    w.p("主界面顶部为产品名称与退出按钮，其下是 42 像素高的大号时钟与日期。时间取自系统本地时间，"
        "每秒比较一次秒数，仅在秒数变化时标记重绘，不做定时全屏刷新。三张 230×236 像素的功能卡片横向排列，"
        "分别是电子相册、小游戏与创意画板；卡片顶部有主题色条，下方是一行说明与「点击进入」提示。"
        "小游戏卡片进入的是列表页而不是某一款游戏，主页因此不必随游戏数量变化而重排。")
    w.p("卡片面积远大于手指落点误差，因此主页不需要额外的吸附逻辑。退出按钮固定在右上角，"
        "与各功能页左上角的返回按钮位置对称，用户在两处都不必寻找。")
    w.figure(os.path.join(SHOT, "01-home.png"), 10.0, "图 2-5　主界面（时钟与三个功能入口）")

    w.h3("2.6.3 电子相册模块")
    w.p("进入相册时先扫描 data/drawings（用户作品）与 res/Album（内置照片）两个目录，只收录常规文件且"
        "扩展名为 bmp 的图片，最多 128 张，单条路径不超过 512 字节；超出数量的图片计入「已达上限」，"
        "扫描不中断。作品按文件名倒序排列，以时间戳命名时近期作品排在最前。")
    w.p("每页 6 张 224×136 像素缩略图，按 3 列 2 行铺满内容区，格与格之间不留缝隙——相邻格共享边界，"
        "落点在任何位置都归属于某一格。加载缩略图后立即释放原尺寸临时缓冲，避免 6 张大图同时驻留内存。")
    w.figure(os.path.join(SHOT, "03-album.png"), 10.0, "图 2-6　电子相册缩略图页（每页 6 张）")
    w.p("点缩略图进入大图浏览，图片按可用区域等比缩放居中显示；上一张、下一张在首尾循环，点大图本身即可"
        "返回缩略图页。损坏或无法解码的文件显示读取失败提示，仍可切换或返回，不会使整页无响应。")
    w.figure(os.path.join(SHOT, "04-photo.png"), 9.4, "图 2-7　大图浏览与切换按钮")

    w.h3("2.6.4 游戏娱乐模块")
    w.p("游戏集中在「小游戏」页：主界面第二张卡片进入后是 2×2 四张大卡片，分别是 2048、五子棋、"
        "井字棋与中国象棋。这样主页始终保持三张卡片，以后再加游戏只需在列表里多一行，"
        "不必重新安排主页版式。四款游戏的页面结构一致——左上角返回、右上角「重新开始」、"
        "左侧棋盘、右侧状态栏——换游戏时不需要重新学习操作位置。")
    w.figure(os.path.join(SHOT, "05-games.png"), 10.0, "图 2-8　小游戏列表（四张入口卡片）")

    w.p("2048 使用 4×4 棋盘，每格以指数存储：0 表示空，1 表示 2，11 表示 2048。四个方向复用同一条"
        "「向前压缩—相邻相同合并一次—回填零格」的算法，通过坐标映射把四个方向转换成同一次行扫描，"
        "因此四个方向的行为天然一致，不存在某个方向漏判的情况。")
    w.p("合并结果计入总分；只有棋盘真正发生变化才生成新的 2 或 4，无效方向不计分也不生成新格。"
        "出现 2048 判定成功，无空位且相邻无可合并数字判定失败，两种情况都允许重新开始。")
    w.p("操作方式有两种：在棋盘区域内滑动，或点击右侧四张 184×168 像素的方向卡片。卡片按轴向分组"
        "（左侧上下、右侧左右），单张卡片的高度远大于触摸报点的纵向偏差，纵向操作不会再被误判为横向；"
        "最近一次生效的方向以浅色底与描边回显，即使该方向确实无法移动，用户也能确认点击已被接收。")
    w.figure(os.path.join(SHOT, "06-game.png"), 10.0, "图 2-9　2048 棋盘与四张方向卡片")

    w.p("五子棋为 15×15 交叉点、间距 25 像素，整盘作为一个大目标按就近吸附：落点先换算成最近的交叉点，"
        "换算失败才用按下点再算一次，所以不会因为偏几像素而丢子。玩家执黑先行，落子后电脑在同一帧内"
        "完成应手，没有「思考中」的中间状态。电脑按四个方向分别统计「连子数」与「开放端数」打分："
        "四连活三权重最高，被堵住的四连只值三成；攻击分乘 2、防守分乘 1.5 并加少量随机，"
        "既会堵对手的活四，也不会永远下同一手。五连即判胜，盘满无连子判和。")
    w.figure(os.path.join(SHOT, "07-gomoku.png"), 10.0, "图 2-10　五子棋（玩家执黑，电脑执白）")

    w.p("井字棋每格 112 像素，比触摸报点的纵向偏差还大，一格一个目标，点哪格就是哪格。玩家执 X 先行，"
        "电脑用全树搜索：九格的可能性有限，程序直接把所有后续走法算到底，胜负按步数折价（越早赢分越高），"
        "所以玩家先手最好也只能和棋。斜线、横线、竖线三种连法共用一张八条线的表，判断胜负与绘制胜利线"
        "读的是同一份数据。")
    w.figure(os.path.join(SHOT, "08-toe.png"), 10.0, "图 2-11　井字棋（3×3 大格）")

    w.p("中国象棋为 9 路 10 行、每格 37 像素的双人同屏对局，红方在下先行。规则按标准实现：将/帅一步一格"
        "不出九宫，士斜走一格不出九宫，象走田且塞象眼、不过河，马走日且蹩马腿，车走直线中间不得有子，"
        "炮不吃子时同车、吃子时必须隔且只隔一子，兵/卒向前一步、过河后可横走且不可后退。"
        "将帅不可照面：程序先试走、再判断两将是否同列且中间无子、最后把棋子放回，"
        "因此不需要为每种棋子单独推导 legality。")
    w.p("为了让人看得懂哪些点能走，选中棋子后先在所有合法落点画标记——空位画绿色圆点、可吃子画红色圆环，"
        "再画棋子本身，标记不会被棋子盖住；吃将直接终局，走完一手后若对方无子可动同样判负。"
        "非法走法不改变局面，只在右下角提示触点坐标，便于上板核对触摸偏差。")
    w.figure(os.path.join(SHOT, "09-chess.png"), 10.0, "图 2-12　中国象棋（选中棋子标出可走点）")

    w.h3("2.6.5 创意画板模块")
    w.p("画布 752×286 像素，位于工具栏下方。工具栏第一行为 7 个 56×56 色块（墨黑、砖红、赭黄、墨绿、"
        "靛蓝、紫、白），第二组为细、中、粗三档笔触，分别对应半径 2、3、7 像素。白色作为橡皮擦使用，"
        "选中后在画布上绘制白点即完成擦除。色块与笔触的命中区域比图形本身大 6 像素且相邻共享边界，"
        "点击落在两个色块之间时按最近中心归属，不会出现「点了没反应」。")
    w.p("笔迹采用圆形笔刷盖印，并用 Bresenham 线段把相邻两个采样点连起来，快速滑动时不会出现断续的圆点；"
        "手指离开画布即结束当前线段，不会把线画到工具栏上。画布内容在页面切换时保留，返回主页再进入仍在。")
    w.figure(os.path.join(SHOT, "02-whiteboard.png"), 10.0, "图 2-13　创意画板（色块与笔触工具栏）")
    w.p("保存时只导出画布区域，输出标准 24 位 BMP 到 data/drawings。文件名由时间戳加查重后缀构成，"
        "同一秒内连续保存不会互相覆盖；写入失败时提示目录权限或存储空间不足。下一次进入相册重新扫描目录，"
        "作品即出现在列表中——两个模块通过文件系统联动，不需要互相修改内部结构。")

    w.h3("2.6.6 通用图形与字体支撑")
    w.p("图形基础模块提供矩形、圆盘、线段、图像缩放与绘制、TrueType 文字五类接口，所有页面只调用这些接口，"
        "不直接操作像素指针。BMP 读写按小端显式解析文件头与信息头，检查尺寸、像素偏移与实际文件长度，"
        "支持 24 位与 32 位、行填充和负高度（自下而上存储）的图像。")
    w.p("中文文字使用内置 TrueType 字体，启动时加载一次，绘制时按字号生成字形并在边界内做像素混合。"
        "图像与字体的内存分配都有上限，解码失败不保留半成品，避免长时间运行后内存持续增长。")


def chapter3(w):
    w.h1("三、硬件设计要求（GEC6818）")

    w.h2("3.1 部件选择")
    w.p("项目直接使用 GEC6818 教学开发板及其配套屏幕，不修改内核、引脚与电路，只通过设备节点访问硬件。"
        "主要部件见表 3-1。")
    w.table_caption("表 3-1　主要部件与参数")
    w.table(["部件名称", "参数", "在本项目中的作用"], [
        ["应用处理器", "S5P6818，Cortex-A53 ×8，主频 1.4 GHz", "运行应用程序，驱动显示与触摸"],
        ["内存", "DDR3 1 GB", "存放程序、图像缓冲与相册缓存"],
        ["存储", "eMMC 8 GB，支持 TF 卡扩展", "存放程序、字体、照片与用户作品"],
        ["显示屏", "7 寸 TFT LCD，800×480，RGB 24 位并行接口", "界面显示，主缓冲区 800×480×4 字节"],
        ["触摸屏", "电容式，GSLX680 控制器，I2C 接口，单点", "采集触点坐标，映射到 800×480"],
        ["通信接口", "以太网 RJ45、UART0", "传输运行包、查看调试日志"],
        ["供电", "DC 5 V / 2 A", "整板供电"],
        ["调试配件", "USB 转 TTL 串口模块", "连接控制台，波特率 115200"],
    ], [2.9, 7.7, 4.0])

    w.h2("3.2 系统连接框图")
    w.p("各部件与主控的连接关系见图 3-1。显示与触摸是两条独立通路：显示走并行 RGB，触摸走 I2C 中断上报，"
        "应用层分别通过 /dev/fb0 与 /dev/input/event0 访问，互不影响。调试与传输走串口和网口，"
        "只在开发阶段使用，量产后可省略。")
    w.figure(os.path.join(FIG, "fig3-1-hardware.png"), 12.6, "图 3-1　系统连接框图")

    w.h2("3.3 系统逻辑框图")
    w.p("软件自上而下分为应用层、框架层、规则层、基础层与设备层五层，依赖方向单一：上层调用下层接口，"
        "下层不感知上层。页面模块不直接打开设备节点，规则模块不引用界面头文件，因此 2048 的算法可以"
        "脱离屏幕单独编译测试。各模块与源文件的对应关系见表 3-2，分层关系见图 3-2。")
    w.table_caption("表 3-2　模块与源文件对应关系")
    w.table(["模块", "源文件", "职责"], [
        ["登录与注册", "LoginSystem.c", "账号校验、九宫格输入、注册写文件与成功动画"],
        ["主界面", "MainScreenSystem.c", "日期时间刷新、三张功能卡片、退出"],
        ["电子相册", "AlbumSystem.c", "目录扫描、缩略图缓存、翻页与大图切换"],
        ["小游戏列表", "GamesMenuSystem.c", "四张入口卡片与返回，图标全部用绘图接口现画"],
        ["2048", "GameSystem.c / game.c", "前者负责绘制与输入，后者负责合并、计分与胜负"],
        ["五子棋", "GomokuSystem.c / boardgames.c", "前者换算落点与绘制棋形，后者判胜与启发式应手"],
        ["井字棋", "ToeSystem.c / boardgames.c", "前者绘制 3×3 大格，后者全树搜索与胜负判定"],
        ["中国象棋", "ChessSystem.c / boardgames.c", "前者绘制棋盘与可走点，后者六种棋子的走子规则"],
        ["创意画板", "WhiteboardSystem.c", "笔迹绘制、色块与笔触、清屏、保存 BMP"],
        ["页面调度与设备适配", "System.c / platform.c", "App 状态、事件分发与命中判定；framebuffer、evdev 与 Win32 预览"],
    ], [3.6, 4.6, 6.4], font=10, tight=True)
    w.figure(os.path.join(FIG, "fig3-2-software.png"), 13.2, "图 3-2　系统逻辑框图")
    w.p("同一套应用代码同时用于板端与 Windows 桌面预览，差异集中在设备层的一个文件里。"
        "桌面预览不是为了替代实机验收，而是让界面与交互在没有开发板时也能回归——"
        "板子不在手边时仍能修改页面并确认没有画错。")
    w.p("资源与数据的分工也在这一层固定下来：res 目录只读，存放字体与内置照片，随程序一起发布；"
        "data 目录可写，存放账号文件与用户画作。画板保存时只写 data/drawings，相册进入时只扫描该目录，"
        "两个模块之间没有函数调用，单独去掉任何一方，另一方仍能编译运行——联动关系落在文件系统上，"
        "而不是代码里。这样增删功能不需要改动对方的内部结构，也便于分别测试。")


def chapter4(w):
    w.h1("四、外观设计要求")

    w.h2("4.1 外观设计整体要求")
    w.p("整机按桌面摆放的展示设备设计，要求正面看不到线缆，配色与界面一致，边角不出现锐边。具体如下：")
    w.items([
        ("整机尺寸：", "190 mm × 120 mm × 22 mm（含外壳，不含支架），屏幕四周边框等宽 12 mm。"),
        ("配色：", "机身米白（#F5F2EB）配墨绿（#227C77）饰条，与程序界面的底色和主色一致，避免屏幕与机身两张皮。"),
        ("放置方式：", "背面 15° 倾斜支架，屏幕中心离桌面约 120 mm，坐姿与站姿都能正对屏幕。"),
        ("接口布置：", "电源、串口、网线接口全部置于背面并向下出线，正面无外露线缆。"),
        ("装配：", "亚克力前后壳卡扣定位加 4 颗 M3 螺丝固定；屏幕与外壳之间加 0.5 mm 泡棉垫，避免硬接触。"),
        ("表面处理：", "外壳亚克力磨砂，边缘倒角 R2，按键与接口处丝印标识。"),
    ])

    w.h2("4.2 产品相关参数")
    w.table_caption("表 4-1　产品参数")
    w.table(["需求名称", "详细要求"], [
        ["处理器", "S5P6818，Cortex-A53 ×8，主频 1.4 GHz"],
        ["显示屏", "7 寸 TFT LCD，800 × 480，RGB 24 位并行接口"],
        ["触摸屏", "电容式，GSLX680 控制器，I2C 接口，单点触摸"],
        ["操作系统", "嵌入式 Linux"],
        ["内存 / 存储", "DDR3 1 GB / eMMC 8 GB，支持 TF 卡扩展"],
        ["通信方式", "以太网 RJ45（调试与文件传输）、UART0 调试串口"],
        ["供电 / 功耗", "DC 5 V / 2 A，整机约 3.5 W（设计值，实测后回填）"],
        ["工作温度", "0 ℃ ～ 50 ℃"],
        ["整机尺寸 / 重量", "190 mm × 120 mm × 22 mm，约 420 g（含外壳，设计值）"],
        ["软件功能", "登录注册、主界面时钟、电子相册、四款小游戏、创意画板"],
    ], [3.4, 11.2], font=10, tight=True)


def chapter5(w):
    w.h1("五、测试要求")

    w.h2("5.1 整机结构方面测试要求")
    w.items([
        ("外壳装配：", "4 颗 M3 螺丝拧紧后前后壳无缝隙、无翘边；按压屏幕四周边框不应有咔哒声。"),
        ("屏幕贴合：", "屏幕与外壳之间泡棉压缩约 30%，四角无漏光，触摸时屏幕不产生形变位移。"),
        ("接口强度：", "电源、串口、网线接口各插拔 500 次后接触良好，无松动与断针。"),
        ("支架稳定性：", "15° 支架在 10° 斜面上不倾覆，轻推屏幕顶部不后仰。"),
        ("跌落（可选）：", "1 m 高度六面各跌落 1 次，功能正常、外壳无裂纹、屏幕无碎裂。"),
    ])

    w.h2("5.2 整机电气方面测试要求")
    w.items([
        ("供电：", "输入 5 V ± 5%，整机电流不超过 1 A；上电浪涌电流不超过 2 A。"),
        ("纹波：", "电源输出纹波峰峰值小于 100 mV，屏幕不出现横纹或闪烁。"),
        ("静电：", "接触放电 ±4 kV、空气放电 ±8 kV 后功能正常，允许重启但不允许器件损坏。"),
        ("温升：", "25 ℃ 环境连续运行 2 小时，主控与背光区域温升不超过 25 K，外壳不烫手。"),
        ("掉电：", "运行中直接断电 50 次，重新上电可正常启动；已保存的 BMP 文件不损坏、相册仍能读取。"),
    ])

    w.h2("5.3 整机环境方面测试要求")

    w.h3("5.3.1 高低温测试、温湿度测试")
    w.p("将整机放入高低温箱，按以下条件逐项执行，每档保温 2 小时后通电检查显示、触摸与文件读写：")
    w.bullets([
        "工作温度下限：0 ℃，保温 2 小时，触摸响应与显示正常；",
        "工作温度上限：50 ℃，保温 2 小时，连续操作 30 分钟不花屏、不重启；",
        "存储温度：-20 ℃ ～ 60 ℃ 各 4 小时，恢复常温 2 小时后通电检查；",
        "恒定湿热：40 ℃ / 93% RH，48 小时，取出擦干后检查屏幕无水印、触摸无误触。",
    ])

    w.h3("5.3.2 老化测试")
    w.p("连续运行 72 小时，用脚本每 30 秒切换一次页面（主页→相册→大图→小游戏→2048→五子棋→井字棋→象棋→画板），"
        "每 10 分钟保存一张画作，每 2 小时记录一次内存占用与外壳温度。判定标准为：崩溃 0 次、"
        "重启 0 次、内存占用波动小于 5%、外壳温度不超过 55 ℃。老化前后各执行一次表 5-1 的功能用例，"
        "两次结果应一致。")

    w.h2("5.4 软件功能与触摸精度测试")
    w.p("功能测试按表 5-1 执行，重点覆盖触摸报点偏差、相邻控件缝隙和页面联动三类容易出问题的地方。")
    w.table_caption("表 5-1　功能与触摸测试用例")
    w.table(["编号", "测试项", "操作", "预期结果"], [
        ["TC-01", "登录成功", "输入 678678 / 678678 后确认", "播放动画并进入主界面"],
        ["TC-02", "登录失败", "输入错误密码后确认", "提示密码错误，停留在登录页"],
        ["TC-03", "注册校验", "两次密码不一致", "提示不一致，不写入账号文件"],
        ["TC-04", "九宫格边界", "点击键位边缘与键缝两侧各 30 像素", "均命中对应按键，不串键"],
        ["TC-05", "相册上排", "依次点击第一行三张缩略图", "三张都能打开大图"],
        ["TC-06", "大图切换", "上一张/下一张 翻至首尾", "循环切换，无越界与空白"],
        ["TC-07", "小游戏入口", "点击主界面第二张卡片，再点四张卡片中的每一张", "四款游戏都能进入，返回键回上一层"],
        ["TC-08", "2048 四方向", "依次点击上、下、左、右卡片", "棋盘按方向移动并计分"],
        ["TC-09", "2048 滑动", "在棋盘内纵向与横向滑动", "触发对应方向，不误判轴向"],
        ["TC-10", "五子棋落子", "故意点在交叉点之间与边缘处", "吸附到最近交叉点，电脑随即应手"],
        ["TC-11", "五子棋判胜", "连成五子后继续点击", "显示胜负，不再接受落子"],
        ["TC-12", "井字棋落子", "在九个格子的边缘各点一次", "每格都命中，电脑不会输"],
        ["TC-13", "象棋走子", "选士、象、马、炮各走一步；点非法目标", "合法点有绿点/红圈提示，非法走法被拒绝并提示坐标"],
        ["TC-14", "象棋终局", "吃将或将死对手", "显示胜负并锁定棋盘"],
        ["TC-15", "画板选色", "依次点击 7 个色块后各画一笔", "笔迹颜色与色块一致"],
        ["TC-16", "笔触切换", "细、中、粗各画一笔", "线宽依次约为 4、6、14 像素"],
        ["TC-17", "保存联动", "保存后返回并重新进入相册", "新作品出现在第一页"],
        ["TC-18", "重启保留", "断电重启后进入相册", "已保存作品仍在，顺序不变"],
        ["TC-19", "触摸自检", "运行 ./run.sh --probe，手指从上沿慢慢滑到下沿", "青线一路跟着下移；rawY 的最小值与最大值都随位置变化，不停在某个数上"],
        ["TC-20", "九点标定与键盘", "跑 ./run.sh --calibrate 点完九个十字，再回登录页依次点击 1、2、3、4~9、0、删除、确认", "标定页显示三组锚点与边缘范围、最大偏差≤40 像素；每一键都有反应，红十字落在按下的那个键上"],
        ["TC-21", "随机点击", "每个页面随机点击 600 次", "无崩溃、无越界，可正常返回"],
        ["TC-22", "长时间运行", "连续切换页面 2 小时", "无卡死，内存无持续增长"],
    ], [1.7, 3.0, 5.4, 4.5], align_center=[0])

    w.p("上板之前，已在桌面端用相同的 C 代码完成一轮自动验证，结果如表 5-2。"
        "桌面结果只说明代码逻辑与界面布局正确，不能替代实机的触摸精度与显示效果。")
    w.table_caption("表 5-2　已完成的主机侧验证")
    w.table(["验证项", "方法与结果"], [
        ["构建", "MinGW Release + CMake 桌面构建通过，告警按错误处理；ARM 侧生成 ELF32 小端、EABI5 软浮点、静态链接可执行文件"],
        ["2048 规则", "四方向、单次合并、无效移动、计分与胜负穷举通过；2 万次随机操作质量守恒"],
        ["BMP 边界", "奇数宽度行填充、负高度、32 位高字节、截断文件与非法尺寸均按预期处理"],
        ["棋类规则", "五子棋判胜与堵活四、井字棋穷举证明玩家不可胜、象棋 7 组走子规则断言全部通过"],
        ["触摸标定", "用「声明范围与实际不符」的假面板验证：旧映射偏 190 像素，标定后偏差≤2 像素；轴向交换、镜像、内缩外推、范围坍缩与单点跑偏各有断言"],
        ["触摸命中", "九宫格 25 项、方向卡片 13 项、工具栏 21 项、相册 12 项断言全部通过"],
        ["随机点击", "11 个页面共 6600 次随机点击，无崩溃、无越界"],
        ["页面联动", "登录→相册→大图→小游戏→四款游戏→画板→保存→相册 全链路通过"],
    ], [3.4, 11.2], font=10, tight=True)


def chapter6(w):
    w.h1("六、成本")

    w.h2("6.1 成本估算")
    w.p("单机成本不含固定资产投入，单位：人民币元。标准配置按一台可演示的整机计算，可选配置按需选配。")
    w.table_caption("表 6-1　单机成本估算")
    w.table(["部件名称", "规格型号", "成本估算", "说明"], [
        ["__bold__", ["1、标准配置", "1、标准配置", "1、标准配置", "1、标准配置"]],
        ["GEC6818 开发板", "含 7 寸电容屏、1 GB 内存、8 GB eMMC", "1380", "核心部件，占成本主要部分"],
        ["电源适配器", "DC 5 V / 2 A", "25", "随板另配一只备用"],
        ["USB 转 TTL 模块", "CH340", "15", "串口控制台与日志"],
        ["网线", "1.5 m", "5", "传输运行包"],
        ["亚克力外壳", "激光切割前后壳 + 15° 支架", "60", "小批量定制"],
        ["TF 卡", "8 GB", "25", "备份运行包与作品"],
        ["__bold__", ["合计", "—", "1510", ""]],
        ["__bold__", ["2、可选配置", "2、可选配置", "2、可选配置", "2、可选配置"]],
        ["备用触摸屏", "7 寸电容屏", "180", "损坏替换"],
        ["展示支架", "亚克力 15°", "40", "答辩展台使用"],
        ["双路串口小板", "USB-TTL ×2", "20", "同时接控制台与日志"],
        ["__bold__", ["合计", "—", "240", ""]],
    ], [3.8, 5.6, 2.2, 3.0], align_center=[2])

    w.h2("6.2 成本控制")
    w.items([
        ("素材复用：", "字体、示例照片与 2048 数字素材沿用课程参考工程，省去美术与素材采购费用。"),
        ("功能聚焦：", "不选配摄像头、音频功放与无线模块，硬件成本集中在一块核心板上。"),
        ("软件零依赖：", "程序静态链接，板端不安装 Qt、SDL 或播放器，省去运行库与可能的授权费用。"),
        ("结构简化：", "外壳用亚克力激光切割替代开模，单件约 60 元；若开模，仅模具费即需数千元。"),
        ("批量议价：", "按 50 台采购时开发板可议价至 1250 元左右，单机成本降至约 1350 元（见表 6-2）。"),
        ("维护成本：", "统一构建脚本与自动回归，改动后一条命令完成构建与验证，减少返工与现场调试时间。"),
    ])
    w.p("从构成看，硬件占单机成本的九成以上，结构与配件合计不足一成，因此控制成本的关键不在外壳或线材，"
        "而在开发板的采购价与是否追加扩展模块。按不同批量重新估算，结果见表 6-2。")
    w.table_caption("表 6-2　批量与单机成本")
    w.table(["批量", "开发板", "结构与配件", "单机成本"], [
        ["1 台（样机）", "1380", "130", "1510"],
        ["10 台", "1320", "115", "1435"],
        ["50 台", "1250", "102", "1352"],
    ], [4.2, 3.6, 4.0, 2.8], font=10, align_center=[1, 2, 3],
        note="注：结构与配件含亚克力外壳、电源适配器、USB 转串口模块、网线与 TF 卡；价格为 2026 年教学套件询价估算。")


def chapter7(w):
    w.h1("七、项目验收标准")

    w.h2("7.1 验收方式")
    w.p("验收采用现场演示、代码走查与文档检查三种方式：现场按 7.2 的流程操作，并随机指定测试点；"
        "代码走查确认模块边界、2048 合并算法、棋类走子规则与触摸命中逻辑；文档检查确认本方案、部署说明与验证记录"
        "与实际实现一致，不以截图替代实机结果。")

    w.h2("7.2 演示流程")
    w.p("演示控制在 5 分钟内，按功能依赖顺序一次走完，中间不重启：")
    w.items([
        ("启动：", "上电，展示串口打印的构建号与登录页。"),
        ("登录：", "用演示账号登录，观看成功动画后进入主界面，指出时钟每秒刷新。"),
        ("相册：", "进入相册，翻页、点缩略图看大图、切换上一张/下一张、点大图返回。"),
        ("小游戏：", "进入小游戏列表，2048 用方向卡片与滑动各走一步；五子棋落一手让电脑应手；"
                   "井字棋与象棋各走一步，指出可走点提示。"),
        ("画板：", "切换颜色与笔触画两笔，保存后返回相册确认新作品出现。"),
        ("持久化：", "断电重启，再次进入相册确认作品仍在。"),
    ])

    w.h2("7.3 评分表")
    w.p("评分按表 7-1 执行，总分 100 分。其中稳定性为否决项：演示过程中出现段错误或功能异常，该项记 0 分。")
    w.table_caption("表 7-1　项目验收评分表", new_page=True)
    w.table(["功能模块", "给分建议", "分值", "评定得分", "备注"], [
        ["界面美观性 / 布局的合理性", "配色统一、控件对齐、入口清晰（8-10）／一般（5-8）／较差（0-5）", "10", "", "米白配墨绿，三卡片入口"],
        ["工程模块化 / 代码规范", "按页面拆分文件 +3／命名规范 +3／代码规范 +2／项目名贴合 +2", "10", "", "src 一页一文件，头文件集中 inc"],
        ["设计文档完成度", "方案、部署、验证记录齐全 5／部分 2／无 0", "5", "", "本方案 + 部署验收 + 验证记录"],
        ["创新能力", "规定之外的功能或实现亮点", "5", "", "三点命中与坐标回显、增量刷新、构建号自证"],
        ["表达能力", "讲解条理清晰、能答出实现细节", "5", "", ""],
        ["项目稳定性", "演示中出现段错误或功能异常记 0", "10", "", ""],
        ["功能一　登录与注册", "九宫格输入、注册校验、成功动画", "8", "", ""],
        ["功能二　电子相册", "缩略图分页、放大、上一张/下一张", "12", "", ""],
        ["功能三　2048", "四方向、合并计分、胜负判断与重开", "8", "", ""],
        ["功能四　五子棋", "落点吸附、五连判胜、电脑应手", "6", "", ""],
        ["功能五　井字棋", "九格命中、三连判胜、电脑不失误", "5", "", ""],
        ["功能六　中国象棋", "六种棋子走法、可走点提示、吃将与困毙判负", "6", "", ""],
        ["功能七　创意画板", "7 色、3 档笔触、擦除、保存与相册联动", "10", "", ""],
        ["__bold__", ["合计", "—", "100", "", ""]],
    ], [4.0, 5.2, 1.3, 1.6, 2.5], font=9, align_center=[2, 3])

    w.h2("7.4 交付物清单")
    w.bullets([
        "源代码工程：src（页面与规则）、inc（公共头文件）、res（字体与素材）、tests（回归测试）、tools（构建与打包脚本）、docs（文档）",
        "板端运行包：bin/terminal、run.sh、res、data 目录",
        "Windows 桌面预览包：terminal.exe 与运行所需资源",
        "文档：本设计方案、部署与验收说明、程序框架说明、复用与改动说明、验证记录",
        "演示材料：现场演示流程与讲解提纲；演示视频由本人录制",
    ])


# ---------------------------------------------------------------- assembly
def main():
    doc = Document()
    build_styles(doc)
    add_toc_styles(doc)
    section = doc.sections[0]
    section.page_width = Cm(21.0)
    section.page_height = Cm(29.7)
    section.left_margin = Cm(3.17)
    section.right_margin = Cm(3.17)
    section.top_margin = Cm(2.54)
    section.bottom_margin = Cm(2.54)
    section.different_first_page_header_footer = True

    # header
    head = section.header.paragraphs[0]
    head.paragraph_format.first_line_indent = Pt(0)
    head.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    run = head.add_run("交互式多媒体终端项目　总体设计方案")
    run.font.size = Pt(9)
    run.font.color.rgb = GREY
    _east_asia(run._r, SONG)
    hrule(head)

    # footer with page number
    foot = section.footer.paragraphs[0]
    foot.paragraph_format.first_line_indent = Pt(0)
    foot.alignment = WD_ALIGN_PARAGRAPH.CENTER
    lead = foot.add_run("— ")
    lead.font.size = Pt(9)
    add_field(foot, "PAGE", "1")
    tail = foot.add_run(" —")
    tail.font.size = Pt(9)
    for r in foot.runs:
        r.font.color.rgb = GREY

    cover(doc)
    doc.add_page_break()
    revision(doc)
    doc.add_page_break()
    contents(doc)
    # No page break here: every chapter heading already starts on a new page, and an
    # explicit break after the TOC field leaves an empty page whenever the table of
    # contents happens to end near the bottom of its own page.

    w = Writer(doc)
    chapter1(w)
    chapter2(w)
    chapter3(w)
    chapter4(w)
    chapter5(w)
    chapter6(w)
    chapter7(w)

    # refresh fields (TOC / page numbers) when the document is opened
    settings = doc.settings.element
    update = OxmlElement("w:updateFields")
    update.set(qn("w:val"), "true")
    settings.append(update)

    doc.save(OUT)
    print("saved", OUT)
    print("paragraphs", len(doc.paragraphs), "tables", len(doc.tables))


if __name__ == "__main__":
    main()
