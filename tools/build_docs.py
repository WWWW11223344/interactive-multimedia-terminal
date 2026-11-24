"""Generate the architecture image, labelled screenshots and Word project report.

Host dependencies: Pillow, python-docx. No video is generated.
Run the CTest regression before this script to refresh the actual C screenshots.
"""
import argparse
import os
from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"


def font_path():
    choices = [os.environ.get("TERMINAL_DOC_FONT", ""),
               "C:/Windows/Fonts/simhei.ttf",
               "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"]
    for item in choices:
        if item and Path(item).is_file():
            return item
    raise RuntimeError("Set TERMINAL_DOC_FONT to a Chinese TrueType/OpenType font")


def architecture(font):
    img = Image.new("RGB", (1600, 1120), "#f3f6fa")
    draw = ImageDraw.Draw(img)
    sizes = {size: ImageFont.truetype(font, size) for size in (25, 28, 32, 38, 48)}
    draw.text((70, 40), "交互式多媒体终端 · 软件结构", font=sizes[48], fill="#18354c")
    draw.text((70, 106), "GEC6818 / 800×480  |  C99  |  单线程事件循环", font=sizes[28], fill="#52697a")

    def box(rect, title, lines, fill="white"):
        draw.rounded_rectangle(rect, radius=18, fill=fill, outline="#c5d5df", width=2)
        x, y = rect[:2]
        draw.text((x + 24, y + 16), title, font=sizes[32], fill="#145a69")
        for n, line in enumerate(lines):
            draw.text((x + 24, y + 64 + n * 36), line, font=sizes[25], fill="#283e51")

    def arrow(x1, y1, x2, y2):
        draw.line((x1, y1, x2, y2), fill="#718e9e", width=4)
        draw.polygon(((x2, y2), (x2 - 7, y2 - 13), (x2 + 7, y2 - 13)), fill="#718e9e")

    box((70, 170, 1530, 290), "入口 main.c", ["参数解析 → 初始化 App / Platform → 主循环 → 关闭设备并释放资源"])
    arrow(530, 290, 530, 328)
    arrow(1300, 290, 1300, 328)
    box((70, 330, 1030, 465), "应用 System.c / terminal.h", ["持有 App 状态、Page、Event；统一分发点击与滑动；dirty 按需重绘", "HOME / ALBUM / PHOTO / GAMES / 棋牌四页 / BOARD；logger.c 运行日志"])
    box((1070, 330, 1530, 885), "平台 platform.c", ["platform.h 定义设备接口", "Linux：framebuffer / evdev", "Windows：Win32 桌面预览", "显示提交、输入轮询、关闭", "只处理设备差异"])
    for x in (220, 550, 880):
        arrow(x, 465, x, 508)
    box((70, 510, 370, 650), "主页 / 相册", ["时钟、入口、缩略图", "翻页、照片放大"])
    box((400, 510, 700, 650), "创意画板", ["颜色、笔触、清屏", "保存 BMP → 相册"])
    box((730, 510, 1030, 650), "小游戏界面", ["2048 / 五子棋 / 井字棋 / 象棋", "落点吸附 → 规则层"])
    for x in (220, 550, 880):
        arrow(x, 650, x, 698)
    box((70, 700, 700, 885), "基础能力 display.h", ["lcd.c / file_manager.c", "内存绘图、BMP 读写与扫描", "print.c → truetype.c → 内置字体"])
    box((730, 700, 1030, 885), "纯规则层", ["game.c 2048 · boardgames.c 棋类", "合并计分、连子判定、走子规则", "不引用界面与设备"])
    box((70, 920, 1530, 1055), "工程与验证", ["src / inc / lib / res   ·   CMake / Makefile   ·   tests / tools / docs", "主机测试 5 项通过 + ARM 交叉编译 + GEC6818 实机运行；失效带为屏体硬件问题，已布局避让"], "#e6f2f2")
    img.save(DOCS / "architecture.png")


def screenshots(font):
    """Badge goes in a strip below the frame, never on top of it.

    The corners of these pages are all occupied (return button top-left, page
    counter top-right, action buttons bottom-right), so an overlay badge would
    always cover real UI. A strip cannot.
    """
    label_font = ImageFont.truetype(font, 17)
    text = "桌面模拟（Windows 预览，非实机）"
    for path in sorted((DOCS / "screenshots").glob("*.bmp")):
        with Image.open(path) as source:
            frame = source.convert("RGB")
        strip = 28
        img = Image.new("RGB", (frame.width, frame.height + strip), "#f4e3b8")
        img.paste(frame, (0, 0))
        draw = ImageDraw.Draw(img)
        width = int(draw.textlength(text, font=label_font)) + 24
        draw.text((img.width - width + 12, frame.height + 5), text, font=label_font, fill="#5a4213")
        img.save(path.with_suffix(".png"))


