/* Xiao Linxiang 2048 UI adapted by Codex, 2026-11-19: original tile assets, gesture and button input. */
#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

/* Reuse the upstream numeric BMPs; keep all controls outside the 4x4 swipe area. */
void game_render(App *app)
{
    ui_header(app,"2048 小游戏");
    ui_button(app,(Rect){600,18,176,44},app->game.started ? "重新开始" : "开始游戏",true);
    draw_rect(&app->screen,(Rect){24,91,354,354},0xC9C4B7);
    for (int i = 0; i < 16; ++i) {
        unsigned int value = app->game.cells[i];
        if (value > 11) { value = 11; }
        draw_image(&app->screen,(Rect){32+i%4*86,99+i/4*86,80,80},&app->tiles[value],false);
    }
    char scoreText[64];
    snprintf(scoreText,sizeof(scoreText),"得分  %u",(unsigned int)app->game.score);
    draw_text(&app->screen,430,102,32,UI_INK,scoreText);
    const char *status = !app->game.started ? "点击开始，生成两个数字" :
        app->game.won ? "达到 2048，挑战成功！" : app->game.over ? "没有可移动格，重新挑战吧" : "滑动棋盘或点击方向按钮";
    draw_text(&app->screen,430,153,23,UI_MUTED,status);
    ui_button(app,(Rect){558,211,80,52},"上",false);
    ui_button(app,(Rect){466,276,80,52},"左",false);
    ui_button(app,(Rect){558,276,80,52},"下",false);
    ui_button(app,(Rect){650,276,80,52},"右",false);
    draw_text(&app->screen,430,366,22,UI_TEAL,"相同数字合并，每步只合并一次");
    draw_text(&app->screen,430,403,20,UI_MUTED,"目标：把数字合成 2048");
    draw_text(&app->screen,24,457,17,UI_MUTED,"返回主界面会保留当前棋盘；重新开始会清除得分。");
}

/* A gesture belongs to the board only if its press started there; buttons need both endpoints. */
void game_event(App *app, Event event)
{
    if (event.type != EVENT_UP) { return; }
    if (ui_clicked(app,event,(Rect){600,18,176,44})) {
        game_start(&app->game,(uint32_t)time(NULL));
    } else {
        Direction direction = GAME_LEFT;
        bool moved = false;
        const Rect buttons[] = {{466,276,80,52},{650,276,80,52},{558,211,80,52},{558,276,80,52}};
        for (int i = 0; i < 4; ++i) {
            if (ui_clicked(app,event,buttons[i])) { direction = (Direction)i; moved = true; }
        }
        int dx = event.x-app->downX, dy = event.y-app->downY;
        if (rect_has((Rect){24,91,354,354},app->downX,app->downY) && (abs(dx) > 32 || abs(dy) > 32)) {
            direction = abs(dx) > abs(dy) ? (dx > 0 ? GAME_RIGHT : GAME_LEFT) :
                (dy > 0 ? GAME_DOWN : GAME_UP);
            moved = true;
        }
        if (moved) { game_move(&app->game,direction); }
    }
    app->dirty = true;
}
