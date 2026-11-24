/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   calibrate.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 九点触摸标定向导，按需通过 --calibrate 运行。登录数字键盘第一排的点按
             会落到下一排，而「量程偏移」与「面板响应弯曲」从外部看现象完全一样，
             因此改为实测面板：在 3x3 网格上画九个十字，记录每根手指下的原始值，
             用实测锚点替换驱动声明量程，每根轴的映射改为两段直线。实测单条直线
             在本板中央偏差达 23 像素，约半个按键高度，一条直线远远不够。
Others:      向导从不做命中判定。点按落在哪里都照单全收，因为落点本身正是要测量的
             对象。标定时长上限 60 秒，超时则沿用驱动声明量程继续运行。
Function List:
  1. cal_centre_text   在屏幕水平居中的位置绘制文字
  2. cal_target        绘制靶心十字，供用户点按
  3. cal_pips          在屏幕顶部画细线式进度条
  4. cal_samples       把已采样的原始值按九宫格位置显示出来
  5. cal_frame         绘制单帧标定界面
  6. cal_report        标定成功后显示结果报告
  7. cal_failure       标定失败时显示诊断信息
  8. cal_wait_release  等待一次抬手事件并取回原始值
  9. touch_calibrate   标定流程主入口，串行完成九点采样、解算与保存
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 九点触摸标定向导，按需通过 --calibrate 运行。
 *
 * 为什么需要它：点登录键盘第一排却落到了下一排。「量程整体偏移」与「面板响应
 * 弯曲」从外部看现象完全一样，所以不去猜，而是实测面板。在 3x3 网格上画九个
 * 十字，记录每根手指下的原始值，用实测锚点替换声明量程，并把映射改成每根轴
 * 两段直线。实测单条直线在本板中央偏差 23 像素，是半个按键高度，一条不够。
 *
 * 向导绝不做命中判定。点按落到哪里都算，因为那个落点正是被测对象本身。
 */
#include "platform.h"
#include <stdio.h>

/* 标定轮询周期，200 毫秒 */
#define CAL_TICK_MS 200
/* 50 个 tick 即 10 秒：提示用户如何跳过标定 */
#define CAL_HINT_TICKS 50
/* 300 个 tick 即 60 秒无触摸：放弃标定，沿用声明量程 */
#define CAL_GIVE_UP_TICKS 300

/* 九个靶点对应的中文提示语，顺序与九宫格采样顺序一致 */
static const char *const g_prompt[TOUCH_CAL_POINTS] = {
    "请点击左上角的十字中心", "请点击上排中间的十字中心", "请点击右上角的十字中心",
    "请点击左侧中间的十字中心", "请点击正中间的十字中心", "请点击右侧中间的十字中心",
    "请点击左下角的十字中心", "请点击下排中间的十字中心", "请点击右下角的十字中心"
};

/* 在屏幕水平居中位置绘制一行文字 */
static void cal_centre_text(App *app, int y, int size, uint32_t colour, const char *text)
{
    int width = text_width(text, size);
    draw_text(&app->screen, (SCREEN_WIDTH - width) / 2, y, size, colour, text);
}

/* 靶心加十字线：即便映射点偏到别处，24 像素的白色圆盘仍然容易点中，这正是画
   固定靶的意义。准星用青色，因为米色底上的白线在日光下几乎看不见。 */
static void cal_target(App *app, int x, int y)
{
    draw_line(&app->screen, x - 40, y, x + 40, y, 2, UI_TEAL);
    draw_line(&app->screen, x, y - 40, x, y + 40, 2, UI_TEAL);
    draw_disc(&app->screen, x, y, 24, 0xFFFFFF);
    draw_disc(&app->screen, x, y, 18, UI_TEAL);
    draw_disc(&app->screen, x, y, 5, 0xFFFFFF);
}

/* 进度画成顶部一条细线：九个刻度会和最上一排靶点打架，而且步骤计数已经用文字
   说明了当前进度。 */
static void cal_pips(App *app, int done)
{
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, 3}, 0xD8D2C4);
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH * done / TOUCH_CAL_POINTS, 3}, UI_TEAL);
}

/* 把已记录的采样按九宫格位置显示出来，点错十字时会看到数字落在邻居旁边而不是
   排在其后，便于目视核对。 */
