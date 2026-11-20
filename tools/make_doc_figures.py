"""Generate the diagrams used by the design document.

Same palette as the terminal UI. Canvases are kept narrow on purpose: the figures are
placed at 12-13 cm in the document, so a smaller canvas with larger type keeps the
labels legible instead of shrinking them to 4 pt.
"""
import math
import os

from PIL import Image, ImageDraw, ImageFont

S = 3  # supersample factor; final image is downsampled for clean edges
INK = (26, 32, 33)
MUTED = (96, 112, 112)
TEAL = (34, 124, 119)
CARD = (255, 255, 255)
LINE = (198, 194, 183)
ACCENT = (188, 132, 54)
SOFT = (228, 239, 236)
CREAM = (252, 248, 236)

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "figures")
FONT_R = "C:/Windows/Fonts/msyh.ttc"
FONT_B = "C:/Windows/Fonts/msyhbd.ttc"


def fnt(size, bold=False):
    return ImageFont.truetype(FONT_B if bold else FONT_R, int(size * S))


def canvas(w, h, bg=(255, 255, 255)):
    return Image.new("RGB", (int(w * S), int(h * S)), bg)


def drawer(im):
    return ImageDraw.Draw(im)


def text_c(d, cx, cy, s, size, color=INK, bold=False):
    f = fnt(size, bold)
    x0, y0, x1, y1 = d.textbbox((0, 0), s, font=f)
    d.text((cx * S - (x1 - x0) / 2 - x0, cy * S - (y1 - y0) / 2 - y0), s, font=f, fill=color)


