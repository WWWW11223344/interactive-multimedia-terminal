/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   touch_sweep.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 竖向扫描诊断页面实现。把屏幕按每 20 像素分为一条带，统计每条带收到的
             采样数及其原始值范围，用条形长度表示采样数、颜色表示原始值跨度。空带
             表示面板不上报，原始值只有单个数的带表示整条带被钉死，两者是不同故障。
             还统计手指持续移动而 rawY 不变的最长连续次数，定位卡死起点。
Others:      所有结论都来自实际采样，本页只写屏幕，不改标定文件、不改页面状态、
             不落笔作画。每次进入新带与每次抬手都向串口输出一行，便于长距离拖动时
             从日志回溯。
Function List:
  1. band_of           把屏幕纵坐标换算成带下标并裁剪到合法范围
  2. empty_runs        找出连续没有任何采样的带区间
  3. touch_sweep_event 按带统计采样、原始值范围与卡死长度，并在串口输出诊断
  4. draw_band_row     绘制单条带的背景、刻度标签与采样数条形
  5. touch_sweep_draw  重绘整帧：标尺、条形图与右侧诊断读数
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 竖向扫描诊断，2026-11-20。存在理由见 inc/touchsweep.h。 */
#include "touchsweep.h"
#include <stdio.h>

/* 扫描带数 */
#define SWEEP_BANDS 24
/* 每带高度，20 像素 */
#define SWEEP_BAND_H (SCREEN_HEIGHT / SWEEP_BANDS)
/* 左侧标签宽度 */
#define SWEEP_LABEL_W 76
/* 条形起点横坐标 */
#define SWEEP_BAR_X 76
/* 条形最大长度，终点在 x = 532 */
#define SWEEP_BAR_MAX 456
/* 右侧诊断面板起点横坐标 */
#define SWEEP_PANEL_X 548

/* 一条带的统计：采样数、原始 Y 范围、原始 X 范围 */
typedef struct {
    int samples;
    int minRawY, maxRawY;
    int minRawX, maxRawX;
} SweepBand;

static SweepBand g_band[SWEEP_BANDS];
static int g_visited;              /* 至少收到过一个采样的带数 */
static bool g_haveMap;
static int g_mapX, g_mapY;
static int g_lastRawX, g_lastRawY, g_haveRaw;
static int g_stuckRun;             /* 连续几次采样 rawY 没有变化 */
static int g_stuckPeak;            /* 启动以来最长的连续次数 */
static int g_stuckBand;            /* 最长连续段起始所在的带 */
static int g_peakCount;

/* 把屏幕纵坐标换算成带下标，并裁剪到 0..SWEEP_BANDS-1。 */
static int band_of(int y)
{
    int band = y / SWEEP_BAND_H;
    if (band < 0) { band = 0; }
    if (band >= SWEEP_BANDS) { band = SWEEP_BANDS - 1; }
    return band;
}

/* 找出连续没有任何采样的带区间。连续区间本身就是结论：要么是手指跳过了那一段，
   要么是面板不上报那一段，只有拿着手指的人能排除前一种可能。 */
static int empty_runs(int *start, int *end, int capacity)
{
    int runs = 0, band = 0;
    while (band < SWEEP_BANDS && runs < capacity) {
        if (g_band[band].samples) { ++band; continue; }
        int first = band;
        while (band < SWEEP_BANDS && !g_band[band].samples) { ++band; }
        start[runs] = first * SWEEP_BAND_H;
        end[runs] = band * SWEEP_BAND_H - 1;
        ++runs;
    }
    return runs;
}

