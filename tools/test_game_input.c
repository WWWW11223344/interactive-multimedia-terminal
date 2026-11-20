/* Headless check that the 2048 direction cards and swipes actually move the board,
 * including the vertical drift the gslx680 shows on the real panel.
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

/* One tile on a known cell, game already running so no spawn from a fresh start. */
static void seed_at(App *app, int index)
{
    memset(&app->game, 0, sizeof(app->game));
    app->game.started = true;
    app->game.seed = 12345u;
    app->game.cells[index] = 2;
}

/* Card centres, from g_dir in GameSystem.c: 上(496,170) 下(496,342) 左(688,170) 右(688,342) */
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

    /* The panel reports up to ~70px away from the visual position; a card is 168px
       tall, so the same tap one card-height/2.4 lower must still pick that card. */
    seed_at(&app, 12);
    tap(&app, UP_X, UP_Y + 70);
    expect(app.game.cells[0] != 0, "up card still fires with the point 70px low");

    seed_at(&app, 0);
    tap(&app, DOWN_X, DOWN_Y - 70);
    expect(app.game.cells[12] != 0, "down card still fires with the point 70px high");

    seed_at(&app, 12);
    tap(&app, UP_X + 60, UP_Y + 60);
    expect(app.game.cells[0] != 0, "a diagonal offset inside the card still means up");

    /* Press inside a card, lift outside the grid: the press point decides. */
    seed_at(&app, 0);
    app_event(&app, (Event){EVENT_DOWN, 700, 300});
    app_event(&app, (Event){EVENT_UP, 790, 470});
    expect(app.game.cells[3] != 0, "a press on a card still counts when the lift lands elsewhere");

    /* Not started yet: the first press starts the game instead of doing nothing. */
    memset(&app.game, 0, sizeof(app.game));
    tap(&app, LEFT_X, LEFT_Y);
    expect(app.game.started, "pressing a card starts a fresh game");

    /* Swipe on the board, release sample snapping back to the press point. */
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

    /* A short press on the board with no movement is not a swipe. */
    seed_at(&app, 0);
    tap(&app, 200, 300);
    expect(app.missTap && app.game.cells[0] != 0, "a still tap on the board just reports a miss");

    app_close(&app);
    fprintf(stderr, failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
