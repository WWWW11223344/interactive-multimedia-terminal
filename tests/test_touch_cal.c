/* 触摸校准算法，2026-11-20。只链接 touch_cal.c：无 framebuffer、无 evdev、
 * 无 App、无字体。在宿主机上以毫秒级运行，因此能把登录键盘变得够不着的拟合
 * 会在到达开发板之前就被发现。
 *
 * 关键案例是促成这次修改的那块开发板：屏幕 y 为 70、250、430 时，原始 y 为 86、330、518。
 * 两端拟合成的直线对中间区域的斜率是错的，因此单直线拟合会使屏幕中心偏低约 23 像素
 * ——半个按键的高度——导致第一排按键按到第二排。下面的断言要求两段式映射
 * 能复现这样的开发板，并要求单直线拟合无法通过，从而把引入第二段的原因
 * 写进测试本身，而不只是写在注释里。
 */
#include "touchcal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* 用面板的真实行为来描述它，这样下面的采样点由公式生成，而非手写，
   也就不会悄悄与错误的公式相互吻合。
 *
 * straight  ：一条直线，无论它报告的范围如何。yTop/yBottom 是屏幕 y = 70 和 430
 *              （外侧锚点）处的原始值，而不是屏幕边缘处的值。
 * bend      ：在屏幕 y=250 处，响应相对穿过两个外侧锚点的直线偏移 `bend` 个原始单位，
   在两端处偏移为零。bend=28、yTop=86、yBottom=518 可复现该开发板自身的读数：
   屏幕 y 为 70 / 250 / 430 时分别为 86 / 330 / 518。 */
typedef struct { int swap; int xAt0, xAt799; int yTop, yBottom, bend; } FakePanel;

/* 手指位于屏幕 (x,y) 时面板会报告的原始坐标对。 */
static void report(const FakePanel *panel, int x, int y, int *rawX, int *rawY)
{
    int horizontal = panel->xAt0 + (panel->xAt799 - panel->xAt0) * x / 799;
    int clamped = y;
    int64_t span, bend = 0;
    if (clamped < TOUCH_CAL_AXIS_Y[0]) { clamped = TOUCH_CAL_AXIS_Y[0]; }
    if (clamped > TOUCH_CAL_AXIS_Y[2]) { clamped = TOUCH_CAL_AXIS_Y[2]; }
    if (panel->bend) {
        span = (int64_t)(clamped - TOUCH_CAL_AXIS_Y[0]) *
               (TOUCH_CAL_AXIS_Y[2] - clamped);
        bend = (int64_t)panel->bend * span /
               ((TOUCH_CAL_AXIS_Y[1] - TOUCH_CAL_AXIS_Y[0]) *
                (TOUCH_CAL_AXIS_Y[2] - TOUCH_CAL_AXIS_Y[1]));
    }
    {
        int vertical = panel->yTop + (panel->yBottom - panel->yTop) *
            (y - TOUCH_CAL_AXIS_Y[0]) / (TOUCH_CAL_AXIS_Y[2] - TOUCH_CAL_AXIS_Y[0]) + (int)bend;
        *rawX = panel->swap ? vertical : horizontal;
        *rawY = panel->swap ? horizontal : vertical;
    }
}

static void sample(const FakePanel *panel, int rawX[TOUCH_CAL_POINTS], int rawY[TOUCH_CAL_POINTS])
{
    for (int i = 0; i < TOUCH_CAL_POINTS; ++i) {
        report(panel, TOUCH_CAL_TARGET[i][0], TOUCH_CAL_TARGET[i][1], &rawX[i], &rawY[i]);
    }
}

/* 用测得的映射计算某原始坐标对会落到屏幕的哪个位置。 */
static void locate(const TouchCal *cal, int rawX, int rawY, int *x, int *y)
{
    *x = touch_cal_map_piece(cal->swap ? rawY : rawX, cal->rawX, TOUCH_CAL_AXIS_X, SCREEN_WIDTH);
    *y = touch_cal_map_piece(cal->swap ? rawX : rawY, cal->rawY, TOUCH_CAL_AXIS_Y, SCREEN_HEIGHT);
}

/* 面板的每一组原始坐标都必须还原到它原本对应的屏幕点上。 */
static void expect_round_trip(const FakePanel *panel, const TouchCal *cal, const char *label,
                             int tolerance)
{
    int probes[6][2] = {{0, 0}, {100, 100}, {400, 240}, {484, 220}, {799, 479}, {60, 396}};
    for (int i = 0; i < 6; ++i) {
        int rawX, rawY, x = 0, y = 0;
        report(panel, probes[i][0], probes[i][1], &rawX, &rawY);
        locate(cal, rawX, rawY, &x, &y);
        assert(abs(x - probes[i][0]) <= tolerance);
        assert(abs(y - probes[i][1]) <= tolerance);
    }
    printf("PASS: %s 六处取样都回到原像素（误差 ≤ %d）\n", label, tolerance);
}

/* 报告范围与声明范围完全一致的面板属于简单情况；
   它只能证明拟合不会凭空造出并不存在的弯曲。 */
static void test_straight_panel(void)
{
    FakePanel panel = {0, 0, 1023, 87, 537, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.swap == 0 && error <= 2);
    expect_round_trip(&panel, &cal, "1:1 直线面板", 2);
}

