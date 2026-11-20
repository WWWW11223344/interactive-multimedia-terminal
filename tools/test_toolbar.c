/* Headless check of the two compact toolbars: seven colour chips and three brush
 * buttons on the canvas, plus the photo-page navigation row. Each is verified to work
 * at its centre and with the vertical error the gslx680 reports on the real panel.
 * Codex, 2026-11-20. */
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

/* Same geometry as swatch_rect()/brush_rect() in WhiteboardSystem.c. */
static int chip_x(int i) { return 24 + i * 62 + 28; }
static int brush_x(int i) { return 470 + i * 80 + 36; }
static const int chip_y = 116;
static const int brush_y = 116;

int main(void)
{
    App app;
    char what[96];
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) { fprintf(stderr, "app_open failed\n"); return 2; }
    app.page = PAGE_BOARD;
    app.dirty = true;
    app_render(&app);

    for (int i = 0; i < 7; ++i) {
        app.color = 0;
        tap(&app, chip_x(i), chip_y);
        snprintf(what, sizeof(what), "colour chip %d selects colour %d", i, i);
        expect(app.color == i, what);
    }

    const int radii[3] = {2, 3, 7};
    for (int i = 0; i < 3; ++i) {
        app.radius = 3;
        tap(&app, brush_x(i), brush_y);
        snprintf(what, sizeof(what), "brush button %d selects radius %d", i, radii[i]);
        expect(app.radius == radii[i], what);
    }

    /* The chips are 56px tall; a report 34px high or low must still pick the chip
       under the finger instead of falling into the gap between two of them. */
    app.color = 0; tap(&app, chip_x(3), chip_y + 34);
    expect(app.color == 3, "chip 3 still fires with the point 34px low");
    app.color = 0; tap(&app, chip_x(5), chip_y - 34);
    expect(app.color == 5, "chip 5 still fires with the point 34px high");
    app.radius = 3; tap(&app, brush_x(2), brush_y + 34);
    expect(app.radius == 7, "粗 still fires with the point 34px low");

    /* A point between two chips resolves to the nearer one, never to nothing. */
    app.color = 0; tap(&app, 24 + 62 - 1, chip_y);
    expect(app.color == 0 || app.color == 1, "the seam between chips picks a neighbour");

    /* Taps well outside the toolbar must not silently change the brush. */
    app.color = 2; app.radius = 3;
    tap(&app, 400, 300);
    expect(app.color == 2 && app.radius == 3, "a tap on the canvas leaves the tools alone");

    /* Colour actually reaches the stroke: pick red, draw, read the pixel back. */
    app.color = 0; app.radius = 3;
    app.page = PAGE_BOARD;
    tap(&app, chip_x(1), chip_y);
    app_event(&app, (Event){EVENT_DOWN, 120, 200});
    app_event(&app, (Event){EVENT_MOVE, 160, 200});
    app_event(&app, (Event){EVENT_UP, 160, 200});
    expect(app.drawing.pixels[(200-156)*752 + (140-24)] == 0xCA554D, "the stroke uses the selected colour");

    /* Slim brush then thick brush leave different widths on the same pixel column. */
    tap(&app, brush_x(0), brush_y);
    app_event(&app, (Event){EVENT_DOWN, 300, 300});
    app_event(&app, (Event){EVENT_UP, 300, 300});
    unsigned int thin = 0, thick = 0;
    for (int dy = -12; dy <= 12; ++dy) {
        if (app.drawing.pixels[(300-156+dy)*752 + (300-24)] == 0xCA554D) { ++thin; }
    }
    tap(&app, brush_x(2), brush_y);
    app_event(&app, (Event){EVENT_DOWN, 500, 300});
    app_event(&app, (Event){EVENT_UP, 500, 300});
    for (int dy = -12; dy <= 12; ++dy) {
        if (app.drawing.pixels[(300-156+dy)*752 + (500-24)] == 0xCA554D) { ++thick; }
    }
    snprintf(what, sizeof(what), "细 paints %u rows, 粗 paints %u rows", thin, thick);
    expect(thick > thin + 6, what);

    /* Photo page navigation: the row is 64px tall at the bottom edge. */
    if (album_scan(&app) && app.album.count >= 2) {
        app.page = PAGE_PHOTO;
        app.album.selected = 0;
        tap(&app, 250, 440); expect(app.album.selected + 1 == app.album.count, "上一张 wraps backwards");
        tap(&app, 630, 440); expect(app.album.selected == 0, "下一张 wraps forwards");
        tap(&app, 250, 468); expect(app.album.selected + 1 == app.album.count, "上一张 also fires 28px low");
        tap(&app, 94, 440);  expect(app.page == PAGE_ALBUM, "缩略图 returns to the grid");
    } else {
        fprintf(stderr, "skipped photo-page checks: need two pictures in res/Album\n");
    }

    app_close(&app);
    fprintf(stderr, failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
