/* 中国象棋页面, 2026-11-20: two players on one screen.
 *
 * Nine files by ten ranks at 40px. A piece is selected first and the legal targets are
 * dotted, which matters more here than on the other boards: a cannon and a rook look
 * alike until you see where they may go, and a beginner cannot tell a blocked horse
 * from a free one. The rules themselves live in boardgames.c. */
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

/* 棋盘几何：9 路 × 10 行，每格 37px。格距从 40 收到 37 是为了让第 0 行棋子（半径 16）
   不再压住页眉那条分隔线，同时末行仍留出 30px 下边距。 */
#define CX0 30
#define CY0 100
#define CC 37
#define PIECE 16

static const char *const G_RED[7] = {"帅", "仕", "相", "马", "车", "炮", "兵"};
static const char *const G_BLACK[7] = {"将", "士", "象", "马", "车", "炮", "卒"};

static int square_at(int x, int y)
{
    if (x < CX0 - CC / 2 || y < CY0 - CC / 2) { return -1; }
    int col = (x - CX0 + CC / 2) / CC;
    int row = (y - CY0 + CC / 2) / CC;
    if (col < 0 || col >= CHESS_COLS || row < 0 || row >= CHESS_ROWS) { return -1; }
    return row * CHESS_COLS + col;
}

static int px(int index) { return CX0 + (index % CHESS_COLS) * CC; }
static int py(int index) { return CY0 + (index / CHESS_COLS) * CC; }

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
                draw_disc(&app->screen, px(i), py(i), PIECE + 5, 0xC4633F);
            } else {
                draw_disc(&app->screen, px(i), py(i), 5, UI_TEAL);
            }
        }
    }
    for (int i = 0; i < CHESS_CELLS; ++i) {
        if (game->cell[i]) { draw_piece(app, i, i == game->sel); }
    }

    const char *status;
    uint32_t color = UI_TEAL;
    if (game->result == 1) { status = "红方胜！"; }
    else if (game->result == -1) { status = "黑方胜！"; color = UI_INK; }
    else { status = game->side > 0 ? "红方走棋" : "黑方走棋"; color = game->side > 0 ? 0xB03A2E : UI_INK; }
    draw_text(&app->screen, 386, 96, 28, color, status);

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

void chess_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) { app->lastX = event.x; app->lastY = event.y; return; }
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;

    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        chess_start(&app->chess);
        app->dirty = true;
        return;
    }
    Chess *game = &app->chess;
    if (game->result) { app->dirty = true; return; }

    int index = square_at(event.x, event.y);
    if (index < 0) { index = square_at(app->downX, app->downY); }
    if (index < 0) { app->missTap = true; app->dirty = true; return; }

    int piece = game->cell[index];
    if (game->sel < 0) {
        if (piece && chess_side(piece) == game->side) {
            game->sel = index;
        } else {
            app->missTap = true;
        }
    } else if (index == game->sel) {
        game->sel = -1;
    } else if (piece && chess_side(piece) == game->side) {
        game->sel = index; /* 改选自己的另一枚 */
    } else if (!chess_move(game, game->sel, index)) {
        game->sel = -1;
        app->missTap = true;
    }
    app->dirty = true;
}
