/* Xie Donghong whiteboard adapted by Codex, 2026-11-19: connected strokes and automatic BMP names. */
#include "terminal.h"
#include <stdio.h>
#include <sys/stat.h>
static const uint32_t g_colors[] = {0x203B3C,0xCA554D,0xE8AF3E,0x227C77,0x487BBC,0x9B69AF,0xFFFFFF};
static const Rect g_canvas = {24,156,752,286};

/* Preserve the canvas across screen navigation; save only the artwork, never the toolbar. */
void board_render(App *app)
{
    ui_header(app,"创意画板");
    ui_button(app,(Rect){548,18,108,44},"清屏",false);
    ui_button(app,(Rect){668,18,108,44},"保存",true);
    for (int i = 0; i < 7; ++i) {
        int x = 24 + i * 49;
        draw_rect(&app->screen,(Rect){x,91,42,42},i == app->color ? UI_TEAL : 0xDFE4DE);
        draw_rect(&app->screen,(Rect){x+4,95,34,34},g_colors[i]);
    }
    draw_text(&app->screen,375,102,19,UI_MUTED,"笔触");
    const int radii[] = {2,3,7};
    const char *labels[] = {"细","中","粗"};
    for (int i = 0; i < 3; ++i) {
        ui_button(app,(Rect){430+i*62,91,54,42},labels[i],app->radius == radii[i]);
    }
    draw_text(&app->screen,632,102,19,UI_MUTED,"白色 = 橡皮擦");
    draw_rect(&app->screen,(Rect){23,155,754,288},0xD8DED7);
    draw_image(&app->screen,g_canvas,&app->drawing,false);
    draw_text(&app->screen,24,453,18,UI_MUTED,
        app->notice[0] ? app->notice : "保存后可在电子相册查看作品；返回后画布保留。");
}

/* Timestamp plus a checked suffix avoids overwriting a drawing made in the same second. */
static bool save_drawing(App *app)
{
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    char stamp[32] = "undated", path[PATH_CAPACITY];
    if (local) { strftime(stamp,sizeof(stamp),"%Y%m%d_%H%M%S",local); }
    for (unsigned int suffix = 0; suffix < 1000; ++suffix) {
        int length = snprintf(path,sizeof(path),"%s/drawing_%s_%03u.bmp",app->saveDir,stamp,suffix);
        if (length < 0 || (size_t)length >= sizeof(path)) { return false; }
        struct stat info;
        if (stat(path,&info) == 0) { continue; }
        return image_save(path,&app->drawing);
    }
    return false;
}

/* Move samples are clipped; leaving the canvas stops the segment instead of bridging UI controls. */
void board_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN && rect_has(g_canvas,event.x,event.y)) {
        app->painting = true;
        app->lastX = event.x - g_canvas.x; app->lastY = event.y - g_canvas.y;
        draw_disc(&app->drawing,app->lastX,app->lastY,app->radius,g_colors[app->color]);
        app->dirty = true;
    } else if ((event.type == EVENT_MOVE || event.type == EVENT_UP) && app->painting) {
        if (rect_has(g_canvas,event.x,event.y)) {
            int x = event.x-g_canvas.x, y = event.y-g_canvas.y;
            draw_line(&app->drawing,app->lastX,app->lastY,x,y,app->radius,g_colors[app->color]);
            app->lastX = x; app->lastY = y;
        } else { app->painting = false; }
        app->dirty = true;
    }
    if (event.type != EVENT_UP || rect_has(g_canvas,app->downX,app->downY)) { return; }
    for (int i = 0; i < 7; ++i) {
        if (ui_clicked(app,event,(Rect){24+i*49,91,42,42})) { app->color = i; }
    }
    const int radii[] = {2,3,7};
    for (int i = 0; i < 3; ++i) {
        if (ui_clicked(app,event,(Rect){430+i*62,91,54,42})) { app->radius = radii[i]; }
    }
    if (ui_clicked(app,event,(Rect){548,18,108,44})) {
        draw_rect(&app->drawing,(Rect){0,0,752,286},0xFFFFFF);
        snprintf(app->notice,sizeof(app->notice),"画布已清空");
    }
    if (ui_clicked(app,event,(Rect){668,18,108,44})) {
        snprintf(app->notice,sizeof(app->notice),"%s", save_drawing(app) ?
            "保存成功！返回相册即可查看作品。" : "保存失败，请检查目录权限或剩余空间。");
    }
    app->dirty = true;
}
