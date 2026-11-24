/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   GomokuSystem.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 五子棋页面。棋盘为 15 路 × 15 行的交叉点，点距 25 像素，一个点约一个
             手指宽；点击一律就近吸附到最近的交叉点，整块棋盘都是命中目标，边缘
             附近的点击也不拒绝。玩家执黑先行，电脑在其落子后立即应手，因此没有
             需要停留的「思考」状态。绘制部分包括木色棋盘底、横竖网格线、五个
             星位、棋子与最后落子标记，以及胜负提示、手数、右侧图例和未命中回显。
Others:      最后落子用一个小圆点标出，颜色取落子方：黑方为金色、白方为青色，
             方便回看对手上一手的位置。规则与电脑走法在 boardgames.c，本文件
             只负责绘制与输入。
Function List:
  1. point_at      把屏幕坐标换算成交叉点下标，带边缘吸附
  2. stone_count   统计盘上已落的棋子数
  3. draw_stone    画一枚棋子，黑子带高光、白子为木色描边
  4. gomoku_render 绘制棋盘、棋子、胜负提示与右侧图例
  5. gomoku_event  落子与重新开局，玩家落子后电脑立即应手
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

/* 棋盘左边界横坐标 */
#define GX 30
/* 棋盘上边界纵坐标 */
#define GY 96
/* 交叉点间距，十五个交叉点占十四个格距 */
#define GC 25
/* 棋子半径 */
#define STONE 10

/* 把屏幕坐标换算成交叉点下标。
   说明：采用就近吸附而不是严格包含，点在棋盘外半个格距以内仍算落在最靠边的
         交叉点；整块棋盘都是一个命中目标，拒绝边缘附近的点击没有意义。 */
static int point_at(int x, int y)
{
    if (x < GX - GC / 2 || y < GY - GC / 2) { return -1; }
    /* 四舍五入到最近的列与行 */
    int col = (x - GX + GC / 2) / GC;
    int row = (y - GY + GC / 2) / GC;
    if (col < 0 || col >= GOMOKU_N || row < 0 || row >= GOMOKU_N) { return -1; }
    return row * GOMOKU_N + col;
}

/* 统计盘上已落的棋子数，用于显示第几手 */
static int stone_count(const Gomoku *game)
{
    int count = 0;
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (game->cell[i]) { ++count; }
    }
    return count;
}

/* 画一枚棋子：黑子为深色实心并带一点高光，白子为木色底加浅色内圈 */
static void draw_stone(App *app, int cx, int cy, int side)
{
    if (side == 1) {
        draw_disc(&app->screen, cx, cy, STONE, 0x243A3B);
        draw_disc(&app->screen, cx - 3, cy - 3, 3, 0x4C6567);
    } else {
        draw_disc(&app->screen, cx, cy, STONE, 0xBFB096);
        draw_disc(&app->screen, cx, cy, STONE - 1, 0xFBF7EC);
    }
}

/* 绘制五子棋页面：棋盘、星位、棋子、最后落子标记、胜负提示与右侧图例。
   棋子值 1 表示玩家的黑子，2 表示电脑的白子，0 表示空点。 */
