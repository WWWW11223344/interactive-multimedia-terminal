/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   GamesMenuSystem.c
Author:      汪贤云        Version: 1.0.1       Date: 2026-11-20
Description: 小游戏列表页。四款游戏各占一行，点击任意一行进入对应游戏。
             主界面的第二张卡片原本直接进 2048，四款游戏加入后改为进这个列表；
             再增加游戏只需在本文件的入口数组里加一项，不必重排主界面。
Others:      列表从 y=260 开始竖排，是因为这块面板在画面中部有一条横带读不出坐标。
             实测下边界约在 y=257（键盘 4/5/6 从 258 起可用，旧的 2×2 布局上排
             104..256 点不动），而画板工具栏在 16..76 又能按，说明失效带位于两者
             之间。四行 50 像素高，占 260..478，不必猜失效带的上边界就能全部避开。
Function List:
  1. games_render 绘制列表：页头、说明文字、四个入口
  2. games_event  判断点中了哪一行并跳转到对应游戏
  3. draw_badge   绘制左侧带序号的圆点
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版，四款游戏排成 2×2
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 改为竖排四行以避开面板失效带，图标换成带序号的圆点；
                   注释改为中文规范格式
******************************************************************************/
#include "terminal.h"
#include <stdio.h>

/* 四行入口的区域，全部位于 y 260 以下，避开面板读不出坐标的那条横带 */
static const Rect G_CARD[4] = {
    {40, 260, 720, 50},
    {40, 316, 720, 50},
    {40, 372, 720, 50},
    {40, 428, 720, 50}
};

/* 游戏名称，与 G_CARD、G_NOTE、G_ACCENT、dest 四个数组按相同下标对应 */
static const char *const G_NAME[4] = {"2048", "五子棋", "井字棋", "中国象棋"};

/* 每款游戏的一句话说明 */
static const char *const G_NOTE[4] = {
    "滑动合并，挑战 2048",
    "五子连珠，电脑执白",
    "三子连线，电脑不会输",
    "双人对弈，红方先行"
};

/* 四行的强调色，与名称、说明按相同下标对应 */
static const uint32_t G_ACCENT[4] = {0xC58636, 0x227C77, 0x487BBC, 0xBA6256};

/* 绘制一行左侧的序号圆点。
   参数：cx/cy 为圆心；index 为从 0 开始的序号。
   说明：一行只有 50 像素高，放不下原来 63 像素的图标，因此改用带序号的圆点；
         序号同时演示了推荐的游玩顺序。 */
static void draw_badge(App *app, int cx, int cy, int index)
{
    char label[2] = {(char)('1' + index), '\0'};
    int width;

    draw_disc(&app->screen, cx, cy, 13, G_ACCENT[index]);
    width = text_width(label, 18);
    draw_text(&app->screen, cx - width / 2, cy - 9, 18, 0xFFFFFF, label);
}

/* 绘制列表页。列表上方的横带完全不能响应触摸，因此那里只放两行说明文字，
   不放任何按钮——放在那里看得见却点不动，比空着更容易让人困惑。 */
void games_render(App *app)
{
    ui_header(app, "小游戏");

    draw_text(&app->screen, 40, 186, 22, UI_INK, "选择一款开始");
    draw_text(&app->screen, 40, 218, 15, UI_MUTED, "四款游戏，点任意一行进入");

    for (int i = 0; i < 4; ++i) {
        Rect card = G_CARD[i];

        /* 白底、左边色条、下边细线：色条让人在读到名字之前先靠颜色分清四行 */
        draw_rect(&app->screen, card, 0xFFFFFF);
        draw_rect(&app->screen, (Rect){card.x, card.y, 6, card.h}, G_ACCENT[i]);
        draw_rect(&app->screen, (Rect){card.x, card.y + card.h - 1, card.w, 1}, 0xDED8C8);

        draw_badge(app, card.x + 38, card.y + 25, i);
        draw_text(&app->screen, card.x + 66, card.y + 12, 25, UI_INK, G_NAME[i]);
        draw_text(&app->screen, card.x + 226, card.y + 18, 17, UI_MUTED, G_NOTE[i]);
        draw_text(&app->screen, card.x + card.w - 32, card.y + 11, 22, G_ACCENT[i], ">");
    }

    /* 点空时把坐标显示出来，便于区分「点偏了」与「面板没上报」 */
    if (app->missTap) {
        char line[48];
        snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 600, 244, 15, 0xAD4E3D, line);
    }
}

/* 处理列表页的点击事件：只有抬起的那一刻才判定，按下与移动不响应。
   命中任意一行就跳到对应游戏；都没命中则把 missTap 置真，让页面显示落点坐标。 */
void games_event(App *app, Event event)
{
    /* 四个目标页面，与 G_CARD 按相同下标对应 */
    static const Page dest[4] = {PAGE_2048, PAGE_GOMOKU, PAGE_TOE, PAGE_CHESS};

    if (event.type == EVENT_DOWN) { return; }
    if (event.type != EVENT_UP) { return; }

    app->missTap = false;
    for (int i = 0; i < 4; ++i) {
        if (ui_clicked(app, event, G_CARD[i])) {
            app->page = dest[i];
            app->notice[0] = '\0';
            app->dirty = true;
            return;
        }
    }
    app->missTap = true;
    app->dirty = true;
}
