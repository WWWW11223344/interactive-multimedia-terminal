/* Font adapter derived from Xie's print/font modules. Codex, 2026-11-19: bounded glyph rendering. */
#include "display.h"
#include "truetype.h"
#include <stdio.h>
#include <stdlib.h>
static uint8_t *g_fontData;
static stbtt_fontinfo g_font;

/* Load the bundled, trusted TTF once; the upstream rasterizer borrows this buffer. */
bool text_open(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) { return false; }
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return false; }
    long size = ftell(file);
    if (size < 12 || size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return false;
    }
    g_fontData = malloc((size_t)size);
    bool ok = g_fontData && fread(g_fontData, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (!ok || !stbtt_InitFont(&g_font, g_fontData, 0)) { text_close(); return false; }
    return true;
}

/* Close only after the UI has stopped using the borrowed font bytes. */
void text_close(void)
{
    free(g_fontData); g_fontData = NULL;
}

/* Decode bounded UTF-8 UI text; malformed bytes become question marks. */
static unsigned int read_codepoint(const unsigned char **text)
{
    const unsigned char *p = *text;
    unsigned int value = *p++, count = 0;
    if (value >= 0xc2 && value <= 0xdf) { count = 1; value &= 31; }
    else if (value >= 0xe0 && value <= 0xef) { count = 2; value &= 15; }
    else if (value >= 0xf0 && value <= 0xf4) { count = 3; value &= 7; }
    else if (value >= 0x80) { value = '?'; }
    for (unsigned int i = 0; i < count; ++i) {
        if ((*p & 0xc0) != 0x80) { *text = p; return '?'; }
        value = (value << 6) | (*p++ & 63);
    }
    *text = p;
    return value;
}

/* Advance width in pixels, summed the same way draw_text advances the cursor, so
   centred labels land on the pixel the rasterizer actually uses. */
int text_width(const char *text, int size)
{
    if (!g_fontData || !text || size < 8 || size > 72) { return 0; }
    float scale = stbtt_ScaleForPixelHeight(&g_font, (float)size);
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;
    while (*cursor) {
        int codepoint = (int)read_codepoint(&cursor), advance;
        stbtt_GetCodepointHMetrics(&g_font, codepoint, &advance, NULL);
        width += (int)(advance * scale);
    }
    return width;
}

/* Rasterize into a fixed scratch buffer and alpha blend with clipping on all four edges. */
void draw_text(Image *target, int x, int y, int size, uint32_t rgb, const char *text)
{
    if (!g_fontData || !target || !target->pixels || !text || size < 8 || size > 72) { return; }
    float scale = stbtt_ScaleForPixelHeight(&g_font, (float)size);
    int ascent;
    stbtt_GetFontVMetrics(&g_font, &ascent, NULL, NULL);
    int baseline = y + (int)(ascent * scale);
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor && x < target->width) {
        int codepoint = (int)read_codepoint(&cursor), x0, y0, x1, y1, advance;
        stbtt_GetCodepointBitmapBox(&g_font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
        int width = x1 - x0, height = y1 - y0;
        uint8_t raster[96 * 96];
        if (width > 0 && height > 0 && width <= 96 && height <= 96) {
            stbtt_MakeCodepointBitmap(&g_font, raster, width, height, width, scale, scale, codepoint);
            for (int j = 0; j < height; ++j) {
                for (int i = 0; i < width; ++i) {
                    int px = x + x0 + i, py = baseline + y0 + j;
                    if (px < 0 || py < 0 || px >= target->width || py >= target->height) { continue; }
                    unsigned int alpha = raster[j * width + i];
                    uint32_t bg = target->pixels[py * target->width + px], value = 0;
                    for (unsigned int shift = 0; shift <= 16; shift += 8) {
                        unsigned int channel = (((rgb >> shift) & 255) * alpha +
                                                ((bg >> shift) & 255) * (255 - alpha)) / 255;
                        value |= channel << shift;
                    }
                    target->pixels[py * target->width + px] = value;
                }
            }
        }
        stbtt_GetCodepointHMetrics(&g_font, codepoint, &advance, NULL);
        x += (int)(advance * scale);
    }
}
