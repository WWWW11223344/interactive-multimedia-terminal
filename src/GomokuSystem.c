/* 五子棋页面, 2026-11-20.
 *
 * The board is 15×15 points 25px apart, which makes one point about a finger wide:
 * a tap is resolved to the nearest intersection with no further tolerance, because
 * the whole board is one target and refusing a tap near the edge helps nobody. Player
 * is black and moves first; the computer answers immediately, before the next frame,
 * so there is no "thinking" state to get stuck in. */
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

#define GX 30
#define GY 96
#define GC 25
#define STONE 10

static int point_at(int x, int y)
{
    if (x < GX - GC / 2 || y < GY - GC / 2) { return -1; }
    int col = (x - GX + GC / 2) / GC;
    int row = (y - GY + GC / 2) / GC;
    if (col < 0 || col >= GOMOKU_N || row < 0 || row >= GOMOKU_N) { return -1; }
    return row * GOMOKU_N + col;
}

static int stone_count(const Gomoku *game)
{
    int count = 0;
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (game->cell[i]) { ++count; }
    }
    return count;
}

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

void gomoku_render(App *app)
{
    char line[64];
    const Gomoku *game = &app->gomoku;

    ui_header_sized(app, "五子棋", 26);
    ui_button(app, (Rect){596, 16, 180, 60}, "重新开始", true);

    draw_rect(&app->screen, (Rect){18, 84, 376, 376}, 0xE7D3AC);
    for (int i = 0; i < GOMOKU_N; ++i) {
        int p = GX + i * GC;
        draw_line(&app->screen, GX, GY + i * GC, GX + (GOMOKU_N - 1) * GC, GY + i * GC, 1, 0x9A7A50);
        draw_line(&app->screen, p, GY, p, GY + (GOMOKU_N - 1) * GC, 1, 0x9A7A50);
    }
    static const int star[5][2] = {{3, 3}, {11, 3}, {7, 7}, {3, 11}, {11, 11}};
    for (int i = 0; i < 5; ++i) {
        draw_disc(&app->screen, GX + star[i][0] * GC, GY + star[i][1] * GC, 3, 0x6E5334);
    }

    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (!game->cell[i]) { continue; }
        draw_stone(app, GX + (i % GOMOKU_N) * GC, GY + (i / GOMOKU_N) * GC, game->cell[i]);
    }
    if (game->last >= 0) {
        int x = GX + (game->last % GOMOKU_N) * GC;
        int y = GY + (game->last / GOMOKU_N) * GC;
        draw_disc(&app->screen, x, y, 3, game->cell[game->last] == 1 ? 0xE9C46A : 0x227C77);
    }

    const char *status;
    uint32_t color = UI_TEAL;
    if (game->winner == 1) { status = "你赢了！"; }
    else if (game->winner == 2) { status = "电脑赢了"; color = 0xAD4E3D; }
    else if (game->winner == 3) { status = "平局"; color = UI_MUTED; }
    else { status = game->turn == 1 ? "轮到你（黑）" : "电脑落子中"; }

    draw_text(&app->screen, 412, 96, 28, color, status);
    snprintf(line, sizeof(line), "第 %d 手", stone_count(game));
    draw_text(&app->screen, 412, 138, 18, UI_MUTED, line);

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

void gomoku_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) { app->lastX = event.x; app->lastY = event.y; return; }
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;

    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        gomoku_start(&app->gomoku, (uint32_t)time(NULL));
        app->dirty = true;
        return;
    }
    Gomoku *game = &app->gomoku;
    if (game->winner || game->turn != 1) { app->dirty = true; return; }

    int index = point_at(event.x, event.y);
    if (index < 0) { index = point_at(app->downX, app->downY); }
    if (index < 0 || !gomoku_place(game, index)) {
        app->missTap = true;
        app->dirty = true;
        return;
    }
    if (!game->winner) {
        int reply = -1;
        gomoku_ai(game, &reply);
    }
    app->dirty = true;
}
