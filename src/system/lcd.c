/* Xie Donghong framebuffer drawing adapted by Codex, 2026-11-19: clipping and correct scaling. */
#include "display.h"
#include <stdlib.h>
#include <string.h>

/* Hit testing uses half-open bounds so neighbouring buttons never overlap. */
bool rect_has(Rect rect, int x, int y)
{
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

/* Clip before every write; row zero and column zero are valid pixels. The first
   row is filled pixel by pixel and the rest is copied, which matters because every
   screen starts with a full 800x480 clear. */
void draw_rect(Image *target, Rect rect, uint32_t rgb)
{
    if (!target || !target->pixels) { return; }
    int x0 = rect.x < 0 ? 0 : rect.x, y0 = rect.y < 0 ? 0 : rect.y;
    int x1 = rect.x + rect.w, y1 = rect.y + rect.h;
    if (x1 > target->width) { x1 = target->width; }
    if (y1 > target->height) { y1 = target->height; }
    int span = x1 - x0;
    if (span <= 0) { return; }
    uint32_t *first = target->pixels + y0 * target->width + x0;
    for (int x = 0; x < span; ++x) { first[x] = rgb; }
    for (int y = y0 + 1; y < y1; ++y) {
        memcpy(target->pixels + y * target->width + x0, first, (size_t)span * sizeof(uint32_t));
    }
}

/* Small bounded brush stamps also support drawing at the canvas edge. One horizontal
   span per row instead of one clamped rectangle per pixel. */
void draw_disc(Image *target, int x, int y, int radius, uint32_t rgb)
{
    if (!target || !target->pixels || radius < 1 || radius > 32) { return; }
    for (int j = -radius; j <= radius; ++j) {
        int py = y + j;
        if (py < 0 || py >= target->height) { continue; }
        int half = 0;
        while ((half + 1) * (half + 1) + j * j <= radius * radius) { ++half; }
        int x0 = x - half, x1 = x + half;
        if (x0 < 0) { x0 = 0; }
        if (x1 >= target->width) { x1 = target->width - 1; }
        if (x1 < x0) { continue; }
        uint32_t *row = target->pixels + py * target->width;
        for (int px = x0; px <= x1; ++px) { row[px] = rgb; }
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

/* Multiply before dividing; fit preserves aspect ratio for portrait and landscape images.
   The 1:1 case is a row copy: the canvas and the 2048 tiles are blitted every frame,
   and a per-pixel 64-bit divide here used to cost more than the whole rest of a frame. */
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
    if (rect.w == source->width && rect.h == source->height) {
        for (int y = 0; y < rect.h; ++y) {
            int ty = rect.y + y;
            if (ty < 0 || ty >= target->height) { continue; }
            int x0 = rect.x < 0 ? 0 : rect.x;
            int x1 = rect.x + rect.w;
            if (x1 > target->width) { x1 = target->width; }
            if (x1 > x0) {
                memcpy(&target->pixels[ty * target->width + x0],
                       &source->pixels[y * source->width + (x0 - rect.x)],
                       (size_t)(x1 - x0) * sizeof(uint32_t));
            }
        }
        return;
    }
    /* Scaled: 32-bit arithmetic is enough for 800px and removes the soft 64-bit divide. */
    for (int y = 0; y < rect.h; ++y) {
        int ty = rect.y + y;
        if (ty < 0 || ty >= target->height) { continue; }
        const uint32_t *row = &source->pixels[y * source->height / rect.h * source->width];
        uint32_t *out = &target->pixels[ty * target->width];
        for (int x = 0; x < rect.w; ++x) {
            int tx = rect.x + x;
            if (tx >= 0 && tx < target->width) { out[tx] = row[x * source->width / rect.w]; }
        }
    }
}
