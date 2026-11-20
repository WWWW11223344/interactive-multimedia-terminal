/* Xie Donghong desktop simplified by Codex, 2026-11-19: three entries and main-loop clock.
 * 2026-11-20: the second card opens the game list instead of 2048 directly, because
 * four games no longer fit as home cards and the list can grow without touching this
 * layout again. */
#include "terminal.h"
#include <stdio.h>

/* Repaint a compact home screen; the hardware clock is displayed as configured by Linux. */
void home_render(App *app)
{
    Image *screen = &app->screen;
    draw_rect(screen, (Rect){0,0,800,480}, UI_BG);
    draw_text(screen, 32, 24, 34, UI_INK, "触控休闲娱乐终端");
    ui_button(app, (Rect){648,16,132,60}, "退出", false);
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char clockText[64] = "--:--:--", dateText[64] = "";
    if (local) {
        strftime(clockText, sizeof(clockText), "%H:%M:%S", local);
        strftime(dateText, sizeof(dateText), "%Y / %m / %d", local);
    }
    draw_text(screen, 32, 82, 42, UI_TEAL, clockText);
    draw_text(screen, 240, 96, 21, UI_MUTED, dateText);
    const char *names[] = {"电子相册", "小游戏", "创意画板"};
    const char *notes[] = {"浏览照片与画板作品", "四款小游戏任你挑", "随手画下你的灵感"};
    const uint32_t accents[] = {0x227C77,0xC58636,0xBA6256};
    for (int i = 0; i < 3; ++i) {
        int x = 32 + i * 253;
        draw_rect(screen, (Rect){x,158,230,236}, 0xFFFFFF);
        draw_rect(screen, (Rect){x,158,230,6}, accents[i]);
        if (i == 0) {
            draw_rect(screen, (Rect){x+25,188,112,69}, 0xD9EDE4);
            draw_disc(screen, x+109,207,10, accents[i]);
            draw_line(screen, x+35,247,x+66,219,3,accents[i]);
            draw_line(screen, x+66,219,x+99,247,3,accents[i]);
        } else if (i == 1) {
            /* One mini board with a stone and a cross: reads as "more than one game"
               without needing a new bitmap or a fifth icon slot. */
            draw_rect(screen, (Rect){x+25,184,116,70}, 0xF3EDDF);
            draw_line(screen, x+83,190,x+83,248,2,0x9A7A50);
            draw_line(screen, x+31,219,x+135,219,2,0x9A7A50);
            draw_disc(screen, x+57,202,9, 0x243A3B);
            draw_disc(screen, x+57,202,4, 0x4C6567);
            draw_line(screen, x+100,228,x+118,246,4,accents[i]);
            draw_line(screen, x+118,228,x+100,246,4,accents[i]);
        } else {
            draw_line(screen,x+34,247,x+104,196,6,accents[i]);
            draw_disc(screen,x+130,240,12,0xE5B149);
        }
        draw_text(screen,x+24,281,31,UI_INK,names[i]);
        draw_text(screen,x+24,327,20,UI_MUTED,notes[i]);
        draw_text(screen,x+24,362,18,accents[i],"点击进入  >");
    }
    draw_rect(screen, (Rect){32,428,736,1}, 0xD8DED7);
    draw_text(screen,32,444,18,UI_MUTED,"GEC6818  /  800 × 480");
    draw_text(screen,566,444,18,UI_MUTED,"照片 · 游戏 · 绘画");
}
