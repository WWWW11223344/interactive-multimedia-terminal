/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   print.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 文字绘制适配层。封装第三方 stb_truetype 光栅化器，负责一次性载入随
             程序打包的可信 TTF 字体、有界 UTF-8 解码、文本宽度计算以及带裁剪的
             字形 alpha 混合绘制。供所有页面显示中文与英文标签。
Others:      字形光栅化到固定 96x96 临时缓冲后再混合，宽高超限的字形直接跳过；
             字体缓冲由 stb_truetype 借用，必须在界面停止使用后才能释放。
Function List:
  1. text_open       一次性载入字体文件并初始化光栅化器
  2. text_close      释放字体缓冲
  3. read_codepoint  有界 UTF-8 解码，非法字节变成问号
  4. text_width      计算文本在给定字号下的像素宽度
  5. draw_text       把文本光栅化并裁剪混合到目标图像
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 由谢氏 print/font 模块改编而来。Codex，2026-11-19：限制字形渲染的缓冲范围。 */
#include "display.h"
#include "truetype.h"
#include <stdio.h>
#include <stdlib.h>
static uint8_t *g_fontData;
static stbtt_fontinfo g_font;

/* 只载入一次随程序打包的可信 TTF；上游光栅化器借用这块缓冲。 */
bool text_open(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) { return false; }
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return false; }
    long size = ftell(file);
    /* 字体文件限制在 12 字节到 32MiB 之间，过小的文件没有意义，过大则拒绝分配。 */
    if (size < 12 || size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file); return false;
    }
    g_fontData = malloc((size_t)size);
    bool ok = g_fontData && fread(g_fontData, 1, (size_t)size, file) == (size_t)size;
    fclose(file);
    if (!ok || !stbtt_InitFont(&g_font, g_fontData, 0)) { text_close(); return false; }
    return true;
}

/* 只有在界面停止使用借用的字体字节之后才能关闭。 */
void text_close(void)
{
    free(g_fontData); g_fontData = NULL;
}

/* 有界 UTF-8 解码；畸形字节统一变成问号，避免越界读取。 */
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

/* 前进宽度按 draw_text 移动光标的方式累加，这样居中标签才会落在光栅化器实际
   使用的那个像素上。 */
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

/* 光栅化到固定临时缓冲，再与四边裁剪后的目标图像做 alpha 混合。 */
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
