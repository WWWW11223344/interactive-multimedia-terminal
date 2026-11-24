/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   ChessSystem.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 中国象棋页面，两人在同一块屏幕上轮流对弈。棋盘 9 路 × 10 行，格距
             37 像素；先点自己的棋子选中，可走点以绿点标出、可吃目标以红圈标出，
             再点目标格落子。绘制顺序是先画可走点再画棋子，避免圆点盖住字。规则层
             在 boardgames.c，本文件只负责棋盘绘制与点击输入。
Others:      格距从 40 收到 37，是为了让第 0 行棋子（半径 16）不再压住页眉那条
             分隔线，同时末行仍留出下边距。红黑两方用不同颜色的圆环，因为同一块
             木色底上单靠字色区分远看几乎一样；炮和车外观相近，必须靠可走点提示
             才能分清，新手也才能分辨被蹩住马腿的马。
Function List:
  1. square_at    把屏幕坐标换算成棋盘格下标，带边缘吸附
  2. px           求某列格心的横坐标
  3. py           求某行格心的纵坐标
  4. draw_grid    绘制棋盘网格、九宫斜线与河界
  5. draw_piece   画一枚棋子，选中时加青色底
  6. chess_render 绘制棋盘、棋子、可走点提示与对局状态
  7. chess_event  处理选子、改选、取消与落子
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

/* 棋盘几何：9 路 × 10 行，每格 37px。格距从 40 收到 37 是为了让第 0 行棋子（半径 16）
   不再压住页眉那条分隔线，同时末行仍留出 30px 下边距。 */
/* 棋盘左边界横坐标 */
#define CX0 30
/* 棋盘上边界纵坐标 */
#define CY0 100
/* 相邻两路／两行之间的格距 */
#define CC 37
/* 棋子半径 */
#define PIECE 16

/* 红方七种棋子的名称，下标与 chess_type 返回值减一对应 */
static const char *const G_RED[7] = {"帅", "仕", "相", "马", "车", "炮", "兵"};
/* 黑方七种棋子的名称，顺序与红方一致 */
static const char *const G_BLACK[7] = {"将", "士", "象", "马", "车", "炮", "卒"};

/* 把屏幕坐标换算成棋盘格下标。
   说明：点在棋盘外半个格距以内仍算落在最靠边的格子，避免边界附近的点击落空。 */
static int square_at(int x, int y)
{
    if (x < CX0 - CC / 2 || y < CY0 - CC / 2) { return -1; }
    /* 四舍五入到最近的路与行 */
    int col = (x - CX0 + CC / 2) / CC;
    int row = (y - CY0 + CC / 2) / CC;
    if (col < 0 || col >= CHESS_COLS || row < 0 || row >= CHESS_ROWS) { return -1; }
    return row * CHESS_COLS + col;
}

/* 求某列格心的横坐标 */
static int px(int index) { return CX0 + (index % CHESS_COLS) * CC; }
/* 求某行格心的纵坐标 */
static int py(int index) { return CY0 + (index / CHESS_COLS) * CC; }

/* 绘制棋盘：木色底、横竖网格、九宫斜线与河界文字 */
static void draw_grid(App *app)
{
    const Rect board = {10, 84, 340, 372};
    draw_rect(&app->screen, board, 0xE9D6B2);
    /* 横线 10 条 */
    for (int r = 0; r < CHESS_ROWS; ++r) {
        draw_line(&app->screen, CX0, CY0 + r * CC, CX0 + (CHESS_COLS - 1) * CC, CY0 + r * CC,
            1, 0x8A6A45);
    }
    /* 竖线：两侧通到底，中间 7 条在河界断开 */
    for (int c = 0; c < CHESS_COLS; ++c) {
        int x = CX0 + c * CC;
        if (c == 0 || c == CHESS_COLS - 1) {
            draw_line(&app->screen, x, CY0, x, CY0 + (CHESS_ROWS - 1) * CC, 1, 0x8A6A45);
        } else {
            draw_line(&app->screen, x, CY0, x, CY0 + 4 * CC, 1, 0x8A6A45);
            draw_line(&app->screen, x, CY0 + 5 * CC, x, CY0 + 9 * CC, 1, 0x8A6A45);
        }
    }
    /* 九宫斜线 */
    draw_line(&app->screen, CX0 + 3 * CC, CY0, CX0 + 5 * CC, CY0 + 2 * CC, 1, 0x8A6A45);
    draw_line(&app->screen, CX0 + 5 * CC, CY0, CX0 + 3 * CC, CY0 + 2 * CC, 1, 0x8A6A45);
    draw_line(&app->screen, CX0 + 3 * CC, CY0 + 7 * CC, CX0 + 5 * CC, CY0 + 9 * CC, 1, 0x8A6A45);
    draw_line(&app->screen, CX0 + 5 * CC, CY0 + 7 * CC, CX0 + 3 * CC, CY0 + 9 * CC, 1, 0x8A6A45);
    /* 河界 */
    draw_text(&app->screen, CX0 + 46, CY0 + 4 * CC + 8, 20, 0x9A7A50, "楚  河");
    draw_text(&app->screen, CX0 + 198, CY0 + 4 * CC + 8, 20, 0x9A7A50, "汉  界");
}