static void cal_samples(App *app, const int rawX[TOUCH_CAL_POINTS],
                        const int rawY[TOUCH_CAL_POINTS], int done)
{
    char line[32];
    for (int i = 0; i < done && i < TOUCH_CAL_POINTS; ++i) {
        snprintf(line, sizeof(line), "%d) %d,%d", i + 1, rawX[i], rawY[i]);
        draw_text(&app->screen, 150 + (i % 3) * 190, 148 + (i / 3) * 15, 13, UI_MUTED, line);
    }
}

/* 文字只放在上排靶点与中排靶点之间的空带里，保证不会盖住用户还要去点的准星。 */
static void cal_frame(App *app, int step, const int rawX[TOUCH_CAL_POINTS],
                      const int rawY[TOUCH_CAL_POINTS], const char *hint)
{
    char line[64];
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, UI_BG);
    cal_pips(app, step);
    cal_centre_text(app, 6, 24, UI_INK, "触摸屏标定");
    snprintf(line, sizeof(line), "第 %d / %d 点", step + 1, TOUCH_CAL_POINTS);
    draw_text(&app->screen, 664, 10, 15, UI_MUTED, line);
    cal_centre_text(app, 124, 18, UI_INK, g_prompt[step]);
    cal_samples(app, rawX, rawY, step);
    if (hint) {
        cal_centre_text(app, 196, 13, 0xAD4E3D, hint);
    }
    cal_target(app, TOUCH_CAL_TARGET[step][0], TOUCH_CAL_TARGET[step][1]);
}

/* 标定成功后显示实测锚点、外推出的屏幕边缘、轴向以及最大偏差，并说明记录是否
   已写入文件。 */
static void cal_report(App *app, const TouchCal *cal, int error, bool saved)
{
    char line[80];
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    touch_cal_edges(cal->rawX, TOUCH_CAL_AXIS_X, SCREEN_WIDTH, &x0, &x1);
    touch_cal_edges(cal->rawY, TOUCH_CAL_AXIS_Y, SCREEN_HEIGHT, &y0, &y1);
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, UI_BG);
    cal_centre_text(app, 56, 32, UI_TEAL, "标定完成");
    snprintf(line, sizeof(line), "X 实测 %d / %d / %d",
        cal->rawX[0], cal->rawX[1], cal->rawX[2]);
    cal_centre_text(app, 124, 18, UI_INK, line);
    snprintf(line, sizeof(line), "Y 实测 %d / %d / %d",
        cal->rawY[0], cal->rawY[1], cal->rawY[2]);
    cal_centre_text(app, 152, 18, UI_INK, line);
    snprintf(line, sizeof(line), "推出屏幕边缘  X %d..%d   Y %d..%d", x0, x1, y0, y1);
    cal_centre_text(app, 190, 17, UI_MUTED, line);
    snprintf(line, sizeof(line), "轴向 %s，最大偏差 %d 像素",
        cal->swap ? "交换（原始 Y 为横轴）" : "正常", error);
    cal_centre_text(app, 222, 17, UI_INK, line);
    cal_centre_text(app, 282, 15, saved ? UI_MUTED : 0xAD4E3D,
        saved ? "已写入 data/touch.cal，正在进入登录页"
              : "未能写入 data/touch.cal，本次运行仍然生效");
}

/* 标定失败页：区分「面板几乎没有变化」与「残差超限」，并列出九组原始值供排查。 */
static void cal_failure(App *app, const int rawX[TOUCH_CAL_POINTS],
                        const int rawY[TOUCH_CAL_POINTS], int error)
{
    char line[64];
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, UI_BG);
    cal_centre_text(app, 56, 30, 0xAD4E3D, "标定失败");
    if (error < 0) {
        /* 这里故意用字符串常量：这句中文按 UTF-8 编码后放不进 64 字节的临时缓冲，
           被截断的中文读出来就是乱码。 */
        cal_centre_text(app, 104, 17, UI_INK, "某一列或某一行几乎没有变化，请按十字中心重新点");
    } else {
        snprintf(line, sizeof(line), "最大偏差 %d 像素，超出 %d 像素上限",
            error, TOUCH_CAL_TOLERANCE);
        cal_centre_text(app, 104, 17, UI_INK, line);
    }
    cal_centre_text(app, 132, 14, UI_MUTED, "下面九组是按顺序记到的原始值，可对照十字位置排查");
    for (int i = 0; i < TOUCH_CAL_POINTS; ++i) {
        snprintf(line, sizeof(line), "%d) %d,%d", i + 1, rawX[i], rawY[i]);
        draw_text(&app->screen, 178 + (i % 3) * 170, 168 + (i / 3) * 22, 14, UI_MUTED, line);
    }
    cal_centre_text(app, 262, 18, UI_INK, "轻点屏幕任意位置重新标定");
    cal_centre_text(app, 292, 13, UI_MUTED, "退出后直接运行 ./run.sh 可跳过本页，使用驱动声明范围");
}

