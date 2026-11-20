/* Codex-assisted adaptation, 2026-11-19: framebuffer/evdev and Windows preview. */
#ifndef TERMINAL_PLATFORM_H
#define TERMINAL_PLATFORM_H
#include "terminal.h"
#include "touchcal.h"
/* One platform instance, owned by the main loop. Poll returns -1 on device loss.
   xBias/yBias shift every reported touch by that many pixels; positive moves the
   pointer right/down. Use them to align the panel with what the user sees. */
bool platform_open(const char *fbPath, const char *touchPath, bool swapXY, bool invertX, bool invertY,
                   int xBias, int yBias);
int platform_poll(Event *event, int timeoutMs);
void platform_present(const Image *image);
/* Push only the given screen rectangle; used while a pen stroke is in progress so
   the panel does not have to receive a full frame per sample. */
void platform_present_area(const Image *image, Rect area);
void platform_close(void);
/* True for the desktop preview, where the pointer already speaks screen pixels and a
   calibration pass would only waste five clicks. */
bool platform_is_preview(void);

/* Measured raw ranges for this panel, stored next to the account list. Loading is
   advisory: a missing or unusable file leaves the driver-declared range in force. */
bool platform_cal_load(TouchCal *cal);
bool platform_cal_save(const TouchCal *cal);
/* Takes effect on the next sample and lasts for the rest of the run; a degenerate
   record is ignored so a truncated file cannot blind the panel. */
void platform_cal_apply(const TouchCal *cal);
/* The five-tap wizard. Blocks, draws on app->screen, and returns when the panel is
   calibrated, when the user gives up, or when the touch device disappears. */
void touch_calibrate(App *app);
#endif