/* 该开发板自身的数值：屏幕 y = 70 / 250 / 430 时，原始 y = 86 / 330 / 518。 */
static void test_bent_panel(void)
{
    FakePanel panel = {0, 0, 1023, 86, 518, 28};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    int rawKeyX = 0, rawKeyY = 0, line, lineY, measured;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(error <= 2);
    expect_round_trip(&panel, &cal, "中间下陷 28 原始单位的弯曲面板", 8);

/* 用穿过首尾两行的一条直线来映射同样的点击：在两端是对的，在按键所在处却是错的。
   在这块开发板上该偏差为 23 像素。 */
    report(&panel, 400, 250, &rawKeyX, &rawKeyY);
    line = cal.rawY[0] + (cal.rawY[2] - cal.rawY[0]) * (0 - TOUCH_CAL_AXIS_Y[0]) /
        (TOUCH_CAL_AXIS_Y[2] - TOUCH_CAL_AXIS_Y[0]);
    lineY = touch_cal_map(rawKeyY, line,
        line + (cal.rawY[2] - cal.rawY[0]) * (SCREEN_HEIGHT - 1) /
            (TOUCH_CAL_AXIS_Y[2] - TOUCH_CAL_AXIS_Y[0]), SCREEN_HEIGHT);
    assert(abs(lineY - TOUCH_CAL_AXIS_Y[1]) > 15);
    locate(&cal, rawKeyX, rawKeyY, &line, &measured);
    assert(abs(measured - 250) <= 2);
    printf("PASS: 弯曲面板上直线拟合偏 %d 像素，两段映射回到 %d（目标 250）\n",
        abs(lineY - TOUCH_CAL_AXIS_Y[1]), measured);
}

/* 通道交叉的面板不需要 --swap-xy：由拟合自行判断。 */
static void test_swapped_panel(void)
{
    FakePanel panel = {1, 0, 1023, 87, 537, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.swap == 1);
    expect_round_trip(&panel, &cal, "轴向交换的面板", 2);
}

/* 镜像面板报告的是递减范围；映射会自动反向，无需额外标志。 */
static void test_mirrored_panel(void)
{
    FakePanel flipX = {0, 1023, 0, 0, 599, 0};
    FakePanel flipY = {0, 0, 1023, 599, 0, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&flipX, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.swap == 0 && cal.rawX[0] > cal.rawX[2]);
    expect_round_trip(&flipX, &cal, "水平镜像的面板", 2);
    sample(&flipY, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    assert(cal.rawY[0] > cal.rawY[2]);
    expect_round_trip(&flipY, &cal, "垂直镜像的面板", 2);
}

/* 锚点位于面板内部较深的位置，因此边缘由外侧线段外推得到。若改为把 60..740 映射到
   0..799，屏幕会被拉伸 12%，每个按键都会偏移半个键宽。 */
static void test_edge_extrapolation(void)
{
    FakePanel panel = {0, 0, 1023, 87, 537, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    sample(&panel, rawX, rawY);
    assert(touch_cal_solve(rawX, rawY, &cal, &error));
    touch_cal_edges(cal.rawX, TOUCH_CAL_AXIS_X, SCREEN_WIDTH, &x0, &x1);
    touch_cal_edges(cal.rawY, TOUCH_CAL_AXIS_Y, SCREEN_HEIGHT, &y0, &y1);
    assert(abs(x0) <= 2 && abs(x1 - 1023) <= 2);
    assert(abs(y0) <= 2 && abs(y1 - 599) <= 2);
    puts("PASS: 靶心内缩后仍能外推到屏幕边缘（x 0..1023、y 0..599）");
}

/* 始终不报告位置变化的面板无法校准，如果强行假装成功，只会把光标钳制在某个角落。 */
static void test_rejects_dead_panel(void)
{
    FakePanel panel = {0, 512, 512, 300, 300, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    assert(!touch_cal_solve(rawX, rawY, &cal, &error));
    assert(error < 0 && !touch_cal_valid(&cal));
    puts("PASS: 报不出位置的面板被拒绝（锚点坍缩，error<0）");
}

/* 一个偏离很大的采样点——手指按错了十字——必须让拟合失败，
   而不是拖着整片屏幕的区域跟着偏移。 */
static void test_rejects_wild_sample(void)
{
    FakePanel panel = {0, 0, 1023, 87, 537, 0};
    TouchCal cal;
    int rawX[TOUCH_CAL_POINTS], rawY[TOUCH_CAL_POINTS], error = 0;
    sample(&panel, rawX, rawY);
    rawY[1] += 260;
    assert(!touch_cal_solve(rawX, rawY, &cal, &error));
    assert(error > TOUCH_CAL_TOLERANCE);
    printf("PASS: 某一点偏 260 原始单位被拒绝（偏差 %d 像素 > %d）\n", error, TOUCH_CAL_TOLERANCE);
}

int main(void)
{
    test_straight_panel();
    test_bent_panel();
    test_swapped_panel();
    test_mirrored_panel();
    test_edge_extrapolation();
    test_rejects_dead_panel();
    test_rejects_wild_sample();
    puts("ALL TOUCH CALIBRATION TESTS PASSED (no device, no font, no window)");
    return 0;
}