/* 扫描期间的事件处理：按带累计采样与原始值范围，并记录 rawY 卡死的长度。 */
void touch_sweep_event(App *app, Event event)
{
    (void)app;
    if (event.type == EVENT_NONE || event.type == EVENT_QUIT) { return; }
    int rawX = 0, rawY = 0;
    app_last_raw(&rawX, &rawY);
    g_mapX = event.x; g_mapY = event.y; g_haveMap = true;

    /* 手指还在动而原始值停住，这本身就是诊断结论，所以在这里计数而不是靠肉眼去找。
       连续段在一次手势开始时清零而不是结束时：数字要在抬手那一刻才有意义，因为
       日志正是那时写出的。 */
    if (event.type == EVENT_DOWN) {
        g_stuckRun = 0;
    } else if (g_haveRaw && rawY == g_lastRawY && event.type == EVENT_MOVE) {
        if (g_stuckRun == 0) { g_stuckBand = band_of(event.y) * SWEEP_BAND_H; }
        ++g_stuckRun;
        if (g_stuckRun > g_stuckPeak) { g_stuckPeak = g_stuckRun; }
    } else if (event.type == EVENT_MOVE) {
        g_stuckRun = 0;
    }
    g_lastRawX = rawX; g_lastRawY = rawY; g_haveRaw = 1;

    int band = band_of(event.y);
    SweepBand *slot = &g_band[band];
    if (!slot->samples) {
        slot->minRawY = slot->maxRawY = rawY;
        slot->minRawX = slot->maxRawX = rawX;
        ++g_visited;
        /* 每进入一条带写一行，这样即使一次长拖不抬手也能留下可用日志。 */
        fprintf(stderr,"sweep enter y=%d raw=%d,%d\n",band * SWEEP_BAND_H,rawX,rawY);
    }
    if (rawY < slot->minRawY) { slot->minRawY = rawY; }
    if (rawY > slot->maxRawY) { slot->maxRawY = rawY; }
    if (rawX < slot->minRawX) { slot->minRawX = rawX; }
    if (rawX > slot->maxRawX) { slot->maxRawX = rawX; }
    ++slot->samples;
    if (slot->samples > g_peakCount) { g_peakCount = slot->samples; }

    if (g_visited == SWEEP_BANDS) {
        static bool announced;
        if (!announced) {
            announced = true;
            fprintf(stderr,"sweep: all %d bands seen, lift and read the table\n",SWEEP_BANDS);
        }
    }
    if (event.type == EVENT_UP) {
        fprintf(stderr,"sweep band y=%d n=%d rawY=%d..%d rawX=%d..%d stuck=%d at y=%d\n",
            band * SWEEP_BAND_H,slot->samples,slot->minRawY,slot->maxRawY,
            slot->minRawX,slot->maxRawX,g_stuckPeak,g_stuckBand);
        int start[SWEEP_BANDS], end[SWEEP_BANDS];
        int runs = empty_runs(start,end,SWEEP_BANDS);
        fprintf(stderr,"sweep empty:");
        for (int i = 0; i < runs; ++i) { fprintf(stderr," y=%d..%d",start[i],end[i]); }
        fprintf(stderr,"%s\n",runs ? "" : " none");
        fprintf(stderr,"sweep map:");
        for (int i = 0; i < SWEEP_BANDS; ++i) { fprintf(stderr," %d",g_band[i].samples); }
        fprintf(stderr,"\n");
    }
}

/* 绘制单条带：背景交替便于分辨，每 5 带打一个刻度标签；有采样时画条形。 */
static void draw_band_row(Image *screen, int band)
{
    int top = band * SWEEP_BAND_H;
    const SweepBand *slot = &g_band[band];
    draw_rect(screen,(Rect){0,top,SCREEN_WIDTH,SWEEP_BAND_H},band % 2 ? 0x161C24u : 0x10151Cu);
    draw_rect(screen,(Rect){0,top,SCREEN_WIDTH,1},0x222B36u);
    if (band % 5 == 0) {
        char label[16];
        snprintf(label,sizeof(label),"y %d",top);
        draw_text(screen,6,top + 3,15,0x7C8B9Au,label);
    }
    if (!slot->samples) { return; }
    /* 条形长度表示采样数，颜色表示原始值跨度。只回一个原始值的带无论采样多少
       都画成红色。 */
    int width = 10 + (int)((long)slot->samples * (SWEEP_BAR_MAX - 10) / (g_peakCount ? g_peakCount : 1));
    uint32_t colour = (slot->maxRawY - slot->minRawY <= 2) ? 0xC0392Bu : 0x20A08Fu;
    draw_rect(screen,(Rect){SWEEP_BAR_X,top + 4,width,SWEEP_BAND_H - 9},colour);
}

