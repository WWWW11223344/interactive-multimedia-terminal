/* 小游戏入口, 2026-11-20: four cards, one per game.
 *
 * Home used to send the second card straight into 2048. With three more games the
 * second card now opens this list, so adding a fifth game means one more entry here
 * instead of reshuffling the home screen. */
#include "terminal.h"
#include <stdio.h>

static const Rect G_CARD[4] = {
    {32, 104, 360, 152}, {408, 104, 360, 152},
    {32, 272, 360, 152}, {408, 272, 360, 152}
};
static const char *const G_NAME[4] = {"2048", "五子棋", "井字棋", "中国象棋"};
static const char *const G_NOTE[4] = {
    "滑动合并，挑战 2048",
    "五子连珠，电脑执白",
    "三子连线，电脑不会输",
    "双人对弈，红方先行"
};
static const uint32_t G_ACCENT[4] = {0xC58636, 0x227C77, 0x487BBC, 0xBA6256};

/* 2×2 数字块 */
static void icon_2048(App *app, int x, int y)
{
    static const uint32_t fill[4] = {0xEDE3CF, 0xE4D5B8, 0xE4D5B8, 0xDCC9A4};
    static const char *const label[4] = {"2", "4", "8", "16"};
    for (int i = 0; i < 4; ++i) {
        Rect cell = {x + (i % 2) * 33, y + (i / 2) * 33, 30, 30};
        draw_rect(&app->screen, cell, fill[i]);
        draw_text(&app->screen, cell.x + 7, cell.y + 5, 18, 0x6B5741, label[i]);
    }
}

/* 5×5 棋盘上的三颗子 */
static void icon_gomoku(App *app, int x, int y)
{
    draw_rect(&app->screen, (Rect){x, y, 62, 62}, 0xE7D3AC);
    for (int i = 0; i < 5; ++i) {
        draw_line(&app->screen, x + 6 + i * 12, y + 6, x + 6 + i * 12, y + 56, 1, 0x9A7A50);
        draw_line(&app->screen, x + 6, y + 6 + i * 12, x + 56, y + 6 + i * 12, 1, 0x9A7A50);
    }
    draw_disc(&app->screen, x + 30, y + 30, 7, 0x243A3B);
    draw_disc(&app->screen, x + 18, y + 18, 7, 0x243A3B);
    draw_disc(&app->screen, x + 42, y + 42, 7, 0xFBF7EC);
    draw_disc(&app->screen, x + 42, y + 18, 7, 0xFBF7EC);
}

/* 井字格 + 一个叉一个圈 */
static void icon_toe(App *app, int x, int y)
{
    for (int i = 1; i < 3; ++i) {
        draw_line(&app->screen, x + 6 + i * 18, y + 4, x + 6 + i * 18, y + 58, 2, 0x8A8F8C);
        draw_line(&app->screen, x + 4, y + 6 + i * 18, x + 58, y + 6 + i * 18, 2, 0x8A8F8C);
    }
    draw_line(&app->screen, x + 9, y + 9, x + 21, y + 21, 3, 0x487BBC);
    draw_line(&app->screen, x + 21, y + 9, x + 9, y + 21, 3, 0x487BBC);
    draw_disc(&app->screen, x + 39, y + 39, 9, 0xBA6256);
    draw_disc(&app->screen, x + 39, y + 39, 6, 0xFFFFFF);
}

/* 一枚棋子 */
static void icon_chess(App *app, int x, int y)
{
    draw_disc(&app->screen, x + 30, y + 30, 26, 0xC9A87A);
    draw_disc(&app->screen, x + 30, y + 30, 24, 0xF6E7C8);
    draw_disc(&app->screen, x + 30, y + 30, 19, 0xC9A87A);
    draw_disc(&app->screen, x + 30, y + 30, 18, 0xF6E7C8);
    draw_text(&app->screen, x + 18, y + 16, 26, 0xAD3B2C, "将");
}

void games_render(App *app)
{
    ui_header(app, "小游戏");
    for (int i = 0; i < 4; ++i) {
        Rect card = G_CARD[i];
        draw_rect(&app->screen, card, 0xFFFFFF);
        draw_rect(&app->screen, (Rect){card.x, card.y, card.w, 6}, G_ACCENT[i]);
        draw_rect(&app->screen, (Rect){card.x, card.y + card.h - 1, card.w, 1}, 0xDED8C8);
        switch (i) {
        case 0: icon_2048(app, card.x + 24, card.y + 44); break;
        case 1: icon_gomoku(app, card.x + 24, card.y + 44); break;
        case 2: icon_toe(app, card.x + 24, card.y + 44); break;
        default: icon_chess(app, card.x + 24, card.y + 44); break;
        }
        draw_text(&app->screen, card.x + 112, card.y + 26, 30, UI_INK, G_NAME[i]);
        draw_text(&app->screen, card.x + 112, card.y + 72, 17, UI_MUTED, G_NOTE[i]);
        draw_text(&app->screen, card.x + 112, card.y + 108, 16, G_ACCENT[i], "点击进入  >");
    }
    draw_text(&app->screen, 24, 444, 15, UI_MUTED, "四款小游戏：2048 · 五子棋 · 井字棋 · 中国象棋");
    if (app->missTap) {
        char line[48];
        snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 600, 444, 15, 0xAD4E3D, line);
    }
}

void games_event(App *app, Event event)
{
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
