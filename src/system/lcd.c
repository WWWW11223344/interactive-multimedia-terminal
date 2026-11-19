/* Xie Donghong framebuffer drawing adapted by Codex, 2026-11-19: clipping and correct scaling. */
#include "display.h"
#include <stdlib.h>

/* Hit testing uses half-open bounds so neighbouring buttons never overlap. */
bool rect_has(Rect rect, int x, int y)
{
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

/* Clip before every write; row zero and column zero are valid pixels. */
void draw_rect(Image *target, Rect rect, uint32_t rgb)
{
    if (!target || !target->pixels) { return; }
    int x0 = rect.x < 0 ? 0 : rect.x, y0 = rect.y < 0 ? 0 : rect.y;
    int x1 = rect.x + rect.w, y1 = rect.y + rect.h;
    if (x1 > target->width) { x1 = target->width; }
    if (y1 > target->height) { y1 = target->height; }
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) { target->pixels[y * target->width + x] = rgb; }
    }
}

/* Small bounded brush stamps also support drawing at the canvas edge. */
void draw_disc(Image *target, int x, int y, int radius, uint32_t rgb)
{
    if (radius < 1 || radius > 32) { return; }
    for (int j = -radius; j <= radius; ++j) {
        for (int i = -radius; i <= radius; ++i) {
            if (i * i + j * j <= radius * radius) { draw_rect(target, (Rect){x+i,y+j,1,1}, rgb); }
        }
    }
}

/* Bresenham connects sparse touchscreen samples without gaps. */
void draw_line(Image *target, int x0, int y0, int x1, int y1, int radius, uint32_t rgb)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        draw_disc(target, x0, y0, radius, rgb);
        if (x0 == x1 && y0 == y1) { break; }
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

/* Multiply before dividing; fit preserves aspect ratio for portrait and landscape images. */
void draw_image(Image *target, Rect rect, const Image *source, bool fit)
{
    if (!source || !source->pixels || rect.w <= 0 || rect.h <= 0) { return; }
    if (fit) {
        int width = rect.w, height = rect.h;
        if ((int64_t)source->width * height > (int64_t)source->height * width) {
            height = (int)((int64_t)width * source->height / source->width);
        } else { width = (int)((int64_t)height * source->width / source->height); }
        rect.x += (rect.w - width) / 2; rect.y += (rect.h - height) / 2;
        rect.w = width; rect.h = height;
    }
    for (int y = 0; y < rect.h; ++y) {
        int ty = rect.y + y;
        if (ty < 0 || ty >= target->height) { continue; }
        for (int x = 0; x < rect.w; ++x) {
            int tx = rect.x + x;
            if (tx >= 0 && tx < target->width) {
                target->pixels[ty * target->width + tx] =
                    source->pixels[(int64_t)y * source->height / rect.h * source->width +
                                   (int64_t)x * source->width / rect.w];
            }
        }
    }
}
