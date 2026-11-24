/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   WhiteboardSystem.c
Author:      汪贤云        Version: 1.0.2       Date: 2026-11-19
Description: 创意画板页面。提供七种颜色、三档笔触、连续笔画、清屏与保存。
             笔画同时写进两份缓冲：app->drawing 是最终保存的作品，app->screen 是
             屏幕上显示的画面。这样主循环每次只需推变化的一小块矩形，而不必为每个
             采样点重绘整屏 800x480——原先的卡顿正是整屏重绘造成的。
Others:      白色是橡皮擦。文件名采用时间戳加序号，避免同一秒内保存两次相互覆盖。
Function List:
  1. board_render   绘制画板：工具栏、颜色、笔触、画布与状态提示
  2. board_event    处理落笔、移动、抬起以及工具栏与按钮点击
  3. save_drawing   生成不重复的文件名并保存画布
  4. stamp          在两份缓冲的同一点画一个圆
  5. mark_segment   把一段笔画涉及的矩形并入脏区
  6. toolbar_hit    工具栏命中判定：颜色与笔触
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 由谢东宏工程改写：连续笔画、局部刷新、自动命名
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 注释改为中文规范格式
******************************************************************************/
#include "terminal.h"
#include "logger.h"
#include <stdio.h>
#include <sys/stat.h>

/* 七种颜色，最后一个是白色，作为橡皮擦使用 */
static const uint32_t g_colors[] = {
    0x203B3C, 0xCA554D, 0xE8AF3E, 0x227C77, 0x487BBC, 0x9B69AF, 0xFFFFFF
};
/* 三档笔触半径 */
static const int g_radii[] = {2, 3, 7};
/* 三档笔触的名称 */
static const char *const g_radiusName[] = {"细", "中", "粗"};
/* 画布区域，尺寸与 app->drawing 一致，坐标系为屏幕像素 */
static const Rect g_canvas = {24, 156, 752, 286};

/* 工具栏的绘制区域与命中区域。
   说明：上游两个工程都是先把整行当成一个横向带来判定（谢东宏：BUTTON_Y <= y <=
   Y+HEIGHT），再由 X 决定是哪个按钮，各窗口紧邻并共用边界，没有缝隙，因此落在
   带内的点不会掉进两个目标中间。这里的带为 82..152，颜色列从 24 铺到 458，
   笔触列从 470 铺到 710；图形在列内缩 6 像素绘制，所以目标可以比图形大而不显怪。 */
static Rect swatch_draw(int index) { return (Rect){24 + index * 62, 88, 56, 56}; }
static Rect swatch_hit(int index) { return (Rect){24 + index * 62, 82, 62, 70}; }
static Rect brush_draw(int index) { return (Rect){470 + index * 80, 88, 72, 56}; }
static Rect brush_hit(int index) { return (Rect){470 + index * 80, 82, 80, 70}; }

/* 绘制画板。切换页面时画布内容保留，保存时只存作品本身，不含工具栏。 */
void board_render(App *app)
{
    char line[96];

    ui_header(app, "创意画板");
    ui_button(app, (Rect){528, 16, 120, 60}, "清屏", false);
    ui_button(app, (Rect){656, 16, 120, 60}, "保存", true);

    /* 七个颜色块，当前选中的加青色外框 */
    for (int i = 0; i < 7; ++i) {
        Rect chip = swatch_draw(i);
        draw_rect(&app->screen, chip, i == app->color ? UI_TEAL : 0xDFE4DE);
        draw_rect(&app->screen, (Rect){chip.x + 5, chip.y + 5, 46, 46}, g_colors[i]);
    }

    /* 三个笔触按钮，当前选中的作为主要按钮高亮 */
    for (int i = 0; i < 3; ++i) {
        ui_button_sized(app, brush_draw(i), g_radiusName[i], app->radius == g_radii[i], 22);
    }

    /* 画布外框与作品内容 */
    draw_rect(&app->screen, (Rect){23, 155, 754, 288}, 0xD8DED7);
    draw_image(&app->screen, g_canvas, &app->drawing, false);

    /* 状态提示：优先显示操作结果，没有时显示默认说明 */
    draw_text(&app->screen, 24, 452, 17, UI_MUTED,
        app->notice[0] ? app->notice : "白色是橡皮擦；保存后可在电子相册查看作品。");

    /* 右下角显示最近一次落点，方便观察触摸是否到位 */
    snprintf(line, sizeof(line), "x %d,y %d", app->tapX, app->tapY);
    draw_text(&app->screen, 690, 455, 12, 0x9C8F7B, line);
}

/* 保存作品。时间戳加三位序号，避免同一秒内连按两次保存互相覆盖；
   序号从 0 试到 999，遇到已存在的文件就往后顺延。 */
static bool save_drawing(App *app)
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char stamp[32] = "undated";
    char path[PATH_CAPACITY];

    if (local) { strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", local); }
    for (unsigned int suffix = 0; suffix < 1000; ++suffix) {
        int length = snprintf(path, sizeof(path), "%s/drawing_%s_%03u.bmp",
            app->saveDir, stamp, suffix);

        if (length < 0 || (size_t)length >= sizeof(path)) {
            log_write(LOG_ERROR, "save drawing failed: path too long");
            return false;
        }
        struct stat info;
        if (stat(path, &info) == 0) { continue; }
        /* 保存结果直接写日志：文件名里带时间戳，回看日志就知道哪一次画板作品落盘了 */
        if (image_save(path, &app->drawing)) {
            log_write(LOG_INFO, "drawing saved: %s", path);
            return true;
        }
        log_write(LOG_ERROR, "save drawing failed: %s", path);
        return false;
    }
    log_write(LOG_ERROR, "save drawing failed: no free suffix");
    return false;
}

