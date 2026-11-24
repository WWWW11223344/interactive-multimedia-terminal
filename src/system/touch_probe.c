/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   touch_probe.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 触摸自检页面实现。把指针背后每个采样的面板原始值与映射后像素值并排
             显示，并记录启动以来两根轴见到的原始值范围。读数放在屏幕底部，被怀疑
             的屏幕上方留给标尺和随手指移动的青线，便于一次滑动就看出量程是否到底。
Others:      若青线滑到某处不再下移，对应的 rawY 就是面板下限；只显示当前值无法发现
             这一点，因此必须持续跟踪极值。每次抬手还向串口打印一行，便于无人截图时
             从日志回溯。
Function List:
  1. touch_probe_event  记录每次采样的原始值与映射值，持续跟踪两轴极值
  2. touch_probe_draw   重绘整帧：分段标尺、青色位置线、原始值与映射值大字读数
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 触摸自检页面，Codex 协助，2026-11-20。
 * 目的：单看像素坐标无法处理「屏幕上方没反应」的报告，因为量程错误与面板停止
 * 上报是两种不同故障，换算之后看起来却一样。本页显示每个采样携带的原始值，
 * 滑动一次即可区分。
 */
#include "touchprobe.h"
#include <stdio.h>

/* 读数放在屏幕底部是有意为之：被怀疑的正是屏幕上方，那一带要留给标尺和移动线。 */
#define PROBE_PLATE_TOP 268
/* 底部读数板的宽度 */
#define PROBE_PLATE_W 496
/* 右侧重复显示位置的竖槽横坐标 */
#define PROBE_GUTTER_X 774
/* 标尺分段数 */
#define PROBE_BANDS 6
/* 每段高度 */
#define PROBE_BAND_H (SCREEN_HEIGHT / PROBE_BANDS)

/* 两轴见到的原始值极值 */
static int g_minRawX, g_maxRawX, g_minRawY, g_maxRawY;
/* 是否已有原始值 */
static bool g_haveRaw;
/* 最近一次映射后的屏幕坐标 */
static int g_mapX, g_mapY;
/* 是否已有映射坐标 */
static bool g_haveMap;

/* 跟踪每根轴的极值而不只是当前采样：某个面板从来不上报低于某值的原始值，这本身
   就是结论，单次点按看不出来。 */
void touch_probe_event(App *app, Event event)
{
    int rawX = 0, rawY = 0;
    (void)app;
    if (event.type == EVENT_NONE || event.type == EVENT_QUIT) { return; }
    app_last_raw(&rawX, &rawY);
    if (!g_haveRaw) {
        g_minRawX = g_maxRawX = rawX;
        g_minRawY = g_maxRawY = rawY;
        g_haveRaw = true;
    }
    if (rawX < g_minRawX) { g_minRawX = rawX; }
    if (rawX > g_maxRawX) { g_maxRawX = rawX; }
    if (rawY < g_minRawY) { g_minRawY = rawY; }
    if (rawY > g_maxRawY) { g_maxRawY = rawY; }
    g_mapX = event.x; g_mapY = event.y; g_haveMap = true;
    /* 每次抬手写一行，即使没人拍屏幕，数字也能留在串口日志里。 */
    if (event.type == EVENT_UP) {
        fprintf(stderr,"probe: map=%d,%d raw=%d,%d seen X=%d..%d Y=%d..%d\n",
            event.x,event.y,rawX,rawY,g_minRawX,g_maxRawX,g_minRawY,g_maxRawY);
    }
}

/* 重绘自检整帧：条纹标尺、位置线、原始值与映射值读数，以及原始值范围提示。 */
void touch_probe_draw(App *app)
{
    Image *screen = &app->screen;
    char line[64];
    int band;
    for (band = 0; band < PROBE_BANDS; ++band) {
        int top = band * PROBE_BAND_H;
        draw_rect(screen,(Rect){0,top,SCREEN_WIDTH,PROBE_BAND_H-1},band%2 ? 0x1C232Du : 0x131820u);
        draw_rect(screen,(Rect){0,top+PROBE_BAND_H-1,SCREEN_WIDTH,1},0x2E3846u);
        snprintf(line,sizeof(line),"%d",top);
        draw_text(screen,8,top+3,15,0x63748Au,line);
    }
    /* 采样落点：先画在读数板下面，再在右侧竖槽里重复一次，这样即使落点被面板
       本身挡住也仍然看得见。 */
    if (g_haveMap) {
        draw_rect(screen,(Rect){0,g_mapY-1,SCREEN_WIDTH,3},0x20C0B0u);
    }
    draw_rect(screen,(Rect){0,PROBE_PLATE_TOP,PROBE_PLATE_W,SCREEN_HEIGHT-PROBE_PLATE_TOP},0x080B0Fu);
    draw_rect(screen,(Rect){0,PROBE_PLATE_TOP,PROBE_PLATE_W,1},0x20C0B0u);
    if (g_haveMap) {
        int rawX = 0, rawY = 0;
        draw_rect(screen,(Rect){PROBE_GUTTER_X,g_mapY-1,SCREEN_WIDTH-PROBE_GUTTER_X,3},0x20C0B0u);
        draw_disc(screen,PROBE_GUTTER_X+11,g_mapY,6,0xFFD24Au);
        app_last_raw(&rawX,&rawY);
        snprintf(line,sizeof(line),"raw  X %d  Y %d",rawX,rawY);
        draw_text(screen,14,PROBE_PLATE_TOP+18,30,0xFFFFFFu,line);
        snprintf(line,sizeof(line),"map  X %d  Y %d",g_mapX,g_mapY);
        draw_text(screen,14,PROBE_PLATE_TOP+58,30,0x20C0B0u,line);
    } else {
        draw_text(screen,14,PROBE_PLATE_TOP+18,30,0xFFFFFFu,"raw  X -  Y -");
        draw_text(screen,14,PROBE_PLATE_TOP+58,30,0x20C0B0u,"map  X -  Y -");
    }
    draw_text(screen,14,PROBE_PLATE_TOP+4,15,0x6F7D8Cu,"触摸自检 touch probe");
    if (g_haveRaw) {
        snprintf(line,sizeof(line),"rawY 见过 %d .. %d",g_minRawY,g_maxRawY);
        draw_text(screen,14,PROBE_PLATE_TOP+100,24,0xFFD24Au,line);
        snprintf(line,sizeof(line),"rawX 见过 %d .. %d",g_minRawX,g_maxRawX);
        draw_text(screen,14,PROBE_PLATE_TOP+134,22,0x9FB0C0u,line);
        draw_text(screen,14,PROBE_PLATE_TOP+164,19,0xE05555u,
            "若这条线滑到某处就不再动，那个 rawY 就是面板的下限");
    } else {
        draw_text(screen,14,PROBE_PLATE_TOP+100,22,0x9FB0C0u,"屏幕上按住并滑动即可看到数值");
    }
    draw_text(screen,PROBE_PLATE_W+16,PROBE_PLATE_TOP+6,17,0x9FB0C0u,"从上沿慢慢滑到下沿");
    draw_text(screen,PROBE_PLATE_W+16,PROBE_PLATE_TOP+30,17,0x9FB0C0u,"看青线是否一路下移");
    draw_text(screen,PROBE_PLATE_W+16,PROBE_PLATE_TOP+62,17,0x7C8B9Au,"Ctrl+C 退出");
}
