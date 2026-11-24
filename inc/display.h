/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   display.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 显示层头文件。定义有界软件画布所需的矩形与图像结构，以及图像读写、
             图元绘制和文字绘制接口，供 GEC6818 板端与桌面目标共用。图像自己持有
             像素缓冲，读取失败时返回空图像，且只接受可信的本地 BMP 文件。
Others:      屏幕为 800x480、32 位色深；所有绘制先在内存画布上完成，再由上层统一
             提交，避免逐点直接操作帧缓冲。文字绘制依赖先打开字库。
Function List:
  1. image_load    从本地 BMP 文件加载图像
  2. image_save    把图像保存到文件
  3. image_free    释放图像持有的像素缓冲
  4. image_create  创建指定尺寸的空图像
  5. rect_has      判断点是否落在矩形内
  6. draw_rect     绘制实心矩形
  7. draw_disc     绘制实心圆
  8. draw_line     绘制线段
  9. draw_image    把源图像缩放绘制到指定矩形
 10. text_open     打开字库，供文字绘制使用
 11. text_close    关闭字库
 12. draw_text     在画布上绘制一行文字
 13. text_width    计算 UTF-8 字符串在给定字号下的推进宽度
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#ifndef TERMINAL_DISPLAY_H
#define TERMINAL_DISPLAY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* 屏幕宽度，单位像素 */
#define SCREEN_WIDTH 800
/* 屏幕高度，单位像素 */
#define SCREEN_HEIGHT 480
/* 矩形区域：x、y 为左上角坐标，w、h 为宽高，单位像素 */
typedef struct { int x, y, w, h; } Rect;
/* 图像缓冲：width、height 为像素尺寸，pixels 指向由图像自己持有的像素数组 */
typedef struct { int width, height; uint32_t *pixels; } Image;
/* 图像自己持有像素；读取失败留下空图像；只接受可信的本地 BMP 文件 */
bool image_load(const char *path, Image *out);
/* 把图像保存到指定路径 */
bool image_save(const char *path, const Image *image);
/* 释放图像持有的像素缓冲 */
void image_free(Image *image);
/* 创建指定尺寸的空图像 */
bool image_create(Image *image, int width, int height);
/* 判断点 (x, y) 是否落在矩形内 */
bool rect_has(Rect rect, int x, int y);
/* 以指定颜色绘制实心矩形 */
void draw_rect(Image *target, Rect rect, uint32_t rgb);
/* 以指定颜色绘制实心圆 */
void draw_disc(Image *target, int x, int y, int radius, uint32_t rgb);
/* 以指定颜色绘制线段，radius 为线宽 */
void draw_line(Image *target, int x0, int y0, int x1, int y1, int radius, uint32_t rgb);
/* 把源图像绘制到目标矩形，fit 决定是否按比例缩放适配 */
void draw_image(Image *target, Rect rect, const Image *source, bool fit);
/* 打开字库；绘制文字前必须先调用 */
bool text_open(const char *path);
/* 关闭字库，释放相关资源 */
void text_close(void);
/* 在指定位置以指定字号和颜色绘制一行文字 */
void draw_text(Image *target, int x, int y, int size, uint32_t rgb, const char *text);
/* 计算 UTF-8 字符串在给定字号下的推进宽度，方便调用方一次测量即可居中；
   字库未打开时返回 0。 */
int text_width(const char *text, int size);
#endif
