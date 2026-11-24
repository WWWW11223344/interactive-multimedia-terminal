/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   ToeSystem.c
Author:      汪贤云        Version: 1.0.2       Date: 2026-11-20
Description: 井字棋页面。玩家执 X 先行，电脑用全搜索应对，因此玩家最好也只能下成
             平局。棋盘取 336 像素见方，每格 112 像素——常规尺寸的棋盘在这块面板上
             几乎全是缝，而 112 像素大于触摸芯片的纵向误差，一次点击不会落到隔壁格。
             几何量集中定义：BX/BY 为棋盘左上角，BC 为格宽；格心等于
             BX + 列号 * BC + BC / 2。分隔线画在格子边界上，保证视觉网格与命中判定
             用的是同一套坐标。
Others:      棋子半径必须小于 32。底层 draw_disc 对超过 32 的半径会静默返回、不画
             任何像素：曾经用 34 时电脑的 O 全部消失，而玩家的 X 由线条绘制半径只有
             5，右侧图例半径 16 也正常，现象就是「只看见自己的叉」。现取 30。
Function List:
  1. toe_render     绘制棋盘、棋子、胜负提示与右侧图例
  2. toe_event      落子与重新开局，玩家落子后电脑立即应手
  3. square_at      把屏幕坐标换算成格子下标，带边缘吸附
  4. draw_cross     画叉
  5. draw_circle    画圆环
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版，接入全搜索电脑走法
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 修复棋子半径超过底层上限导致电脑的 O 不显示；注释改为中文
******************************************************************************/
#include "terminal.h"
#include <stdio.h>

/* 棋盘左边界 */
#define BX 44
/* 棋盘上边界 */
#define BY 92
/* 每格边长，三格共 336 像素 */
#define BC 112
/* 盘上 X 与 O 的半径。底层画圆函数拒绝大于 32 的半径且不做提示，这里必须留在其内 */
#define MARK 30
/* 右侧图例里 X 与 O 的半径 */
#define LEG 16

/* 求某列的格心横坐标 */
static int cell_cx(int col) { return BX + col * BC + BC / 2; }
/* 求某行的格心纵坐标 */
static int cell_cy(int row) { return BY + row * BC + BC / 2; }

/* 把屏幕坐标换算成格子下标。
   说明：这里采用吸附而不是严格包含——点在棋盘外 60 像素以内仍算点在最靠边的格子。
         直接拒绝的话，看上去只会像面板又坏了一块。 */
static int square_at(int x, int y)
{
    int col = (x - BX) / BC;
    int row = (y - BY) / BC;

    /* 超出上边界或左边界半个格距以外才算未命中 */
    if (x < BX - BC / 2 || y < BY - BC / 2) { return -1; }
    if (col < 0 || col > 2 || row < 0 || row > 2) { return -1; }
    return row * 3 + col;
}

/* 画叉。盘上的叉用 5 像素线宽，图例里的用 3 像素 */
static void draw_cross(App *app, int cx, int cy, int half, uint32_t color)
{
    int weight = half > 20 ? 5 : 3;

    draw_line(&app->screen, cx - half, cy - half, cx + half, cy + half, weight, color);
    draw_line(&app->screen, cx + half, cy - half, cx - half, cy + half, weight, color);
}

/* 画圆环：先画实心圆，再用棋盘底色盖住内核，剩下的就是一圈环。
   半径大于 20 时环宽 8 像素，小图例取 5 像素。 */
static void draw_circle(App *app, int cx, int cy, int radius, uint32_t color)
{
    draw_disc(&app->screen, cx, cy, radius, color);
    draw_disc(&app->screen, cx, cy, radius - (radius > 20 ? 8 : 5), 0xFBF7EC);
}

/* 绘制井字棋页面：棋盘、棋子、胜负线、右侧状态与图例。
   棋子值 1 表示玩家的叉，2 表示电脑的圆环，0 表示空格。 */
