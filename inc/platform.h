/* Codex-assisted adaptation, 2026-11-19: framebuffer/evdev and Windows preview. */
#ifndef TERMINAL_PLATFORM_H
#define TERMINAL_PLATFORM_H
#include "terminal.h"
/* One platform instance, owned by the main loop. Poll returns -1 on device loss. */
bool platform_open(const char *fbPath, const char *touchPath, bool swapXY, bool invertX, bool invertY);
int platform_poll(Event *event, int timeoutMs);
void platform_present(const Image *image);
void platform_close(void);
#endif
