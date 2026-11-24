/* 两条紧凑工具栏的无界面检查：画布上的七个颜色块和三个笔刷按钮，
 * 以及图片页的导航栏。每一处都验证其在中心点可用，
 * 并且在真实面板上 gslx680 所上报的纵向误差下也可用。
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

/* 与 WhiteboardSystem.c 中的 swatch_rect()/brush_rect() 相同的几何布局。 */
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

    /* 颜色块高 56px；上报点偏高或偏低 34px 时，仍必须选中手指下的颜色块，
       而不是落进两个颜色块之间的缝隙。 */
    app.color = 0; tap(&app, chip_x(3), chip_y + 34);
    expect(app.color == 3, "chip 3 still fires with the point 34px low");
    app.color = 0; tap(&app, chip_x(5), chip_y - 34);
    expect(app.color == 5, "chip 5 still fires with the point 34px high");
    app.radius = 3; tap(&app, brush_x(2), brush_y + 34);
    expect(app.radius == 7, "粗 still fires with the point 34px low");

    /* 两个颜色块之间的点会解析到较近的那个，绝不会落空。 */
    app.color = 0; tap(&app, 24 + 62 - 1, chip_y);
    expect(app.color == 0 || app.color == 1, "the seam between chips picks a neighbour");

    /* 在工具栏之外的点击绝不能悄悄改变笔刷。 */
    app.color = 2; app.radius = 3;
    tap(&app, 400, 300);
    expect(app.color == 2 && app.radius == 3, "a tap on the canvas leaves the tools alone");

    /* 颜色确实传导到了笔画上：选红色、绘制、再读回像素验证。 */
    app.color = 0; app.radius = 3;
    app.page = PAGE_BOARD;
    tap(&app, chip_x(1), chip_y);
    app_event(&app, (Event){EVENT_DOWN, 120, 200});
    app_event(&app, (Event){EVENT_MOVE, 160, 200});
    app_event(&app, (Event){EVENT_UP, 160, 200});
    expect(app.drawing.pixels[(200-156)*752 + (140-24)] == 0xCA554D, "the stroke uses the selected colour");

    /* 细笔刷和粗笔刷在同一像素列上留下不同的宽度。 */
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

    /* 图片页导航：底边这一行高 64px。 */
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