void toe_render(App *app)
{
    char line[64];
    const Toe *game = &app->toe;

    ui_header_sized(app, "井字棋", 26);
    ui_button(app, (Rect){596, 16, 180, 60}, "重新开始", true);

    /* 棋盘底色与两条分隔线，分隔线落在格子边界上 */
    draw_rect(&app->screen, (Rect){BX, BY, BC * 3, BC * 3}, 0xFBF7EC);
    for (int i = 1; i < 3; ++i) {
        draw_rect(&app->screen, (Rect){BX + i * BC - 2, BY, 4, BC * 3}, 0xD0C8B6);
        draw_rect(&app->screen, (Rect){BX, BY + i * BC - 2, BC * 3, 4}, 0xD0C8B6);
    }

    /* 逐格画棋子 */
    for (int i = 0; i < TOE_CELLS; ++i) {
        int cx = cell_cx(i % 3);
        int cy = cell_cy(i / 3);

        if (game->cell[i] == 1) { draw_cross(app, cx, cy, MARK, 0x487BBC); }
        else if (game->cell[i] == 2) { draw_circle(app, cx, cy, MARK, 0xBA6256); }
    }

    /* 有胜负时把三连的那条线画成金色 */
    int a = 0, b = 0, c = 0;
    if (toe_win_line(game, &a, &b, &c)) {
        draw_line(&app->screen, cell_cx(a % 3), cell_cy(a / 3),
            cell_cx(c % 3), cell_cy(c / 3), 5, 0xE0B24A);
    }

    /* 状态文字：胜负优先，没结束则显示轮到谁 */
    const char *status;
    uint32_t color = UI_TEAL;

    if (game->result == 1) { status = "你赢了！"; }
    else if (game->result == 2) { status = "电脑赢了"; color = 0xAD4E3D; }
    else if (game->result == 3) { status = "平局"; color = UI_MUTED; }
    else { status = game->turn == 1 ? "轮到你（X）" : "电脑思考中"; }

    draw_text(&app->screen, 412, 96, 28, color, status);

    /* 已落子数由棋盘反推，不另存计数，避免状态不同步 */
    int moves = 0;
    for (int i = 0; i < TOE_CELLS; ++i) { if (game->cell[i]) { ++moves; } }
    snprintf(line, sizeof(line), "第 %d 手", moves);
    draw_text(&app->screen, 412, 138, 18, UI_MUTED, line);

    /* 右侧图例，让玩家一眼分清谁执叉谁执圈 */
    draw_cross(app, 418, 186, LEG, 0x487BBC);
    draw_text(&app->screen, 446, 176, 18, UI_INK, "你执 X 先行");
    draw_circle(app, 418, 232, LEG, 0xBA6256);
    draw_text(&app->screen, 446, 222, 18, UI_INK, "电脑执 O");

    draw_text(&app->screen, 412, 286, 16, UI_MUTED, "点击空格落子");
    draw_text(&app->screen, 412, 312, 16, UI_MUTED, "横、竖、斜任意一线");
    draw_text(&app->screen, 412, 338, 16, UI_MUTED, "连成三个即获胜");

    if (game->result) {
        draw_text(&app->screen, 412, 392, 16, 0x9C8F7B, "点右上角重新开局");
    }
    if (app->missTap) {
        snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 596, 444, 15, 0xAD4E3D, line);
    }
}

/* 处理井字棋的输入：按下时记录起点，只在抬起时判定。
   顺序是先判「重新开始」，再判是否已经结束或轮到电脑，最后换算格子并落子；
   玩家落子成功后电脑立即全搜索应手，因此每次玩家只走一步。 */
void toe_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) {
        app->lastX = event.x;
        app->lastY = event.y;
        return;
    }
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;

    /* 重新开局 */
    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        toe_start(&app->toe);
        app->dirty = true;
        return;
    }

    Toe *game = &app->toe;

    /* 对局已结束或还没轮到玩家时，只请求重绘不处理落子 */
    if (game->result || game->turn != 1) {
        app->dirty = true;
        return;
    }

    /* 先按抬起点换算格子，未命中再退回按下点 */
    int index = square_at(event.x, event.y);
    if (index < 0) { index = square_at(app->downX, app->downY); }
    if (index < 0 || !toe_place(game, index)) {
        app->missTap = true;
        app->dirty = true;
        return;
    }

    /* 玩家这一手没有直接取胜时，电脑立即应手 */
    if (!game->result) {
        int reply = -1;
        toe_ai(game, &reply);
    }
    app->dirty = true;
}
