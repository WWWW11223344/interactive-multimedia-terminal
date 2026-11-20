/* Touch calibration maths: pure integer arithmetic, no device access, so the fit can
 * be tested on the desktop build (tests/test_touch_cal.c). Added 2026-11-20.
 */
#include "touchcal.h"

/* The four corners sit symmetric about the centre of their own rectangle, so the fifth
   tap is the exact midpoint of both axes: a linear panel must report the average of the
   left and right samples, and of the top and bottom ones. That makes the fifth tap a
   real check on the fit instead of a fifth sample to fit.
 *
 * The rectangle is deliberately kept in the lower two thirds. The panel that motivated
 * this wizard cannot report a position in its top band at all, so a crosshair drawn up
 * there would simply never answer; the fit only needs known screen positions, not the
 * screen corners, and the edge values are extrapolated from these four. */
const int TOUCH_CAL_TARGET[TOUCH_CAL_POINTS][2] = {
    { 60, 272}, {740, 272}, { 60, 440}, {740, 440}, {400, 356}
};

static int cal_abs(int value)
{
    return value < 0 ? -value : value;
}

int touch_cal_map(int raw, int lo, int hi, int size)
{
    int64_t scaled = ((int64_t)raw - lo) * (size - 1) / ((int64_t)hi - lo);
    if (scaled < 0) { scaled = 0; }
    if (scaled >= size) { scaled = size - 1; }
    return (int)scaled;
}

/* Raw value at screen coordinate `screenAt`, given the raw samples taken at
   screenA and screenB. The two samples sit well inside the panel, so the edges are
   extrapolated rather than clamped: mapping 48..752 onto 0..799 would stretch the
   whole screen by 13% and throw every key off by half a key width. */
static int cal_edge(int atA, int atB, int screenA, int screenB, int screenAt)
{
    int64_t run = (int64_t)screenB - screenA;
    int64_t span = (int64_t)atB - atA;
    if (run == 0) { return atA; }
    return atA + (int)(span * ((int64_t)screenAt - screenA) / run);
}

bool touch_cal_valid(const TouchCal *cal)
{
    if (!cal) { return false; }
    return cal_abs(cal->x1 - cal->x0) >= 64 && cal_abs(cal->y1 - cal->y0) >= 64;
}

bool touch_cal_solve(const int rawX[TOUCH_CAL_POINTS], const int rawY[TOUCH_CAL_POINTS],
                     TouchCal *cal, int *errorPx)
{
    int leftX, rightX, leftY, rightY;
    int topX, bottomX, topY, bottomY;
    int swap, worst, i;
    if (!cal) { return false; }
    /* Average the two taps that share a side, so one clumsy finger is half a sample
       instead of half the calibration. */
    leftX = (rawX[0] + rawX[2]) / 2;
    rightX = (rawX[1] + rawX[3]) / 2;
    leftY = (rawY[0] + rawY[2]) / 2;
    rightY = (rawY[1] + rawY[3]) / 2;
    topX = (rawX[0] + rawX[1]) / 2;
    bottomX = (rawX[2] + rawX[3]) / 2;
    topY = (rawY[0] + rawY[1]) / 2;
    bottomY = (rawY[2] + rawY[3]) / 2;
    /* Which raw channel moved when the finger moved from left to right? Comparing
       the two spreads also decides a swapped panel, so no --swap-xy is needed. */
    swap = cal_abs(rightY - leftY) > cal_abs(rightX - leftX) ? 1 : 0;
    cal->swap = swap;
    cal->x0 = cal_edge(swap ? leftY : leftX, swap ? rightY : rightX,
                       TOUCH_CAL_TARGET[0][0], TOUCH_CAL_TARGET[1][0], 0);
    cal->x1 = cal_edge(swap ? leftY : leftX, swap ? rightY : rightX,
                       TOUCH_CAL_TARGET[0][0], TOUCH_CAL_TARGET[1][0], SCREEN_WIDTH - 1);
    cal->y0 = cal_edge(swap ? topX : topY, swap ? bottomX : bottomY,
                       TOUCH_CAL_TARGET[0][1], TOUCH_CAL_TARGET[2][1], 0);
    cal->y1 = cal_edge(swap ? topX : topY, swap ? bottomX : bottomY,
                       TOUCH_CAL_TARGET[0][1], TOUCH_CAL_TARGET[2][1], SCREEN_HEIGHT - 1);
    if (!touch_cal_valid(cal)) {
        if (errorPx) { *errorPx = -1; }
        return false;
    }
    /* The fit is checked against the very samples it was built from, including the
       centre one, which no edge sample can influence on its own. A panel that
       reports a dead strip fails here instead of silently shifting every key. */
    worst = 0;
    for (i = 0; i < TOUCH_CAL_POINTS; ++i) {
        int horizontal = touch_cal_map(swap ? rawY[i] : rawX[i], cal->x0, cal->x1, SCREEN_WIDTH);
        int vertical = touch_cal_map(swap ? rawX[i] : rawY[i], cal->y0, cal->y1, SCREEN_HEIGHT);
        int dx = cal_abs(horizontal - TOUCH_CAL_TARGET[i][0]);
        int dy = cal_abs(vertical - TOUCH_CAL_TARGET[i][1]);
        if (dx > worst) { worst = dx; }
        if (dy > worst) { worst = dy; }
    }
    if (errorPx) { *errorPx = worst; }
    return worst <= TOUCH_CAL_TOLERANCE;
}