def box(d, cx, cy, w, h, title, sub=None, fill=CARD, outline=LINE, tsize=24, ssize=18,
        radius=10, tcolor=INK, scolor=MUTED, lw=2):
    d.rounded_rectangle([(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S],
                        radius=radius * S, fill=fill, outline=outline, width=int(lw * S))
    if sub:
        text_c(d, cx, cy - h * 0.20, title, tsize, tcolor, True)
        text_c(d, cx, cy + h * 0.24, sub, ssize, scolor)
    else:
        text_c(d, cx, cy, title, tsize, tcolor, True)


def pill(d, cx, cy, w, h, s, fill=TEAL, color=(255, 255, 255), size=21):
    d.rounded_rectangle([(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S],
                        radius=(h / 2) * S, fill=fill)
    text_c(d, cx, cy, s, size, color, True)


def diamond(d, cx, cy, hw, hh, s, size=22):
    d.polygon([(cx * S, (cy - hh) * S), ((cx + hw) * S, cy * S),
               (cx * S, (cy + hh) * S), ((cx - hw) * S, cy * S)],
              fill=CREAM, outline=ACCENT, width=int(2 * S))
    text_c(d, cx, cy, s, size, INK, True)


def arrow(d, pts, color=TEAL, lw=2, head=10):
    pts = [(p[0] * S, p[1] * S) for p in pts]
    d.line(pts, fill=color, width=int(lw * S), joint="curve")
    (x0, y0), (x1, y1) = pts[-2], pts[-1]
    ang = math.atan2(y1 - y0, x1 - x0)
    hl = head * S
    for delta in (0.42, -0.42):
        d.line([(x1, y1), (x1 - hl * math.cos(ang - delta), y1 - hl * math.sin(ang - delta))],
               fill=color, width=int(lw * S))


def label(d, x, y, s, size=17, color=MUTED):
    d.text((x * S, y * S), s, font=fnt(size), fill=color)


def save(im, name):
    os.makedirs(OUT, exist_ok=True)
    w, h = im.size
    im = im.resize((w // S, h // S), Image.LANCZOS)
    im.save(os.path.join(OUT, name))
    print("wrote", name, im.size)


# ---------------------------------------------------------------- 图2-1 总体流程
def fig_flow():
    W, H = 880, 1160
    im = canvas(W, H)
    d = drawer(im)
    cx, bw, bh = 440, 600, 68

    pill(d, cx, 40, 176, 44, "开机启动", size=20)

    steps = [
        (124, "解析启动参数", "显示节点 / 触摸节点 / 资源目录 / 坐标校正"),
        (216, "打开显示与触摸设备", "framebuffer 映射、evdev 量程读取"),
        (308, "加载字体与界面资源", "TrueType 字体、内存画布、相册缓存"),
        (400, "初始化 App，进入登录页", "九宫格输入账号与密码"),
    ]
    for y, t, s in steps:
        box(d, cx, y, bw, bh, t, s)

    diamond(d, cx, 518, 168, 46, "校验通过？")
    box(d, cx, 620, bw, bh, "进入主界面", "日期时间 + 三个功能入口卡片")
    box(d, cx, 712, bw, bh, "主循环取输入事件", "最多等待 16 ms，一次取完队列")
    diamond(d, cx, 812, 150, 44, "退出事件？")
    box(d, cx, 914, bw, bh, "分发到当前页面", "登录 / 相册 / 大图 / 小游戏 / 画板")
    box(d, cx, 1006, bw, bh, "标记脏区域并按需重绘", "只把变化的矩形提交到显存")

    arrow(d, [(cx, 62), (cx, 124 - bh / 2)])
    for y0, y1 in ((124, 216), (216, 308), (308, 400)):
        arrow(d, [(cx, y0 + bh / 2), (cx, y1 - bh / 2)])
    arrow(d, [(cx, 400 + bh / 2), (cx, 518 - 46)])
    arrow(d, [(cx, 518 + 46), (cx, 620 - bh / 2)])
    arrow(d, [(cx, 620 + bh / 2), (cx, 712 - bh / 2)])
    arrow(d, [(cx, 712 + bh / 2), (cx, 812 - 44)])
    arrow(d, [(cx, 812 + 44), (cx, 914 - bh / 2)])
    arrow(d, [(cx, 914 + bh / 2), (cx, 1006 - bh / 2)])

    # 登录失败：左侧回到登录页
    arrow(d, [(cx - 168, 518), (96, 518), (96, 400), (cx - bw / 2, 400)], color=ACCENT)
    label(d, 20, 436, "重新", 15, (150, 110, 50))
    label(d, 20, 456, "输入", 15, (150, 110, 50))
    label(d, 52, 556, "否：提示错误，留在登录页", 14, (150, 110, 50))

    # 主循环回环：左侧
    arrow(d, [(cx - bw / 2, 1006), (44, 1006), (44, 712), (cx - bw / 2, 712)])
    label(d, 18, 840, "下", 15, TEAL)
    label(d, 18, 858, "一", 15, TEAL)
    label(d, 18, 876, "轮", 15, TEAL)

    # 退出：右侧独占通道
    arrow(d, [(cx + 150, 812), (836, 812), (836, 1100), (742, 1100)], color=MUTED)
    box(d, 560, 1100, 360, 58, "关闭设备并释放资源", fill=SOFT, outline=TEAL, tsize=21)
    label(d, cx + 158, 780, "是", 16, MUTED)
    save(im, "fig2-1-flow.png")


# ---------------------------------------------------------------- 图2-2 页面状态
def fig_state():
    W, H = 900, 720
    im = canvas(W, H)
    d = drawer(im)
    box(d, 140, 92, 200, 72, "登录", "PAGE_LOGIN", tsize=22, ssize=15)
    box(d, 140, 300, 200, 72, "注册", "PAGE_REGISTER", tsize=22, ssize=15)
    box(d, 410, 92, 190, 72, "主界面", "PAGE_HOME", fill=SOFT, outline=TEAL, tsize=22, ssize=15)
    box(d, 700, 92, 200, 72, "电子相册", "PAGE_ALBUM", tsize=22, ssize=15)
    box(d, 700, 250, 200, 72, "大图浏览", "PAGE_PHOTO", tsize=22, ssize=15)
    box(d, 700, 420, 200, 72, "小游戏列表", "PAGE_GAMES", fill=SOFT, outline=TEAL,
        tsize=21, ssize=15)
    box(d, 410, 420, 190, 72, "创意画板", "PAGE_BOARD", tsize=22, ssize=15)

    games = [(95, "2048", "PAGE_2048"), (280, "五子棋", "PAGE_GOMOKU"),
             (465, "井字棋", "PAGE_TOE"), (650, "中国象棋", "PAGE_CHESS")]
    for cx, title, page in games:
        box(d, cx, 640, 165, 64, title, page, tsize=20, ssize=14)

    arrow(d, [(126, 128), (126, 264)], color=ACCENT)
    arrow(d, [(154, 264), (154, 128)], color=ACCENT)
    label(d, 40, 186, "去注册", 14, (150, 110, 50))
    label(d, 162, 186, "返回登录", 14, (150, 110, 50))

    arrow(d, [(240, 92), (315, 92)])
    label(d, 244, 58, "校验通过", 14, TEAL)
    arrow(d, [(505, 92), (600, 92)])
    label(d, 512, 58, "卡片", 14, TEAL)

    arrow(d, [(662, 128), (662, 214)], color=ACCENT)
    arrow(d, [(738, 214), (738, 128)], color=ACCENT)
    label(d, 590, 158, "点缩略图", 14, (150, 110, 50))
    label(d, 744, 158, "返回", 14, (150, 110, 50))

    # 主界面 → 画板（竖直）与 → 小游戏列表（折线，避开画板）
    arrow(d, [(410, 128), (410, 384)])
    label(d, 356, 250, "卡片", 14, TEAL)
    arrow(d, [(505, 110), (552, 110), (552, 420), (600, 420)])
    label(d, 506, 122, "卡片", 14, TEAL)

    # 小游戏列表 → 四款游戏：一条横线加四个短箭头，比四条长斜线干净
    d.line([(700 * S, 456 * S), (700 * S, 556 * S)], fill=TEAL, width=int(2 * S))
    d.line([(95 * S, 556 * S), (700 * S, 556 * S)], fill=TEAL, width=int(2 * S))
    for cx, _, _ in games:
        arrow(d, [(cx, 556), (cx, 606)], head=8)
    label(d, 470, 522, "四个入口卡片", 14, TEAL)

    label(d, 40, 700, "注：除登录页外左上角统一返回；棋局页面返回小游戏列表，其余页面返回主界面。",
          13, MUTED)
    save(im, "fig2-2-state.png")


# ---------------------------------------------------------------- 图3-1 系统连接
def fig_hw():
    W, H = 900, 760
    im = canvas(W, H)
    d = drawer(im)

    d.rounded_rectangle([(330 * S), (250 * S), (620 * S), (470 * S)], radius=12 * S,
                        fill=SOFT, outline=TEAL, width=int(2.5 * S))
    text_c(d, 475, 278, "S5P6818 应用处理器", 23, INK, True)
    text_c(d, 475, 306, "Cortex-A53 ×8，1.4 GHz", 17, MUTED)
    d.line([(346 * S, 324 * S), (604 * S, 324 * S)], fill=(176, 202, 196), width=int(1.4 * S))
    for i, s in enumerate(["显示控制器", "I2C / GPIO", "UART ×3", "DDR3 控制器"]):
        text_c(d, 475, 344 + i * 30, s, 16, MUTED)

    box(d, 250, 110, 220, 78, "DDR3 SDRAM", "1 GB 运行内存", tsize=21, ssize=16)
    box(d, 620, 110, 220, 78, "eMMC", "8 GB 板载存储", tsize=21, ssize=16)
    arrow(d, [(250, 149), (250, 250)], color=LINE)
    arrow(d, [(620, 149), (620, 250)], color=LINE)
    label(d, 258, 186, "16 bit 总线", 15)
    label(d, 628, 186, "SDIO", 15)

    box(d, 796, 300, 190, 86, "7 寸 TFT LCD", "800 × 480", tsize=21, ssize=16)
    arrow(d, [(620, 300), (700, 300)], color=TEAL)
    label(d, 634, 272, "RGB 时序", 15, TEAL)
    box(d, 796, 520, 190, 86, "电容触摸屏", "GSLX680", tsize=21, ssize=16)
    arrow(d, [(700, 520), (620, 520)], color=ACCENT)
    label(d, 622, 490, "坐标上报", 15, (150, 110, 50))
    d.line([(760 * S, 344 * S), (760 * S, 476 * S)], fill=MUTED, width=int(1.4 * S))
    label(d, 764, 396, "贴", 14, MUTED)
    label(d, 764, 414, "合", 14, MUTED)

    box(d, 110, 232, 170, 86, "电源适配器", "DC 5 V / 2 A", tsize=20, ssize=15)
    arrow(d, [(196, 246), (330, 288)], color=LINE)
    label(d, 206, 302, "整板供电", 14)
    box(d, 110, 470, 170, 86, "调试主机", "USB 转串口", tsize=20, ssize=15)
    arrow(d, [(196, 462), (330, 452)], color=LINE)
    label(d, 202, 484, "UART0", 14)

    box(d, 475, 650, 220, 80, "以太网口 RJ45", "TFTP / HTTP 传程序", tsize=20, ssize=15)
    arrow(d, [(475, 610), (475, 470)], color=LINE)
    label(d, 486, 528, "RMII", 15)
    save(im, "fig3-1-hardware.png")


# ---------------------------------------------------------------- 图3-2 软件逻辑
def fig_sw():
    W, H = 820, 664
    im = canvas(W, H)
    d = drawer(im)
    layers = [
        (78, "应用层", "登录注册 · 主界面 · 电子相册 · 2048 · 创意画板",
         "LoginSystem.c / AlbumSystem.c / GameSystem.c / WhiteboardSystem.c"),
        (200, "框架层", "App 状态、页面调度、命中判定、增量刷新",
         "System.c + terminal.h（Page / Event / ui_nearest）"),
        (322, "规则层", "2048 棋盘压缩、合并、计分、胜负判断",
         "game.c + game.h，不引用界面与设备"),
        (444, "基础层", "矩形、圆、线段、图像缩放、BMP 读写、文字",
         "display.h → lcd.c / file_manager.c / print.c"),
        (566, "设备层", "framebuffer 提交、evdev 读取、坐标归一化",
         "platform.h → platform.c（Linux 与 Win32）"),
    ]
    bw, bh, cx = 600, 88, 480
    for y, name, sub, files in layers:
        left = cx - bw / 2
        d.rounded_rectangle([(left * S), ((y - bh / 2) * S), ((left + bw) * S), ((y + bh / 2) * S)],
                            radius=10 * S, fill=CARD, outline=LINE, width=int(2 * S))
        d.rounded_rectangle([(left * S), ((y - bh / 2) * S), ((left + 13) * S), ((y + bh / 2) * S)],
                            radius=5 * S, fill=TEAL)
        text_c(d, cx, y - 14, sub, 21, INK, True)
        text_c(d, cx, y + 22, files, 15, MUTED)
        text_c(d, 84, y, name, 21, TEAL, True)

    for y in (78, 200, 322, 444):
        arrow(d, [(cx, y + bh / 2), (cx, y + 122 - bh / 2)], color=MUTED, lw=1.6, head=8)

    label(d, 60, 632, "调用方向：上层只依赖下层接口，页面模块不直接访问设备节点。", 16, MUTED)
    save(im, "fig3-2-software.png")


if __name__ == "__main__":
    fig_flow()
    fig_state()
    fig_hw()
    fig_sw()
