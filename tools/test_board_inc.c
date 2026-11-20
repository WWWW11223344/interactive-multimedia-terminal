/* Headless check of the whiteboard incremental path: a stroke must land in both the
 * saved drawing and the visible frame, and must report only a small dirty rectangle.
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

int main(void)
{
    App app;
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) { fprintf(stderr, "app_open failed\n"); return 2; }
    app.page = PAGE_BOARD;
    app.dirty = true;
    app_render(&app);
    expect(app.dirty == false, "render clears the full-redraw flag");
    expect(app.dirtyArea.w == 0, "no pending area after a full render");

    app_event(&app, (Event){EVENT_DOWN,120,200});
    expect(app.painting, "press inside the canvas starts a stroke");
    expect(app.dirty == false, "a stamp does not request a full redraw");
    expect(app.dirtyArea.w > 0 && app.dirtyArea.w < 60, "dirty area stays local");

    app_event(&app, (Event){EVENT_MOVE,180,240});
    app_event(&app, (Event){EVENT_MOVE,240,260});
    app_event(&app, (Event){EVENT_UP,240,260});
    expect(app.dirty == false, "a finished stroke still needs no full redraw");

    uint32_t ink = 0x203B3C;
    expect(app.screen.pixels[200*800+120] != 0xFFFFFF, "screen shows the stroke at the press point");
    expect(app.screen.pixels[260*800+240] != 0xFFFFFF, "screen shows the stroke at the release point");
    expect(app.drawing.pixels[(200-156)*752+(120-24)] != 0xFFFFFF, "drawing buffer has the same stroke");
    (void)ink;

    image_save("shots/board_stroke.bmp", &app.screen);

    /* Back to a full render: the canvas is re-blitted, nothing is lost. */
    app.dirty = true;
    app_render(&app);
    expect(app.screen.pixels[260*800+240] != 0xFFFFFF, "stroke survives a full repaint");

    app_close(&app);
    fprintf(stderr, failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