/* 在 index 处画一枚棋子，选中时先垫一个青色大圆底；正值为红方，负值为黑方 */
static void draw_piece(App *app, int index, bool selected)
{
    int piece = app->chess.cell[index];
    int cx = px(index), cy = py(index);
    /* 红黑两方用不同颜色的圆环：同一块木色底上单靠字色区分，远看几乎一样。 */
    if (selected) { draw_disc(&app->screen, cx, cy, PIECE + 3, UI_TEAL); }
    draw_disc(&app->screen, cx, cy, PIECE, piece > 0 ? 0xC0654F : 0x5A6B69);
    draw_disc(&app->screen, cx, cy, PIECE - 2, 0xF6E7C8);
    const char *label = (piece > 0 ? G_RED : G_BLACK)[chess_type(piece) - 1];
    int width = text_width(label, 21);
    draw_text(&app->screen, cx - width / 2, cy - 12, 21, piece > 0 ? 0xB03A2E : 0x243A3B, label);
}

/* 绘制中国象棋页面：棋盘、可走点提示、棋子、选子状态与操作说明 */
void chess_render(App *app)
{
    char line[64];
    const Chess *game = &app->chess;

    ui_header_sized(app, "中国象棋", 26);
    ui_button(app, (Rect){596, 16, 180, 60}, "重新开始", true);
    draw_grid(app);

    /* 选中棋子的可走点先画，棋子压在上面，避免圆点盖住字 */
    if (game->sel >= 0 && !game->result) {
        for (int i = 0; i < CHESS_CELLS; ++i) {
            if (!chess_target_ok(game, game->sel, i)) { continue; }
            if (game->cell[i]) {
                /* 可吃的目标加红圈 */
                draw_disc(&app->screen, px(i), py(i), PIECE + 5, 0xC4633F);
            } else {
                /* 可走的空格加绿点 */
                draw_disc(&app->screen, px(i), py(i), 5, UI_TEAL);
            }
        }
    }
    for (int i = 0; i < CHESS_CELLS; ++i) {
        if (game->cell[i]) { draw_piece(app, i, i == game->sel); }
    }

    /* 状态文字：红胜、黑胜或轮到哪一方 */
    const char *status;
    uint32_t color = UI_TEAL;
    if (game->result == 1) { status = "红方胜！"; }
    else if (game->result == -1) { status = "黑方胜！"; color = UI_INK; }
    else { status = game->side > 0 ? "红方走棋" : "黑方走棋"; color = game->side > 0 ? 0xB03A2E : UI_INK; }
    draw_text(&app->screen, 386, 96, 28, color, status);

    /* 已选棋子显示其名称，未选时给出提示 */
    if (game->sel >= 0) {
        int piece = game->cell[game->sel];
        const char *label = (piece > 0 ? G_RED : G_BLACK)[chess_type(piece) - 1];
        snprintf(line, sizeof(line), "已选：%s", label);
    } else {
        snprintf(line, sizeof(line), "未选中棋子");
    }
    draw_text(&app->screen, 386, 138, 18, UI_MUTED, line);

    draw_text(&app->screen, 386, 186, 16, UI_MUTED, "1. 点自己的棋子选中");
    draw_text(&app->screen, 386, 212, 16, UI_MUTED, "2. 点目标格落子");
    draw_text(&app->screen, 386, 238, 16, UI_MUTED, "3. 绿点可走，红圈可吃");

    draw_text(&app->screen, 386, 292, 16, UI_MUTED, "吃将／帅即胜；无子可动");
    draw_text(&app->screen, 386, 318, 16, UI_MUTED, "也算负。将帅不可照面。");

    if (game->result) {
        draw_text(&app->screen, 386, 372, 16, 0x9C8F7B, "点右上角重新开局");
    }
    if (app->missTap) {
        snprintf(line, sizeof(line), "不能这样走 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 560, 444, 15, 0xAD4E3D, line);
    }
}

/* 处理中国象棋的输入：按下时记录起点，只在抬起时判定。
   未选中时点自己的棋子即选中；已选中时再点同一枚取消、点自己另一枚改选、
   点其他格则尝试走子，走法不合法时取消选中并提示未命中。 */
void chess_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) { app->lastX = event.x; app->lastY = event.y; return; }
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;

    /* 重新开局 */
    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        chess_start(&app->chess);
        app->dirty = true;
        return;
    }
    Chess *game = &app->chess;
    /* 对局已结束，只请求重绘 */
    if (game->result) { app->dirty = true; return; }

    /* 先按抬起点换算格子，未命中再退回按下点 */
    int index = square_at(event.x, event.y);
    if (index < 0) { index = square_at(app->downX, app->downY); }
    /* 两次都未命中 */
    if (index < 0) { app->missTap = true; app->dirty = true; return; }

    int piece = game->cell[index];
    if (game->sel < 0) {
        /* 未选中：点自己的棋子才选中，其余视为未命中 */
        if (piece && chess_side(piece) == game->side) {
            game->sel = index;
        } else {
            app->missTap = true;
        }
    } else if (index == game->sel) {
        /* 再点同一枚则取消选中 */
        game->sel = -1;
    } else if (piece && chess_side(piece) == game->side) {
        game->sel = index; /* 改选自己的另一枚 */
    } else if (!chess_move(game, game->sel, index)) {
        /* 走法不合法：取消选中并提示 */
        game->sel = -1;
        app->missTap = true;
    }
    app->dirty = true;
}
