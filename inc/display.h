/* Codex-assisted adaptation, 2026-11-19: bounded software canvas for both targets. */
#ifndef TERMINAL_DISPLAY_H
#define TERMINAL_DISPLAY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
typedef struct { int x, y, w, h; } Rect;
typedef struct { int width, height; uint32_t *pixels; } Image;
/* Images own their pixels. Failed reads leave an empty image. Trusted local BMP only. */
bool image_load(const char *path, Image *out);
bool image_save(const char *path, const Image *image);
void image_free(Image *image);
bool image_create(Image *image, int width, int height);
bool rect_has(Rect rect, int x, int y);
void draw_rect(Image *target, Rect rect, uint32_t rgb);
void draw_disc(Image *target, int x, int y, int radius, uint32_t rgb);
void draw_line(Image *target, int x0, int y0, int x1, int y1, int radius, uint32_t rgb);
void draw_image(Image *target, Rect rect, const Image *source, bool fit);
bool text_open(const char *path);
void text_close(void);
void draw_text(Image *target, int x, int y, int size, uint32_t rgb, const char *text);
/* Advance width of a UTF-8 string at the given pixel height; lets callers centre
   labels without measuring twice. Returns 0 when the font is not open. */
int text_width(const char *text, int size);
#endif
