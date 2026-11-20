/* Five-tap touch calibration wizard, run once on a panel that has no record yet.
 *
 * Why it exists: the 1/2/3 key row of the login pad could not be pressed while 4..9
 * responded, and the two rows are only 68 px apart - a key rectangle cannot be half a
 * screen away from its own pixels. That makes the reported coordinates, not the
 * layout, the suspect: the driver on this panel declares one ABS range and reports
 * another, so one band of the screen maps onto a place nobody tapped. Guessing a bias
 * from the outside is hopeless, so the panel is measured instead: five crosses are
 * drawn, the raw values under each finger are recorded, and the measured raw range
 * replaces the declared one for the rest of the run.
 *
 * The wizard never hit-tests a tap. A tap is taken wherever it arrives, because the
 * arrival point is exactly what is being measured.
 */
#include "platform.h"
#include <stdio.h>

#define CAL_TICK_MS 200
#define CAL_HINT_TICKS 50      /* 10 s: tell the user how to skip */
#define CAL_GIVE_UP_TICKS 150  /* 30 s: carry on with the declared range */

static const char *const g_prompt[TOUCH_CAL_POINTS] = {
    "请点击左侧上方的十字中心",
    "请点击右侧上方的十字中心",
    "请点击左侧下方的十字中心",
    "请点击右侧下方的十字中心",
    "请点击中间偏下的十字中心"
};

static void cal_centre_text(App *app, int y, int size, uint32_t colour, const char *text)
{
    int width = text_width(text, size);
    draw_text(&app->screen, (SCREEN_WIDTH - width) / 2, y, size, colour, text);
}

/* Bullseye plus crosshair: a 26 px ring is easy to hit even when the mapped point is
   somewhere else entirely, which is the whole point of drawing a fixed target. The
   arms are teal because white on the beige page is invisible in daylight. */
static void cal_target(App *app, int x, int y)
{
    draw_line(&app->screen, x - 46, y, x + 46, y, 2, UI_TEAL);
    draw_line(&app->screen, x, y - 46, x, y + 46, 2, UI_TEAL);
    draw_disc(&app->screen, x, y, 26, 0xFFFFFF);
    draw_disc(&app->screen, x, y, 20, UI_TEAL);
    draw_disc(&app->screen, x, y, 6, 0xFFFFFF);
}

/* Progress pips: filled once a point has been captured, so a mis-tap sequence is
   visible before the fit is attempted. */
static void cal_pips(App *app, int done)
{
    int left = (SCREEN_WIDTH - (TOUCH_CAL_POINTS * 26 - 8)) / 2;
    for (int i = 0; i < TOUCH_CAL_POINTS; ++i) {
        draw_rect(&app->screen, (Rect){left + i * 26, 78, 18, 8},
            i < done ? UI_TEAL : 0xD8D2C4);
    }
}

static void cal_samples(App *app, const int rawX[TOUCH_CAL_POINTS],
                        const int rawY[TOUCH_CAL_POINTS], int done, int y)
{
    char line[48];
    int x = 40;
    for (int i = 0; i < done; ++i) {
        snprintf(line, sizeof(line), "%d) raw %d,%d   ", i + 1, rawX[i], rawY[i]);
        draw_text(&app->screen, x, y, 13, UI_MUTED, line);
        x += text_width(line, 13);
        if (x > SCREEN_WIDTH - 110) { break; }
    }
}

static void cal_frame(App *app, int step, const int rawX[TOUCH_CAL_POINTS],
                      const int rawY[TOUCH_CAL_POINTS], const char *hint)
{
    char line[64];
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, UI_BG);
    cal_centre_text(app, 14, 30, UI_INK, "触摸屏标定");
    snprintf(line, sizeof(line), "第 %d / %d 点", step + 1, TOUCH_CAL_POINTS);
    cal_centre_text(app, 52, 16, UI_MUTED, line);
    cal_pips(app, step);
    cal_centre_text(app, 120, 18, UI_INK, g_prompt[step]);
    cal_samples(app, rawX, rawY, step, 156);
    if (hint) {
        cal_centre_text(app, 196, 14, 0xAD4E3D, hint);
    }
    cal_target(app, TOUCH_CAL_TARGET[step][0], TOUCH_CAL_TARGET[step][1]);
}

static void cal_report(App *app, const TouchCal *cal, int error, bool saved)
{
    char line[64];
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, UI_BG);
    cal_centre_text(app, 92, 32, UI_TEAL, "标定完成");
    snprintf(line, sizeof(line), "X 原始范围 %d .. %d", cal->x0, cal->x1);
    cal_centre_text(app, 168, 19, UI_INK, line);
    snprintf(line, sizeof(line), "Y 原始范围 %d .. %d", cal->y0, cal->y1);
    cal_centre_text(app, 202, 19, UI_INK, line);
    snprintf(line, sizeof(line), "轴向 %s，最大偏差 %d 像素",
        cal->swap ? "交换（原始 Y 为横轴）" : "正常", error);
    cal_centre_text(app, 236, 19, UI_INK, line);
    cal_centre_text(app, 292, 16, saved ? UI_MUTED : 0xAD4E3D,
        saved ? "已写入 data/touch.cal，正在进入登录页"
              : "未能写入 data/touch.cal，本次运行仍然生效");
}

static void cal_failure(App *app, const int rawX[TOUCH_CAL_POINTS],
                        const int rawY[TOUCH_CAL_POINTS], int error)
{
    char line[64];
    draw_rect(&app->screen, (Rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}, UI_BG);
    cal_centre_text(app, 96, 32, 0xAD4E3D, "标定失败");
    if (error < 0) {
        snprintf(line, sizeof(line), "左右或上下两点几乎重合，请按十字位置重新点");
    } else {
        snprintf(line, sizeof(line), "最大偏差 %d 像素，超出 %d 像素上限",
            error, TOUCH_CAL_TOLERANCE);
    }
    cal_centre_text(app, 156, 17, UI_INK, line);
    cal_centre_text(app, 200, 15, UI_MUTED, "这通常说明面板有一段区域报不出位置");
    for (int i = 0; i < TOUCH_CAL_POINTS; ++i) {
        snprintf(line, sizeof(line), "第 %d 点  raw %d,%d", i + 1, rawX[i], rawY[i]);
        cal_centre_text(app, 248 + i * 26, 15, UI_MUTED, line);
    }
    cal_centre_text(app, 404, 18, UI_INK, "轻点屏幕任意位置重新标定");
    cal_centre_text(app, 436, 13, UI_MUTED, "退出后加 --no-calibrate 可跳过本页");
}

/* One tap, delivered whenever it comes. Returns -1 when the device failed. The raw
   pair is read in the same breath as the event, before the next report can overwrite
   it, which is what makes the sample a measurement of the panel rather than of the
   mapping. */
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
            if (++idle == CAL_HINT_TICKS) {
                hint = "没有收到触摸？退出后加 --no-calibrate 可跳过标定";
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
        platform_cal_apply(&cal);
        bool saved = platform_cal_save(&cal);
        if (!saved) {
            fprintf(stderr, "calibration not saved: cannot write data/touch.cal\n");
        }
        fprintf(stderr, "calibration applied: swap=%d X=%d..%d Y=%d..%d worst=%d\n",
            cal.swap, cal.x0, cal.x1, cal.y0, cal.y1, error);
        cal_report(app, &cal, error, saved);
        platform_present(&app->screen);
        /* Hold the result long enough to be read, then hand over to the app. */
        for (int i = 0; i < 12; ++i) {
            Event event;
            if (platform_poll(&event, CAL_TICK_MS) < 0) { break; }
        }
        return;
    }
}
