/* 临时诊断：点击 2048 的左键到底会发生什么？
 * Codex, 2026-11-19. */
#include "terminal.h"
#include <stdio.h>
#include <string.h>

static void dump(const char *tag, const App *app)
{
    fprintf(stderr, "%s: page=%d started=%d over=%d won=%d seed=%u down=(%d,%d)\n",
        tag, (int)app->page, app->game.started, app->game.over, app->game.won,
        app->game.seed, app->downX, app->downY);
    fprintf(stderr, "   cells:");
    for (int i = 0; i < 16; ++i) { fprintf(stderr, " %u", app->game.cells[i]); }
    fprintf(stderr, "\n");
}

int main(void)
{
    App app;
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) { return 2; }
    app.page = PAGE_2048;
    app.dirty = true;
    app_render(&app);

    memset(&app.game, 0, sizeof(app.game));
    app.game.started = true;
    app.game.seed = 12345u;
    app.game.cells[3] = 2;
    dump("before", &app);

    app_event(&app, (Event){EVENT_DOWN, 469, 222});
    fprintf(stderr, "after DOWN: pressed=%d down=(%d,%d)\n", app.pressed, app.downX, app.downY);
    app_event(&app, (Event){EVENT_UP, 469, 222});
    dump("after tap", &app);

    /* 同样的点击，但直接通过 ui_clicked 求值。 */
    fprintf(stderr, "rect_has(left,%d,%d)=%d\n", 469, 222, rect_has((Rect){430,196,78,52}, 469, 222));
    app_close(&app);
    return 0;
}
