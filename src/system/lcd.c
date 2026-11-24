/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   lcd.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 帧缓冲绘图模块。提供矩形命中判定、裁剪填充、圆盘、直线与图像贴图的
             纯软件实现，全部在内存中的 Image 缓冲上完成，再由平台层提交到
             /dev/fb0。所有写操作都先裁剪到目标范围内，保证边缘安全。
Others:      draw_disc 的半径上限是 32 且超限静默返回：不返回值、不打日志。曾经把
             井字棋棋子半径设为 34，结果电脑的 O 全部消失，而同一屏上半径 16 的图例
             圆环却正常，现象就是「只看见自己的叉」。选尺寸前务必确认不超过此上限，
             不要为某一个图形去抬高它。
Function List:
  1. rect_has    用半开区间判断点是否落在矩形内
  2. draw_rect   裁剪后填充矩形
  3. draw_disc   画实心圆盘，半径必须在 1..32 之间
  4. draw_line   用 Bresenham 算法画连线，连接稀疏的触摸采样点
  5. draw_image  按比例或原尺寸贴图，支持裁剪与快速路径
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 谢东宏的帧缓冲绘图代码，由 Codex 于 2026-11-19 改写：增加裁剪并修正缩放。 */
#include "display.h"
#include <stdlib.h>
#include <string.h>

/* 命中判定用半开区间，相邻按钮才不会重叠。 */
bool rect_has(Rect rect, int x, int y)
{
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

/* 每次写入前先裁剪；第 0 行与第 0 列都是合法像素。首行逐像素填充、其余行整行
   拷贝，这点很关键，因为每屏开始都会先做一次 800x480 的整屏清屏。 */
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

/* 有界的小笔刷也能在画布边缘作画：每行只写一段水平跨度，而不是逐像素各裁剪一个
   矩形。
   半径上限 32 是刻意设定的：draw_line 每走一个像素就调用本函数一次，调用方若
   不小心给出巨大半径会拖慢整帧。但丢弃半径是静默的，没有返回值也没有日志 ——
   这正是半径 34 的井字棋棋子消失数周的原因，而同屏半径 16 的图例圆环看起来却
   好好的。选图形尺寸前先核对这个上限，不要为迁就某个图形而抬高它。 */
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

/* Bresenham 算法把稀疏的触摸采样点连成不断线的笔画。 */
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

/* 先乘后除；fit 模式保持宽高比，横图竖图都适用。1:1 的情况是整行拷贝：画布与
   2048 数字牌每帧都要贴图，这里逐像素做 64 位除法曾经比一帧其余所有工作加起来
   还费时。 */
void draw_image(Image *target, Rect rect, const Image *source, bool fit)
{
    if (!source || !source->pixels || rect.w <= 0 || rect.h <= 0) { return; }
    /* fit 模式按源图宽高比缩放并在目标矩形内居中 */
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
    /* 缩放路径：800 像素宽度用 32 位运算足够，省掉昂贵的 64 位除法。 */
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