/* 重绘整帧：逐带条形、标尺边线、当前位置线，以及右侧的诊断读数与图例。 */
void touch_sweep_draw(App *app)
{
    Image *screen = &app->screen;
    char line[80];
    for (int band = 0; band < SWEEP_BANDS; ++band) { draw_band_row(screen,band); }

    /* 标尺边线与条形起点，使长度为零的条形读作零。 */
    draw_rect(screen,(Rect){SWEEP_BAR_X - 1,0,1,SCREEN_HEIGHT},0x39465Au);
    draw_rect(screen,(Rect){SWEEP_PANEL_X - 6,0,1,SCREEN_HEIGHT},0x39465Au);

    if (g_haveMap) {
        draw_rect(screen,(Rect){0,g_mapY,SWEEP_BAR_X - 6,2},0xFFC24Au);
        draw_rect(screen,(Rect){SWEEP_PANEL_X - 5,g_mapY,7,2},0xFFC24Au);
    }

    draw_text(screen,SWEEP_PANEL_X,6,19,0xE8F1F8u,"竖向扫描");
    draw_rect(screen,(Rect){SWEEP_PANEL_X,30,246,1},0x39465Au);
    if (g_haveMap) {
        snprintf(line,sizeof(line),"raw  %d , %d",g_lastRawX,g_lastRawY);
        draw_text(screen,SWEEP_PANEL_X,38,18,0xFFFFFFu,line);
        snprintf(line,sizeof(line),"map  %d , %d",g_mapX,g_mapY);
        draw_text(screen,SWEEP_PANEL_X,62,18,0x20C0B0u,line);
        snprintf(line,sizeof(line),"第 %d 带  样本 %d",g_mapY / SWEEP_BAND_H,
            g_band[band_of(g_mapY)].samples);
        draw_text(screen,SWEEP_PANEL_X,88,16,0x9FB0C0u,line);
        int slot = band_of(g_mapY);
        snprintf(line,sizeof(line),"本带 rawY %d..%d",g_band[slot].minRawY,g_band[slot].maxRawY);
        draw_text(screen,SWEEP_PANEL_X,110,16,
            (g_band[slot].maxRawY - g_band[slot].minRawY <= 2) ? 0xE08070u : 0x9FB0C0u,line);
    } else {
        draw_text(screen,SWEEP_PANEL_X,38,18,0x9FB0C0u,"raw  - , -");
    }

    int start[SWEEP_BANDS], end[SWEEP_BANDS];
    int runs = empty_runs(start,end,SWEEP_BANDS);
    draw_text(screen,SWEEP_PANEL_X,140,15,0x9FB0C0u,"没收到样本的带");
    if (!runs) {
        draw_text(screen,SWEEP_PANEL_X,160,17,0x20C0B0u,"无（每带都有）");
    }
    for (int i = 0; i < runs && i < 5; ++i) {
        snprintf(line,sizeof(line),"y %d .. %d",start[i],end[i]);
        draw_text(screen,SWEEP_PANEL_X + 6,160 + i * 20,16,0xE08070u,line);
    }
    if (runs > 5) {
        snprintf(line,sizeof(line),"共 %d 段",runs);
        draw_text(screen,SWEEP_PANEL_X + 6,160 + 5 * 20,16,0xE08070u,line);
    }

    draw_text(screen,SWEEP_PANEL_X,300,15,0x9FB0C0u,"原地不动最长");
    if (g_stuckPeak >= 3) {
        snprintf(line,sizeof(line),"y %d 起 %d 次",g_stuckBand,g_stuckPeak);
        draw_text(screen,SWEEP_PANEL_X,320,17,0xFFC24Au,line);
    } else {
        draw_text(screen,SWEEP_PANEL_X,320,16,0x66748Au,"未出现");
    }
    /* 图例放在这里而不是最后一条条形下面：最下面那条带会把图例盖住。 */
    draw_text(screen,SWEEP_PANEL_X,348,14,0xE08070u,"红 = 该带只有一个 rawY");
    draw_text(screen,SWEEP_PANEL_X,366,14,0x20A08Fu,"绿 = 该带正常");

    draw_rect(screen,(Rect){SWEEP_PANEL_X,384,246,1},0x39465Au);
    draw_text(screen,SWEEP_PANEL_X,392,15,0xE8F1F8u,"从上沿慢慢拖到下沿");
    draw_text(screen,SWEEP_PANEL_X,412,15,0xE8F1F8u,"再反着拖一次");
    draw_text(screen,SWEEP_PANEL_X,440,14,0x7C8B9Au,"Ctrl+C 退出");
}
