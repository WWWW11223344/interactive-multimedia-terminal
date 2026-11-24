/* 无界面检查：2048 的方向卡片和滑动确实能移动棋盘，
 * 包括在真实面板上 gslx680 所表现出的纵向漂移的情况。
 * Codex, 2026-11-19. */
#include "terminal.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *what)
{
    if (condition) { fprintf(stderr, "ok   %s\n", what); }
    else { fprintf(stderr, "FAIL %s\n", what); ++failures; }
}

static void tap(App *app, int x, int y)
{
    app_event(app, (Event){EVENT_DOWN, x, y});
    app_event(app, (Event){EVENT_UP, x, y});
}

/* 在一个已知格子上放一个方块，游戏已在进行中，因此不会因新开局而额外生成方块。 */
static void seed_at(App *app, int index)
{
    memset(&app->game, 0, sizeof(app->game));
    app->game.started = true;
    app->game.seed = 12345u;
    app->game.cells[index] = 2;
}

/* 卡片中心，取自 GameSystem.c 中的 g_dir：上(496,170) 下(496,342) 左(688,170) 右(688,342) */
#define UP_X 496
#define UP_Y 170
#define DOWN_X 496
#define DOWN_Y 342
#define LEFT_X 688
#define LEFT_Y 170
#define RIGHT_X 688
#define RIGHT_Y 342

int main(void)
{
    App app;
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) { fprintf(stderr, "app_open failed\n"); return 2; }
    app.page = PAGE_2048;
    app.dirty = true;
    app_render(&app);

    seed_at(&app, 3);
    tap(&app, LEFT_X, LEFT_Y);
    expect(app.game.cells[0] != 0 && app.game.seed != 12345u, "left card moves the tile to the left edge");

    seed_at(&app, 0);
    tap(&app, RIGHT_X, RIGHT_Y);
    expect(app.game.cells[3] != 0, "right card moves the tile to the right edge");

    seed_at(&app, 12);
    tap(&app, UP_X, UP_Y);
    expect(app.game.cells[0] != 0, "up card moves the tile to the top edge");

    seed_at(&app, 0);
    tap(&app, DOWN_X, DOWN_Y);
    expect(app.game.cells[12] != 0, "down card moves the tile to the bottom edge");

    /* 面板上报的位置最多可偏离视觉位置约 70px；一张卡片高 168px，
       所以同样的点击向下偏移卡片高度的 1/2.4 时仍必须选中该卡片。 */
    seed_at(&app, 12);
    tap(&app, UP_X, UP_Y + 70);
    expect(app.game.cells[0] != 0, "up card still fires with the point 70px low");

    seed_at(&app, 0);
    tap(&app, DOWN_X, DOWN_Y - 70);
    expect(app.game.cells[12] != 0, "down card still fires with the point 70px high");

    seed_at(&app, 12);
    tap(&app, UP_X + 60, UP_Y + 60);
    expect(app.game.cells[0] != 0, "a diagonal offset inside the card still means up");

    /* 在卡片内按下，在棋盘外抬起：以按下点为准。 */
    seed_at(&app, 0);
    app_event(&app, (Event){EVENT_DOWN, 700, 300});
    app_event(&app, (Event){EVENT_UP, 790, 470});
    expect(app.game.cells[3] != 0, "a press on a card still counts when the lift lands elsewhere");

    /* 尚未开始：第一次按下会启动游戏，而不是什么都不做。 */
    memset(&app.game, 0, sizeof(app.game));
    tap(&app, LEFT_X, LEFT_Y);
    expect(app.game.started, "pressing a card starts a fresh game");

    /* 在棋盘上滑动，抬起时的采样点回弹到按下点。 */
    seed_at(&app, 3);
    app_event(&app, (Event){EVENT_DOWN, 300, 300});
    app_event(&app, (Event){EVENT_MOVE, 40, 302});
    app_event(&app, (Event){EVENT_UP, 300, 300});
    expect(app.game.cells[0] != 0, "a swipe counts even when the release sample snaps back");

    seed_at(&app, 12);
    app_event(&app, (Event){EVENT_DOWN, 200, 400});
    app_event(&app, (Event){EVENT_MOVE, 202, 120});
    app_event(&app, (Event){EVENT_UP, 202, 120});
    expect(app.game.cells[0] != 0, "an upward swipe on the board moves tiles up");

    seed_at(&app, 0);
    app_event(&app, (Event){EVENT_DOWN, 200, 120});
    app_event(&app, (Event){EVENT_MOVE, 198, 400});
    app_event(&app, (Event){EVENT_UP, 198, 400});
    expect(app.game.cells[12] != 0, "a downward swipe on the board moves tiles down");

    /* 在棋盘上短按且不移动，不算滑动。 */
    seed_at(&app, 0);
    tap(&app, 200, 300);
    expect(app.missTap && app.game.cells[0] != 0, "a still tap on the board just reports a miss");

    app_close(&app);
    fprintf(stderr, failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