void gomoku_render(App *app)
{
    char line[64];
    const Gomoku *game = &app->gomoku;

    ui_header_sized(app, "五子棋", 26);
    ui_button(app, (Rect){596, 16, 180, 60}, "重新开始", true);

    /* 木色棋盘底 */
    draw_rect(&app->screen, (Rect){18, 84, 376, 376}, 0xE7D3AC);
    /* 横竖各十五条网格线 */
    for (int i = 0; i < GOMOKU_N; ++i) {
        int p = GX + i * GC;
        draw_line(&app->screen, GX, GY + i * GC, GX + (GOMOKU_N - 1) * GC, GY + i * GC, 1, 0x9A7A50);
        draw_line(&app->screen, p, GY, p, GY + (GOMOKU_N - 1) * GC, 1, 0x9A7A50);
    }
    /* 五个星位 */
    static const int star[5][2] = {{3, 3}, {11, 3}, {7, 7}, {3, 11}, {11, 11}};
    for (int i = 0; i < 5; ++i) {
        draw_disc(&app->screen, GX + star[i][0] * GC, GY + star[i][1] * GC, 3, 0x6E5334);
    }

    /* 逐点画棋子 */
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (!game->cell[i]) { continue; }
        draw_stone(app, GX + (i % GOMOKU_N) * GC, GY + (i / GOMOKU_N) * GC, game->cell[i]);
    }
    /* 最后落子用小圆点标出，颜色取落子方 */
    if (game->last >= 0) {
        int x = GX + (game->last % GOMOKU_N) * GC;
        int y = GY + (game->last / GOMOKU_N) * GC;
        draw_disc(&app->screen, x, y, 3, game->cell[game->last] == 1 ? 0xE9C46A : 0x227C77);
    }

    /* 状态文字：胜负优先，没结束则显示轮到谁 */
    const char *status;
    uint32_t color = UI_TEAL;
    if (game->winner == 1) { status = "你赢了！"; }
    else if (game->winner == 2) { status = "电脑赢了"; color = 0xAD4E3D; }
    else if (game->winner == 3) { status = "平局"; color = UI_MUTED; }
    else { status = game->turn == 1 ? "轮到你（黑）" : "电脑落子中"; }

    draw_text(&app->screen, 412, 96, 28, color, status);
    /* 手数由棋盘反推，不另存计数，避免状态不同步 */
    snprintf(line, sizeof(line), "第 %d 手", stone_count(game));
    draw_text(&app->screen, 412, 138, 18, UI_MUTED, line);

    /* 右侧图例：黑白各一枚，说明谁先行 */
    draw_stone(app, 422, 190, 1);
    draw_text(&app->screen, 440, 178, 18, UI_INK, "你执黑先行");
    draw_stone(app, 422, 226, 2);
    draw_text(&app->screen, 440, 214, 18, UI_INK, "电脑执白");

    draw_text(&app->screen, 412, 276, 16, UI_MUTED, "点击交叉点落子");
    draw_text(&app->screen, 412, 302, 16, UI_MUTED, "横竖斜任意方向连成");
    draw_text(&app->screen, 412, 328, 16, UI_MUTED, "五子即获胜");

    if (game->winner) {
        draw_text(&app->screen, 412, 388, 16, 0x9C8F7B, "点右上角重新开局");
    }

    if (app->missTap) {
        snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 596, 444, 15, 0xAD4E3D, line);
    }
}

/* 处理五子棋的输入：按下时记录起点，只在抬起时判定。
   顺序是先判「重新开始」，再判是否终局或轮到电脑，最后换算交叉点落子；
   玩家落子成功后电脑立即应手，因此每次玩家只走一步。 */
void gomoku_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) { app->lastX = event.x; app->lastY = event.y; return; }
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;

    /* 重新开局 */
    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        gomoku_start(&app->gomoku, (uint32_t)time(NULL));
        app->dirty = true;
        return;
    }
    Gomoku *game = &app->gomoku;
    /* 对局已结束或还没轮到玩家时，只请求重绘不处理落子 */
    if (game->winner || game->turn != 1) { app->dirty = true; return; }

    /* 先按抬起点换算交叉点，未命中再退回按下点 */
    int index = point_at(event.x, event.y);
    if (index < 0) { index = point_at(app->downX, app->downY); }
    if (index < 0 || !gomoku_place(game, index)) {
        app->missTap = true;
        app->dirty = true;
        return;
    }
    /* 玩家这一手没有直接取胜时，电脑立即应手 */
    if (!game->winner) {
        int reply = -1;
        gomoku_ai(game, &reply);
    }
    app->dirty = true;
}
