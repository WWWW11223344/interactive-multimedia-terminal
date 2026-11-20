/* Codex-assisted adaptation, 2026-11-19: framebuffer/evdev and Windows preview. */
#ifndef TERMINAL_PLATFORM_H
#define TERMINAL_PLATFORM_H
#include "terminal.h"
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
#endif
