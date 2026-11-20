/* Touch calibration maths, no device access: the panel is measured once on the
 * board and the result is a pair of raw ranges per screen axis. Added 2026-11-20
 * because the gslx680 driver on this panel does not always declare the range it
 * actually reports, which leaves a band of the screen unreachable.
 */
#ifndef TERMINAL_TOUCHCAL_H
#define TERMINAL_TOUCHCAL_H
#include "display.h"

/* Five targets in screen pixels, asked for in this order: top-left, top-right,
   bottom-left, bottom-right, centre. The first four fix the axes, the centre one
   only checks the result, so a panel that reports a wrong area cannot pass. */
#define TOUCH_CAL_POINTS 5
extern const int TOUCH_CAL_TARGET[TOUCH_CAL_POINTS][2];

/* Raw ranges of the axis measured on the panel, expressed at the screen edges:
   raw value at column 0, at column 799, at row 0 and at row 479. swap=1 means the
   raw Y channel drives the horizontal axis. Either pair may run backwards, so a
   mirrored panel needs no separate flag. */
typedef struct { int swap; int x0, x1, y0, y1; } TouchCal;

/* Linear map of one raw axis onto 0..size-1, clamped. Descending ranges map in
   reverse, which is what makes an inverted panel work without extra flags. */
int touch_cal_map(int raw, int lo, int hi, int size);

/* Fit the five samples. Returns false when a pair of samples collapsed (the tap
   never arrived where it was asked for) or when any target misses by more than
   TOUCH_CAL_TOLERANCE pixels; *errorPx receives the worst miss either way. */
#define TOUCH_CAL_TOLERANCE 40
bool touch_cal_solve(const int rawX[TOUCH_CAL_POINTS], const int rawY[TOUCH_CAL_POINTS],
                     TouchCal *cal, int *errorPx);

/* True when a record describes a usable panel rather than an empty file. */
bool touch_cal_valid(const TouchCal *cal);
#endif
