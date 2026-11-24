/* 无界面检查：相册的每一个格子，尤其是顶行，都能打开各自的图片，
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

/* 与 AlbumSystem.c 中 thumb_cell() 相同的几何布局，这里显式写出来，
   这样那边一旦悄悄改动，就会表现为测试失败，而不是一张错误的图片。 */
static int cell_x(int column) { return 24 + column * 252 + 126; }
static int cell_y(int row) { return 86 + row * 162 + 81; }

int main(void)
{
    App app;
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) { fprintf(stderr, "app_open failed\n"); return 2; }
    if (!album_scan(&app)) { fprintf(stderr, "album_scan failed\n"); return 2; }
    fprintf(stderr, "album has %zu pictures\n", app.album.count);
    if (app.album.count < 6) { fprintf(stderr, "need six pictures in res/Album\n"); return 2; }

    char what[96];
    for (int i = 0; i < 6; ++i) {
        app.page = PAGE_ALBUM;
        app.album.selected = 99;
        tap(&app, cell_x(i % 3), cell_y(i / 3));
        snprintf(what, sizeof(what), "cell %d (%s row) opens picture %d", i, i < 3 ? "top" : "bottom", i);
        expect(app.page == PAGE_PHOTO && app.album.selected == (size_t)i, what);
    }

    /* 顶行在上报点偏低 60px 和偏高 60px 时：仍然打开同一张图片。 */
    app.page = PAGE_ALBUM; app.album.selected = 99;
    tap(&app, cell_x(0), cell_y(0) + 60);
    expect(app.page == PAGE_PHOTO && app.album.selected == 0, "top-left still opens with the point 60px low");

    app.page = PAGE_ALBUM; app.album.selected = 99;
    tap(&app, cell_x(2), cell_y(0) - 60);
    expect(app.page == PAGE_PHOTO && app.album.selected == 2, "top-right still opens with the point 60px high");

    /* 两行之间的点必须落到其中一行，绝不能落空。 */
    app.page = PAGE_ALBUM; app.album.selected = 99;
    tap(&app, cell_x(1), 247);
    expect(app.page == PAGE_PHOTO && app.album.selected == 1, "the seam between the rows is not a dead band");

    app.page = PAGE_ALBUM; app.album.selected = 99;
    tap(&app, cell_x(1), 249);
    expect(app.page == PAGE_PHOTO && app.album.selected == 4, "just below the seam picks the bottom row");

    /* 在图片页面内，三个按钮仍然可用。 */
    app.page = PAGE_PHOTO; app.album.selected = 0;
    tap(&app, 240, 447);
    expect(app.album.selected + 1 == app.album.count, "上一张 wraps to the last picture");
    tap(&app, 640, 447);
    expect(app.album.selected == 0, "下一张 wraps back to the first");
    tap(&app, 94, 447);
    expect(app.page == PAGE_ALBUM, "缩略图 goes back to the grid");

    app_close(&app);
    fprintf(stderr, failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
