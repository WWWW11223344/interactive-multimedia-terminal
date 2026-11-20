/* 井字棋页面, 2026-11-20.
 *
 * Player is X and moves first; the computer replies with a full search, so the best a
 * player can do is draw. The board is 336px wide - three 112px cells - because a 3×3
 * board at normal size is all gap on this panel, and a 112px cell is bigger than the
 * gslx680's vertical error, so a tap cannot land in the neighbouring row.
 *
 * Geometry is stated once: BX/BY is the top-left corner of the board, BC the cell size,
 * so a cell centre is BX + col * BC + BC / 2. The separators sit on the cell boundaries,
 * which is what keeps the drawn grid and the hit test on the same coordinates. */
#include "terminal.h"
#include <stdio.h>

#define BX 44      /* 棋盘左边界 */
#define BY 92      /* 棋盘上边界 */
#define BC 112     /* 每格边长 */
#define MARK 34    /* 盘上 X／O 的半径 */
#define LEG 16     /* 右侧图例里 X／O 的半径 */

static int cell_cx(int col) { return BX + col * BC + BC / 2; }
static int cell_cy(int row) { return BY + row * BC + BC / 2; }

/* Snapping, not strict containment: a tap 60px outside the board still reads as the
 * nearest edge cell, because refusing it would only look like a dead panel. */
static int square_at(int x, int y)
{
    int col = (x - BX) / BC;
    int row = (y - BY) / BC;
    if (x < BX - BC / 2 || y < BY - BC / 2) { return -1; }
    if (col < 0 || col > 2 || row < 0 || row > 2) { return -1; }
    return row * 3 + col;
}

static void draw_cross(App *app, int cx, int cy, int half, uint32_t color)
{
    int weight = half > 20 ? 5 : 3;
    draw_line(&app->screen, cx - half, cy - half, cx + half, cy + half, weight, color);
    draw_line(&app->screen, cx + half, cy - half, cx - half, cy + half, weight, color);
}

static void draw_circle(App *app, int cx, int cy, int radius, uint32_t color)
{
    draw_disc(&app->screen, cx, cy, radius, color);
    draw_disc(&app->screen, cx, cy, radius - (radius > 20 ? 8 : 5), 0xFBF7EC);
}

void toe_render(App *app)
{
    char line[64];
    const Toe *game = &app->toe;

    ui_header_sized(app, "井字棋", 26);
    ui_button(app, (Rect){596, 16, 180, 60}, "重新开始", true);

    draw_rect(&app->screen, (Rect){BX, BY, BC * 3, BC * 3}, 0xFBF7EC);
    for (int i = 1; i < 3; ++i) {           /* 分隔线落在格子边界上 */
        draw_rect(&app->screen, (Rect){BX + i * BC - 2, BY, 4, BC * 3}, 0xD0C8B6);
        draw_rect(&app->screen, (Rect){BX, BY + i * BC - 2, BC * 3, 4}, 0xD0C8B6);
    }

    for (int i = 0; i < TOE_CELLS; ++i) {
        int cx = cell_cx(i % 3), cy = cell_cy(i / 3);
        if (game->cell[i] == 1) { draw_cross(app, cx, cy, MARK, 0x487BBC); }
        else if (game->cell[i] == 2) { draw_circle(app, cx, cy, MARK, 0xBA6256); }
    }

    int a = 0, b = 0, c = 0;
    if (toe_win_line(game, &a, &b, &c)) {
        draw_line(&app->screen, cell_cx(a % 3), cell_cy(a / 3),
            cell_cx(c % 3), cell_cy(c / 3), 5, 0xE0B24A);
    }

    const char *status;
    uint32_t color = UI_TEAL;
    if (game->result == 1) { status = "你赢了！"; }
    else if (game->result == 2) { status = "电脑赢了"; color = 0xAD4E3D; }
    else if (game->result == 3) { status = "平局"; color = UI_MUTED; }
    else { status = game->turn == 1 ? "轮到你（X）" : "电脑思考中"; }

    draw_text(&app->screen, 412, 96, 28, color, status);
    int moves = 0;
    for (int i = 0; i < TOE_CELLS; ++i) { if (game->cell[i]) { ++moves; } }
    snprintf(line, sizeof(line), "第 %d 手", moves);
    draw_text(&app->screen, 412, 138, 18, UI_MUTED, line);

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

void toe_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) { app->lastX = event.x; app->lastY = event.y; return; }
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;

    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        toe_start(&app->toe);
        app->dirty = true;
        return;
    }
    Toe *game = &app->toe;
    if (game->result || game->turn != 1) { app->dirty = true; return; }

    int index = square_at(event.x, event.y);
    if (index < 0) { index = square_at(app->downX, app->downY); }
    if (index < 0 || !toe_place(game, index)) {
        app->missTap = true;
        app->dirty = true;
        return;
    }
    if (!game->result) {
        int reply = -1;
        toe_ai(game, &reply);
    }
    app->dirty = true;
}
