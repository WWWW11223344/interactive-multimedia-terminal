/* Touch calibration maths, 2026-11-20. Links touch_cal.c only: no framebuffer, no
 * evdev, no App, no font. Runs in milliseconds on the desktop, so a fit that would
 * put the login keypad out of reach is caught before it reaches the panel.
 *
 * 这里的重点是"声明范围与实际范围不一致"这一种面板：驱动程序给出的 ABS 范围与它真正
 * 上报的数值不同，屏幕就会有一段区域对应到谁也不曾点到的位置——登录页 1/2/3 键点不动
 * 正是这种症状。断言分两半：这种面板必须能被拟合回正确位置，而完全报不出位置的
 * 面板必须被拒绝。
 */
#include "touchcal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* A panel described by what it really does, so the samples below are generated
   instead of hand-written and cannot quietly agree with a broken formula. */
typedef struct { int swap; int xAt0, xAt799, yAt0, yAt479; } FakePanel;

/* Raw pair the panel would report for a finger at screen (x,y). */
static void report(const FakePanel *panel, int x, int y, int *rawX, int *rawY)
{
    int horizontal = panel->xAt0 + (panel->xAt799 - panel->xAt0) * x / 799;
    int vertical = panel->yAt0 + (panel->yAt479 - panel->yAt0) * y / 479;
    *rawX = panel->swap ? vertical : horizontal;
    *rawY = panel->swap ? horizontal : vertical;
}

static void sample(const FakePanel *panel, int rawX[TOUCH_CAL_POINTS], int rawY[TOUCH_CAL_POINTS])
{
    for (int i = 0; i < TOUCH_CAL_POINTS; ++i) {
        report(panel, TOUCH_CAL_TARGET[i][0], TOUCH_CAL_TARGET[i][1], &rawX[i], &rawY[i]);
    }
}

/* Where a raw pair lands with a given calibration. */
static void locate(const TouchCal *cal, int rawX, int rawY, int *x, int *y)
{
    *x = touch_cal_map(cal->swap ? rawY : rawX, cal->x0, cal->x1, SCREEN_WIDTH);
    *y = touch_cal_map(cal->swap ? rawX : rawY, cal->y0, cal->y1, SCREEN_HEIGHT);
}

/* Every raw pair of the panel must land back on the screen point it came from. */
static void expect_round_trip(const FakePanel *panel, const TouchCal *cal, const char *label)
{
    int probes[6][2] = {{0, 0}, {100, 100}, {400, 240}, {484, 220}, {799, 479}, {60, 396}};
    for (int i = 0; i < 6; ++i) {
        int rawX, rawY, x = 0, y = 0;
        report(panel, probes[i][0], probes[i][1], &rawX, &rawY);
        locate(cal, rawX, rawY, &x, &y);
        assert(abs(x - probes[i][0]) <= 2);
        assert(abs(y - probes[i][1]) <= 2);
    }
    printf("PASS: %s 六处取样拟合后都回到原像素\n", label);
}

/* A panel that reports exactly the declared range is the easy case; it only proves the
   fit does not invent an offset where none exists. */
static void test_true_panel(void)
{
    FakePanel panel = {0, 0, 1023, 0, 599};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.swap == 0 && error <= 2);
    expect_round_trip(&panel, &cal, "1:1 面板");
}

/* The real case: the driver declares one range and the panel reports another. With the
   declared range in force a tap at the key "1" row is mapped somewhere else entirely;
   after calibration the same tap lands on the key. */
static void test_declared_range_is_wrong(void)
{
    /* Panel reports 0..1023 / 0..599 but the driver advertises 0..799 / 0..479, which is
       the screen size rather than the raw range. */
    FakePanel panel = {0, 0, 1023, 0, 599};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    int rawKeyX = 0, rawKeyY = 0, legacy, legacyY;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    expect_round_trip(&panel, &cal, "声明范围错误的面板");

    /* Key "1" centre is (484,220). The old path mapped raw through the declared range. */
    report(&panel, 484, 220, &rawKeyX, &rawKeyY);
    legacy = touch_cal_map(rawKeyX, 0, 799, SCREEN_WIDTH);
    legacyY = touch_cal_map(rawKeyY, 0, 479, SCREEN_HEIGHT);
    assert(abs(legacy - 484) + abs(legacyY - 220) > 60);
    locate(&cal, rawKeyX, rawKeyY, &legacy, &legacyY);
    assert(abs(legacy - 484) <= 2 && abs(legacyY - 220) <= 2);
    puts("PASS: 声明范围与实际不符时，按声明范围映射会偏掉，标定后回到 1 键中心");
}

/* A panel whose channels are crossed needs no --swap-xy: the fit decides it. */
static void test_swapped_panel(void)
{
    FakePanel panel = {1, 0, 1023, 0, 599};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.swap == 1);
    expect_round_trip(&panel, &cal, "轴向交换的面板");
}

/* A mirrored panel reports a descending range; the map runs backwards without a flag. */
static void test_mirrored_panel(void)
{
    FakePanel flipX = {0, 1023, 0, 0, 599};
    FakePanel flipY = {0, 0, 1023, 599, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&flipX, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.swap == 0 && cal.x0 > cal.x1);
    expect_round_trip(&flipX, &cal, "水平镜像的面板");
    sample(&flipY, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.y0 > cal.y1);
    expect_round_trip(&flipY, &cal, "垂直镜像的面板");
}

/* Targets sit well inside the panel, so the edges are extrapolated. Mapping 60..740
   onto 0..799 instead would stretch the screen by 12% and shift keys by half a width. */
static void test_edge_extrapolation(void)
{
    FakePanel panel = {0, 0, 1023, 0, 599};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(abs(cal.x0 - 0) <= 2 && abs(cal.x1 - 1023) <= 2);
    assert(abs(cal.y0 - 0) <= 2 && abs(cal.y1 - 599) <= 2);
    puts("PASS: 靶心内缩后仍能外推到屏幕边缘（x0≈0 x1≈1023 y0≈0 y1≈599）");
}

/* A panel that never reports movement cannot be calibrated, and pretending otherwise
   would clamp the pointer into a corner. */
static void test_rejects_dead_panel(void)
{
    FakePanel panel = {0, 512, 512, 300, 300};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(!touch_cal_solve(rawX, rawY, &cal, &error));
    assert(error < 0 && !touch_cal_valid(&cal));
    puts("PASS: 报不出位置的面板被拒绝（范围坍缩，error<0）");
}

/* One wild sample - the finger landed on the wrong half of the screen - must fail the
   fit rather than drag the whole mapping with it. */
static void test_rejects_wild_sample(void)
{
    FakePanel panel = {0, 0, 1023, 0, 599};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    rawY[4] += 200;
    assert(!touch_cal_solve(rawX, rawY, &cal, &error));
    assert(error > TOUCH_CAL_TOLERANCE);
    printf("PASS: 中心点偏 200 原始单位被拒绝（偏差 %d 像素 > %d）\n", error, TOUCH_CAL_TOLERANCE);
}

int main(void)
{
    test_true_panel();
    test_declared_range_is_wrong();
    test_swapped_panel();
    test_mirrored_panel();
    test_edge_extrapolation();
    test_rejects_dead_panel();
    test_rejects_wild_sample();
    puts("ALL TOUCH CALIBRATION TESTS PASSED (no device, no font, no window)");
    return 0;
}