/* 在两份缓冲的同一点画一个圆。
   参数 x/y 是画布内的相对坐标，screen 缓冲要再加上画布左上角偏移。 */
static void stamp(App *app, int x, int y, int radius)
{
    draw_disc(&app->drawing, x, y, radius, g_colors[app->color]);
    draw_disc(&app->screen, g_canvas.x + x, g_canvas.y + y, radius, g_colors[app->color]);
}

/* 把一段笔画涉及的矩形并入脏区。
   说明：脏区取各段的外接矩形，主循环据此只提交这一块；坐标从画布相对坐标
   换算成屏幕坐标，并裁剪在画布范围内，避免覆盖工具栏。 */
static void mark_segment(App *app, int x0, int y0, int x1, int y1, int radius)
{
    int pad = radius + 2;
    int left = (x0 < x1 ? x0 : x1) - pad;
    int right = (x0 > x1 ? x0 : x1) + pad;
    int top = (y0 < y1 ? y0 : y1) - pad;
    int bottom = (y0 > y1 ? y0 : y1) + pad;

    if (left < 0) { left = 0; }
    if (top < 0) { top = 0; }
    if (right > g_canvas.w - 1) { right = g_canvas.w - 1; }
    if (bottom > g_canvas.h - 1) { bottom = g_canvas.h - 1; }
    if (right < left || bottom < top) { return; }

    Rect area = {left + g_canvas.x, top + g_canvas.y,
        right - left + 1, bottom - top + 1};
    Rect merged = app->dirtyArea;
    if (merged.w <= 0 || merged.h <= 0) {
        app->dirtyArea = area;
        return;
    }

    /* 新旧脏区合并成一个更大的外接矩形 */
    int ax = merged.x < area.x ? merged.x : area.x;
    int ay = merged.y < area.y ? merged.y : area.y;
    int bx = merged.x + merged.w > area.x + area.w ? merged.x + merged.w : area.x + area.w;
    int by = merged.y + merged.h > area.y + area.h ? merged.y + merged.h : area.y + area.h;
    app->dirtyArea = (Rect){ax, ay, bx - ax, by - ay};
}

/* 工具栏命中判定。颜色区与笔触区各调用一次 ui_nearest：
   先在无缝隙的带内精确命中，带外一点点则退化为就近取列中心，搜索半径 40 像素。 */
static bool toolbar_hit(App *app, Event event)
{
    Rect rects[7];

    for (int i = 0; i < 7; ++i) { rects[i] = swatch_hit(i); }
    int index = ui_nearest(app, event, rects, 7, 40);
    if (index >= 0) {
        app->color = index;
        snprintf(app->notice, sizeof(app->notice), "%s",
            index == 6 ? "白色：橡皮擦" : "已换颜色，可以画了");
        app->dirty = true;
        return true;
    }

    for (int i = 0; i < 3; ++i) { rects[i] = brush_hit(i); }
    index = ui_nearest(app, event, rects, 3, 40);
    if (index >= 0) {
        app->radius = g_radii[index];
        snprintf(app->notice, sizeof(app->notice), "笔触：%s", g_radiusName[index]);
        app->dirty = true;
        return true;
    }
    return false;
}

/* 处理画板输入。
   说明：移动采样一律裁剪在画布内；手指离开画布就停止这一笔，而不是把画板上的
   线段硬接到工具栏上。按下点不在画布内时，才去判定工具栏、清屏与保存。 */
void board_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN && rect_has(g_canvas, event.x, event.y)) {
        app->painting = true;
        app->lastX = event.x - g_canvas.x;
        app->lastY = event.y - g_canvas.y;
        stamp(app, app->lastX, app->lastY, app->radius);
        mark_segment(app, app->lastX, app->lastY, app->lastX, app->lastY, app->radius);
        return;
    }

    if ((event.type == EVENT_MOVE || event.type == EVENT_UP) && app->painting) {
        if (rect_has(g_canvas, event.x, event.y)) {
            int x = event.x - g_canvas.x;
            int y = event.y - g_canvas.y;

            draw_line(&app->drawing, app->lastX, app->lastY, x, y,
                app->radius, g_colors[app->color]);
            draw_line(&app->screen, g_canvas.x + app->lastX, g_canvas.y + app->lastY,
                g_canvas.x + x, g_canvas.y + y, app->radius, g_colors[app->color]);
            mark_segment(app, app->lastX, app->lastY, x, y, app->radius);
            app->lastX = x;
            app->lastY = y;
        } else {
            app->painting = false;
        }
        return;
    }

    /* 抬起点不在画布内时，才判定工具栏与两个按钮 */
    if (event.type != EVENT_UP || rect_has(g_canvas, app->downX, app->downY)) { return; }
    if (toolbar_hit(app, event)) { return; }

    if (ui_clicked(app, event, (Rect){528, 16, 120, 60})) {
        draw_rect(&app->drawing, (Rect){0, 0, 752, 286}, 0xFFFFFF);
        snprintf(app->notice, sizeof(app->notice), "画布已清空");
    }
    if (ui_clicked(app, event, (Rect){656, 16, 120, 60})) {
        snprintf(app->notice, sizeof(app->notice), "%s", save_drawing(app) ?
            "保存成功！返回相册即可查看作品。" : "保存失败，请检查目录权限或剩余空间。");
    }
    app->dirty = true;
}