/* 等待一次抬手并把原始值取回。原始值与事件在同一次轮询里紧接着读取，趁下一条
   上报还没覆盖它，这样采样测的是面板本身而不是映射结果。设备失败返回 -1。 */
static int cal_wait_release(int *rawX, int *rawY)
{
    for (;;) {
        Event event;
        int ready = platform_poll(&event, CAL_TICK_MS);
        if (ready < 0) { return -1; }
        if (ready > 0 && event.type == EVENT_UP) {
            app_last_raw(rawX, rawY);
            return 1;
        }
    }
}

/* 标定主流程：循环取九点采样，解算并校验标定记录，应用、保存并显示结果；
   失败则显示诊断并等待重新标定。设备丢失时直接返回。 */
void touch_calibrate(App *app)
{
    int rawX[TOUCH_CAL_POINTS] = {0}, rawY[TOUCH_CAL_POINTS] = {0};
    if (!app) { return; }
    for (;;) {
        int step = 0, idle = 0;
        const char *hint = NULL;
        TouchCal cal;
        int error = 0;
        while (step < TOUCH_CAL_POINTS) {
            Event event;
            cal_frame(app, step, rawX, rawY, hint);
            platform_present(&app->screen);
            int ready = platform_poll(&event, CAL_TICK_MS);
            if (ready < 0) { fprintf(stderr, "calibration abandoned: touch device lost\n"); return; }
            if (ready > 0 && event.type == EVENT_UP) {
                app_last_raw(&rawX[step], &rawY[step]);
                fprintf(stderr, "calibration point %d = raw %d,%d\n", step + 1, rawX[step], rawY[step]);
                ++step; idle = 0; hint = NULL;
                continue;
            }
            /* 10 秒无触摸给出跳过提示，超过 60 秒则放弃标定 */
            if (++idle == CAL_HINT_TICKS) {
                hint = "没有收到触摸？退出后直接 ./run.sh 可跳过标定";
            } else if (idle > CAL_GIVE_UP_TICKS) {
                fprintf(stderr, "calibration skipped: no touch for %d s\n", CAL_GIVE_UP_TICKS / 5);
                return;
            }
        }
        if (!touch_cal_solve(rawX, rawY, &cal, &error)) {
            int anyX = 0, anyY = 0;
            cal_failure(app, rawX, rawY, error);
            platform_present(&app->screen);
            fprintf(stderr, "calibration rejected, worst miss %d px\n", error);
            if (cal_wait_release(&anyX, &anyY) < 0) { return; }
            continue;
        }
        /* 解算通过：先让映射生效，再尝试写入文件；写失败只提示，本次仍生效 */
        platform_cal_apply(&cal);
        bool saved = platform_cal_save(&cal);
        if (!saved) {
            fprintf(stderr, "calibration not saved: cannot write data/touch.cal\n");
        }
        {
            int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
            touch_cal_edges(cal.rawX, TOUCH_CAL_AXIS_X, SCREEN_WIDTH, &x0, &x1);
            touch_cal_edges(cal.rawY, TOUCH_CAL_AXIS_Y, SCREEN_HEIGHT, &y0, &y1);
            fprintf(stderr, "calibration applied: swap=%d X=%d/%d/%d Y=%d/%d/%d "
                            "edges X=%d..%d Y=%d..%d worst=%d\n",
                cal.swap, cal.rawX[0], cal.rawX[1], cal.rawX[2],
                cal.rawY[0], cal.rawY[1], cal.rawY[2], x0, x1, y0, y1, error);
        }
        cal_report(app, &cal, error, saved);
        platform_present(&app->screen);
        /* 把结果停留足够久让人读完，再交回应用 */
        for (int i = 0; i < 12; ++i) {
            Event event;
            if (platform_poll(&event, CAL_TICK_MS) < 0) { break; }
        }
        return;
    }
}
