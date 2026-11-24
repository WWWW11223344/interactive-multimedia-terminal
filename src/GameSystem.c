/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   GameSystem.c
Author:      汪贤云        Version: 1.0.2       Date: 2026-11-19
Description: 2048 的界面页。棋盘 4x4，支持在棋盘上滑动，也支持点击右侧四张大方向卡。
             规则层在 game.c，本文件只负责绘制与输入。
Others:      方向键做成四张大卡而不是十字形细键，是因为 gslx680 的 X 准确而 Y 会漂移
             几十像素：64 像素高的细键比误差还小，168 像素高的卡片则足够大，中心落在
             84 像素内都能命中，一次「上」不会被误判成「下」。卡片按轴分组——左右在
             右列、上下在左列——靠字而不靠位置辨认；最近一次有效方向会高亮，即使棋盘
             合法地不能往那个方向走，玩家也能看到面板确实收到了这次点击。
Function List:
  1. game_render    绘制棋盘、数字牌、方向卡、得分与状态
  2. game_event     处理滑动与方向键点击
  3. dir_at         屏幕坐标换算成方向下标，带就近吸附
  4. draw_dir_card  绘制一张方向卡
  5. pad_hit        方向命中：先抬起点再按下点
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 由肖林祥工程改写：大方向卡替代十字键
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 滑动改为记住拖动最远点；注释改为中文规范格式
******************************************************************************/
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

/* 四张方向卡的区域，下标与 Direction 枚举一致，互不重叠 */
static const Rect g_dir[4] = {
    {596, 86, 184, 168},   /* GAME_LEFT  右列上方 */
    {596, 258, 184, 168},  /* GAME_RIGHT 右列下方 */
    {404, 86, 184, 168},   /* GAME_UP    左列上方 */
    {404, 258, 184, 168}   /* GAME_DOWN  左列下方 */
};
/* 方向卡的文字 */
static const char *const g_dirName[4] = {"左", "右", "上", "下"};
/* 棋盘区域 */
static const Rect g_board = {24, 88, 354, 354};

/* 把屏幕坐标换算成方向下标。
   说明：先做精确包含，未命中再在 60 像素内取最近卡片的中心，用来照顾卡片之间
   那条窄缝以及刚好落在边界外的抬起点。 */
static int dir_at(int x, int y)
{
    for (int i = 0; i < 4; ++i) {
        if (rect_has(g_dir[i], x, y)) { return i; }
    }
    int best = -1;
    int64_t bestDistance = 60LL * 60LL;
    for (int i = 0; i < 4; ++i) {
        int64_t dx = x - (g_dir[i].x + g_dir[i].w / 2);
        int64_t dy = y - (g_dir[i].y + g_dir[i].h / 2);
        int64_t distance = dx * dx + dy * dy;
        if (distance < bestDistance) { bestDistance = distance; best = i; }
    }
    return best;
}

/* 绘制一张方向卡：细边框、大字，最近一次用的方向加青色底高亮。 */
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

/* 绘制 2048 页面。得分与标题共用页头那一行，得分的横坐标按标题的实测宽度算出，
   而不是猜一个列位置，因此两者永远不会重叠。最后方向靠高亮方向卡来回显，
   比再加一行文字更容易一眼看清。 */
