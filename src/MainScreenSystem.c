/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   MainScreenSystem.c
Author:      汪贤云        Version: 1.0.1       Date: 2026-11-19
Description: 主界面。显示系统时间与三张功能入口卡片（电子相册、小游戏、创意画板）。
             小游戏的入口在第二张卡片上，它打开的是一个游戏列表页，四款游戏都在
             列表里；这样将来再加游戏只需改列表，不必重排主界面的卡片。
Others:      时钟取 Linux 本机时间，每秒由 app_tick 触发一次重绘，不需要单独线程。
Function List:
  1. home_render 绘制主界面：时间、日期、三张卡片与底部说明
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 初版，精简原工程的桌面，保留三个功能入口
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 第二张卡片改开游戏列表；注释改为中文规范格式
******************************************************************************/
#include "terminal.h"
#include <stdio.h>

/* 绘制主界面。时钟与日期由本机 Linux 时间格式化得到；
   三张卡片的图标全部用基本图元直接画，不额外占用位图资源。 */
void home_render(App *app)
{
    Image *screen = &app->screen;

    draw_rect(screen, (Rect){0, 0, 800, 480}, UI_BG);
    draw_text(screen, 32, 24, 34, UI_INK, "交互式多媒体终端");
    ui_button(app, (Rect){648, 16, 132, 60}, "退出", false);

    /* 取本机时间并格式化。若 localtime 失败则保留占位符 "--:--:--"，
       不让界面出现半个时钟 */
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char clockText[64] = "--:--:--";
    char dateText[64] = "";
    if (local) {
        strftime(clockText, sizeof(clockText), "%H:%M:%S", local);
        strftime(dateText, sizeof(dateText), "%Y / %m / %d", local);
    }
    draw_text(screen, 32, 82, 42, UI_TEAL, clockText);
    draw_text(screen, 240, 96, 21, UI_MUTED, dateText);

    /* 卡片文案与配色。三种强调色分别对应相册、游戏、画板，
       和进入子页面后的页头颜色保持一致 */
    const char *names[] = {"电子相册", "小游戏", "创意画板"};
    const char *notes[] = {"浏览照片与画板作品", "四款小游戏任你挑", "随手画下你的灵感"};
    const uint32_t accents[] = {0x227C77, 0xC58636, 0xBA6256};

    for (int i = 0; i < 3; ++i) {
        int x = 32 + i * 253;

        /* 卡片白底与顶部色条 */
        draw_rect(screen, (Rect){x, 158, 230, 236}, 0xFFFFFF);
        draw_rect(screen, (Rect){x, 158, 230, 6}, accents[i]);

        if (i == 0) {
            /* 相册：一张风景缩略图的示意画法（太阳加两座山） */
            draw_rect(screen, (Rect){x + 25, 188, 112, 69}, 0xD9EDE4);
            draw_disc(screen, x + 109, 207, 10, accents[i]);
            draw_line(screen, x + 35, 247, x + 66, 219, 3, accents[i]);
            draw_line(screen, x + 66, 219, x + 99, 247, 3, accents[i]);
        } else if (i == 1) {
            /* 游戏：一个迷你棋盘，上面同时有棋子和叉。
               这一张图就表达了「不止一款游戏」，无需新增位图或第五个图标位 */
            draw_rect(screen, (Rect){x + 25, 184, 116, 70}, 0xF3EDDF);
            draw_line(screen, x + 83, 190, x + 83, 248, 2, 0x9A7A50);
            draw_line(screen, x + 31, 219, x + 135, 219, 2, 0x9A7A50);
            draw_disc(screen, x + 57, 202, 9, 0x243A3B);
            draw_disc(screen, x + 57, 202, 4, 0x4C6567);
            draw_line(screen, x + 100, 228, x + 118, 246, 4, accents[i]);
            draw_line(screen, x + 118, 228, x + 100, 246, 4, accents[i]);
        } else {
            /* 画板：一支斜置的画笔加一个颜料点 */
            draw_line(screen, x + 34, 247, x + 104, 196, 6, accents[i]);
            draw_disc(screen, x + 130, 240, 12, 0xE5B149);
        }

        /* 卡片文字：名称、说明、进入提示 */
        draw_text(screen, x + 24, 281, 31, UI_INK, names[i]);
        draw_text(screen, x + 24, 327, 20, UI_MUTED, notes[i]);
        draw_text(screen, x + 24, 362, 18, accents[i], "点击进入  >");
    }

    /* 底部分隔线与平台说明 */
    draw_rect(screen, (Rect){32, 428, 736, 1}, 0xD8DED7);
    draw_text(screen, 32, 444, 18, UI_MUTED, "GEC6818  /  800 × 480");
    draw_text(screen, 566, 444, 18, UI_MUTED, "照片 · 游戏 · 绘画");
}
