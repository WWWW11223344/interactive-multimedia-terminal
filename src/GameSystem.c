/* Xiao Linxiang 2048 UI adapted by Codex, 2026-11-19: original tile assets, gesture and button input. */
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

/* Four large cards instead of a cross of thin keys.
 *
 * The gslx680 reports X accurately but its Y drifts by tens of pixels, and that is
 * exactly why 左/右 felt fine while 上/下 did not: a 64px-tall key is smaller than the
 * error, a 168px-tall card is not. Any tap whose centre is within 84px vertically
 * still lands inside the card it was aimed at, so the drift can no longer promote a
 * 上 tap into a 下 tap. Cards are grouped by axis - vertical moves on the left,
 * horizontal moves on the right - so the character, not the position, identifies the
 * key, and the last accepted direction is tinted so the user can see the panel did
 * receive the tap even when the board legitimately cannot move that way. */
static const Rect g_dir[4] = {   /* index == Direction, no two rects overlap */
    {596, 86,184,168},           /* GAME_LEFT  right column, top    */
    {596,258,184,168},           /* GAME_RIGHT right column, bottom */
    {404, 86,184,168},           /* GAME_UP    left column, top     */
    {404,258,184,168}            /* GAME_DOWN  left column, bottom  */
};
static const char *const g_dirName[4] = {"左", "右", "上", "下"};
static const Rect g_board = {24,88,354,354};

/* Exact rectangle first; otherwise the nearest card centre within 60px, for the
   sliver between cards and for a release point that falls just outside. */
static int dir_at(int x, int y)
{
    for (int i = 0; i < 4; ++i) {
        if (rect_has(g_dir[i], x, y)) { return i; }
    }
    int best = -1;
    int64_t bestDistance = 60LL * 60LL;
    for (int i = 0; i < 4; ++i) {
        int64_t dx = x - (g_dir[i].x + g_dir[i].w/2);
        int64_t dy = y - (g_dir[i].y + g_dir[i].h/2);
        int64_t distance = dx * dx + dy * dy;
        if (distance < bestDistance) { bestDistance = distance; best = i; }
    }
    return best;
}

/* One card: hairline frame, big character, and a tint when it was the last move. */
static void draw_dir_card(App *app, int index)
{
    Rect rect = g_dir[index];
    bool used = app->lastDir == index;
    draw_rect(&app->screen, rect, used ? 0xE4EFEC : 0xFFFFFF);
    uint32_t border = used ? UI_TEAL : 0xDCD6C9;
    draw_rect(&app->screen, (Rect){rect.x, rect.y, rect.w, 2}, border);
    draw_rect(&app->screen, (Rect){rect.x, rect.y + rect.h - 2, rect.w, 2}, border);
    draw_rect(&app->screen, (Rect){rect.x, rect.y, 2, rect.h}, border);
    draw_rect(&app->screen, (Rect){rect.x + rect.w - 2, rect.y, 2, rect.h}, border);
    int width = text_width(g_dirName[index], 56);
    draw_text(&app->screen, rect.x + (rect.w - width) / 2, rect.y + (rect.h - 56) / 2,
        56, used ? UI_TEAL : UI_INK, g_dirName[index]);
}

void game_render(App *app)
{
    char line[64];
    ui_header_sized(app, "2048 小游戏", 26);
    ui_button(app, (Rect){596,16,180,60}, app->game.started ? "重新开始" : "开始游戏", true);

    /* Score shares the header strip with the title; the last direction is echoed by
       tinting its card, which is easier to read at a glance than another line of text.
       The score is placed from the measured title, not from a guessed column, so the
       two can never run into each other. */
    snprintf(line, sizeof(line), "得分 %u", (unsigned int)app->game.score);
    draw_text(&app->screen, 168 + text_width("2048 小游戏", 26) + 26, 27, 26, UI_INK, line);

    draw_rect(&app->screen, g_board, 0xC9C4B7);
    for (int i = 0; i < 16; ++i) {
        unsigned int value = app->game.cells[i];
        if (value > 11) { value = 11; }
        draw_image(&app->screen, (Rect){32 + i % 4 * 86, 96 + i / 4 * 86, 80, 80},
            &app->tiles[value], false);
    }
    for (int i = 0; i < 4; ++i) { draw_dir_card(app, i); }

    const char *status = !app->game.started ? "点击开始，生成两个数字" :
        app->game.won ? "达到 2048，挑战成功！" : app->game.over ? "没有可移动格，重新挑战吧" :
        "滑动棋盘，或点右侧的大方向键";
    draw_text(&app->screen, 24, 444, 16, UI_MUTED, status);

    snprintf(line, sizeof(line), "down %d,%d   up %d,%d", app->downX, app->downY, app->tapX, app->tapY);
    draw_text(&app->screen, 24, 464, 12, 0x9C8F7B, line);
    if (app->missTap) {
        snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 600, 444, 15, 0xAD4E3D, line);
    }
}

/* The release point first, then the press point: the panel can lift a few dozen
   pixels away from where it went down. */
static int pad_hit(const App *app, Event event)
{
    int zone = dir_at(event.x, event.y);
    if (zone < 0) { zone = dir_at(app->downX, app->downY); }
    return zone;
}

/* Swipe first, keys second.
 *
 * Xiao Linxiang's ts_func_slide() decides the gesture once, on the lift event, from
 * the press point and the last reported point: |dx| > |dy| means horizontal, else
 * vertical. That is the same rule used here. The one addition is that the furthest
 * point of the drag is remembered instead of the last one, because the panel's final
 * sample can sit almost on the press point after a long drag, which made a real
 * swipe look like a tap. */
void game_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) {
        app->lastX = event.x; app->lastY = event.y;
        return;
    }
    if (event.type == EVENT_MOVE) {
        int best = abs(app->lastX-app->downX) + abs(app->lastY-app->downY);
        int now = abs(event.x-app->downX) + abs(event.y-app->downY);
        if (now > best) { app->lastX = event.x; app->lastY = event.y; }
        return;
    }
    if (event.type != EVENT_UP) { return; }

    app->missTap = false;
    if (ui_clicked(app, event, (Rect){596,16,180,60})) {
        game_start(&app->game, (uint32_t)time(NULL));
        app->lastDir = -1;
        app->dirty = true;
        return;
    }

    int dx = app->lastX - app->downX, dy = app->lastY - app->downY;
    bool onBoard = rect_has(g_board, app->downX, app->downY);
    Direction direction = GAME_LEFT;
    bool moved = false;

    if (onBoard && (abs(dx) > 22 || abs(dy) > 22)) {
        direction = abs(dx) > abs(dy) ? (dx > 0 ? GAME_RIGHT : GAME_LEFT) :
            (dy > 0 ? GAME_DOWN : GAME_UP);
        moved = true;
    } else {
        int zone = pad_hit(app, event);
        if (zone >= 0) { direction = (Direction)zone; moved = true; }
    }

    if (moved) {
        if (!app->game.started) { game_start(&app->game, (uint32_t)time(NULL)); }
        app->lastDir = (int)direction;
        game_move(&app->game, direction);
    } else {
        app->missTap = true;
    }
    app->dirty = true;
}