void game_render(App *app)
{
    char line[64];

    ui_header_sized(app, "2048 小游戏", 26);
    ui_button(app, (Rect){596, 16, 180, 60}, app->game.started ? "重新开始" : "开始游戏", true);

    snprintf(line, sizeof(line), "得分 %u", (unsigned int)app->game.score);
    draw_text(&app->screen, 168 + text_width("2048 小游戏", 26) + 26, 27, 26, UI_INK, line);

    /* 棋盘底与 16 张数字牌。指数超过 11 的按 11 显示，防止越界访问 tiles */
    draw_rect(&app->screen, g_board, 0xC9C4B7);
    for (int i = 0; i < 16; ++i) {
        unsigned int value = app->game.cells[i];
        if (value > 11) { value = 11; }
        draw_image(&app->screen, (Rect){32 + i % 4 * 86, 96 + i / 4 * 86, 80, 80},
            &app->tiles[value], false);
    }
    for (int i = 0; i < 4; ++i) { draw_dir_card(app, i); }

    /* 状态文字按未开始、成功、失败、正常四种情况给出 */
    const char *status = !app->game.started ? "点击开始，生成两个数字" :
        app->game.won ? "达到 2048，挑战成功！" : app->game.over ? "没有可移动格，重新挑战吧" :
        "滑动棋盘，或点右侧的大方向键";
    draw_text(&app->screen, 24, 444, 16, UI_MUTED, status);

    /* 按下与抬起的坐标，用于观察手势是否到位 */
    snprintf(line, sizeof(line), "down %d,%d   up %d,%d",
        app->downX, app->downY, app->tapX, app->tapY);
    draw_text(&app->screen, 24, 464, 12, 0x9C8F7B, line);
    if (app->missTap) {
        snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
        draw_text(&app->screen, 600, 444, 15, 0xAD4E3D, line);
    }
}

/* 方向命中：先按抬起点找，找不到再退回按下点。
   这块面板抬起时可能偏离按下点几十像素，两个都试才稳。 */
static int pad_hit(const App *app, Event event)
{
    int zone = dir_at(event.x, event.y);
    if (zone < 0) { zone = dir_at(app->downX, app->downY); }
    return zone;
}

/* 处理 2048 的输入：滑动优先，方向键其次。
   说明：肖林祥的 ts_func_slide() 在抬起那一刻根据按下点与最后上报点一次性判定
   方向，|dx| > |dy| 为横向否则纵向，这里沿用同一规则。唯一改动是记住拖动过程中
   离起点最远的点，而不是最后一个点——长距离拖动后面板最后一次采样常常又回到
   起点附近，会把一次真实滑动误判成点击。 */
void game_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN) {
        app->lastX = event.x;
        app->lastY = event.y;
        return;
    }
    if (event.type == EVENT_MOVE) {
        /* 记录离起点最远的采样点，供抬起时判定方向 */
        int best = abs(app->lastX - app->downX) + abs(app->lastY - app->downY);
        int now = abs(event.x - app->downX) + abs(event.y - app->downY);
        if (now > best) {
            app->lastX = event.x;
            app->lastY = event.y;
        }
        return;
    }
    if (event.type != EVENT_UP) { return; }

    app->missTap = false;

    /* 开始或重新开始按钮 */
    if (ui_clicked(app, event, (Rect){596, 16, 180, 60})) {
        game_start(&app->game, (uint32_t)time(NULL));
        app->lastDir = -1;
        app->dirty = true;
        return;
    }

    int dx = app->lastX - app->downX;
    int dy = app->lastY - app->downY;
    bool onBoard = rect_has(g_board, app->downX, app->downY);
    Direction direction = GAME_LEFT;
    bool moved = false;

    if (onBoard && (abs(dx) > 22 || abs(dy) > 22)) {
        /* 棋盘上的滑动：位移超过 22 像素才算手势，按主轴决定方向 */
        direction = abs(dx) > abs(dy) ? (dx > 0 ? GAME_RIGHT : GAME_LEFT) :
            (dy > 0 ? GAME_DOWN : GAME_UP);
        moved = true;
    } else {
        /* 没有滑动手势时，才判定方向卡 */
        int zone = pad_hit(app, event);
        if (zone >= 0) {
            direction = (Direction)zone;
            moved = true;
        }
    }

    if (moved) {
        /* 还没开始则自动开局，方便直接滑动试玩 */
        if (!app->game.started) { game_start(&app->game, (uint32_t)time(NULL)); }
        app->lastDir = (int)direction;
        game_move(&app->game, direction);
    } else {
        app->missTap = true;
    }
    app->dirty = true;
}