def set_font(style, latin, east_asia, size):
    style.font.name = latin
    style.font.size = Pt(size)
    style.element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), east_asia)


def inline(paragraph, value):
    for part in re.split(r"(\*\*[^*]+\*\*|`[^`]+`)", value):
        if part.startswith("**") and part.endswith("**"):
            paragraph.add_run(part[2:-2]).bold = True
        elif part.startswith("`") and part.endswith("`"):
            run = paragraph.add_run(part[1:-1])
            run.font.name = "Consolas"
            run.font.size = Pt(9)
        else:
            paragraph.add_run(part)


def report():
    document = Document()
    sec = document.sections[0]
    sec.page_width, sec.page_height = Cm(21), Cm(29.7)
    sec.top_margin, sec.bottom_margin = Cm(2.2), Cm(2.0)
    sec.left_margin, sec.right_margin = Cm(2.5), Cm(2.2)
    set_font(document.styles["Normal"], "Calibri", "宋体", 11)
    normal = document.styles["Normal"].paragraph_format
    normal.line_spacing = 1.35
    normal.space_after = Pt(6)
    for name, size in (("Title", 24), ("Heading 1", 17), ("Heading 2", 13), ("Heading 3", 12)):
        set_font(document.styles[name], "Calibri", "黑体", size)
        document.styles[name].font.color.rgb = RGBColor.from_string("183F55")
    header = sec.header.paragraphs[0]
    header.text = "交互式多媒体终端 · 课程项目书"
    header.alignment = WD_ALIGN_PARAGRAPH.RIGHT
    header.style.font.size = Pt(9)
    footer = sec.footer.paragraphs[0]
    footer.alignment = WD_ALIGN_PARAGRAPH.CENTER
    footer.add_run("第 ")
    field = OxmlElement("w:fldSimple")
    field.set(qn("w:instr"), "PAGE")
    footer._p.append(field)
    footer.add_run(" 页")
    lines = (DOCS / "项目书.md").read_text(encoding="utf-8").splitlines()
    n, code = 0, False
    while n < len(lines):
        line = lines[n].strip()
        if not line:
            n += 1
            continue
        if line.startswith("```"):
            code = not code
        elif code:
            p = document.add_paragraph()
            run = p.add_run(lines[n])
            run.font.name, run.font.size = "Consolas", Pt(9)
        elif line.startswith("# "):
            p = document.add_paragraph(line[2:], "Title")
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            p.paragraph_format.space_before = Pt(90)
            p.paragraph_format.space_after = Pt(60)
        elif line.startswith("## "):
            if line == "## 摘要":
                document.add_page_break()
            document.add_heading(line[3:], level=1)
        elif line.startswith("### "):
            document.add_heading(line[4:], level=2)
        elif line.startswith("!["):
            match = re.fullmatch(r"!\[(.*?)\]\((.*?)\)", line)
            p = document.add_paragraph()
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            p.add_run().add_picture(str(DOCS / match[2]), width=Cm(15.8))
            p = document.add_paragraph(match[1])
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            p.runs[0].font.size = Pt(9)
        elif line.startswith("|"):
            rows = []
            while n < len(lines) and lines[n].strip().startswith("|"):
                cells = [item.strip() for item in lines[n].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?", item) for item in cells):
                    rows.append(cells)
                n += 1
            table = document.add_table(rows=0, cols=len(rows[0]))
            table.style = "Table Grid"
            for j, row in enumerate(rows):
                tr = table.add_row()
                for cell, value in zip(tr.cells, row):
                    inline(cell.paragraphs[0], value)
                    for run in cell.paragraphs[0].runs:
                        run.font.size = Pt(9)
                        run.bold = j == 0
                props = tr._tr.get_or_add_trPr()
                props.append(OxmlElement("w:cantSplit"))
                if j == 0:
                    props.append(OxmlElement("w:tblHeader"))
            document.add_paragraph()
            continue
        else:
            p = document.add_paragraph()
            inline(p, line)
        n += 1
    document.core_properties.title = "基于嵌入式 Linux 的交互式多媒体终端设计与实现"
    document.core_properties.subject = "课程项目书；截图为桌面模拟；实机待验收"
    document.core_properties.author = ""  # Student fills in actual identity.
    document.save(DOCS / "项目书.docx")


if __name__ == "__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    selected_font = font_path()
    architecture(selected_font)
    screenshots(selected_font)
    report()
    print("Generated architecture.png, 5 labelled screenshots and project report.docx; no video generated.")
