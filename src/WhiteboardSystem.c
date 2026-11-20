/* Xie Donghong whiteboard adapted by Codex, 2026-11-19: connected strokes and automatic BMP names. */
#include "terminal.h"
#include <stdio.h>
#include <sys/stat.h>
static const uint32_t g_colors[] = {0x203B3C,0xCA554D,0xE8AF3E,0x227C77,0x487BBC,0x9B69AF,0xFFFFFF};
static const int g_radii[] = {2,3,7};
static const char *const g_radiusName[] = {"细","中","粗"};
static const Rect g_canvas = {24,156,752,286};

/* The toolbar row: seven colour chips then three brush buttons.
 *
 * Both upstream projects hit-test their toolbars the same way: first the whole row is
 * tested as one horizontal band (Xie Donghong: BUTTON_Y <= y <= Y+HEIGHT), then the X
 * decides which button, with the windows adjacent and sharing their edges. There is no
 * gap between targets, so a point in the band can never fall between two of them. The
 * band here is 82..152 and the columns tile from 24 to 458 and 470 to 710; the artwork
 * is drawn 6px inside its column, which is why the target can be bigger than the chip
 * without looking wrong. */
static Rect swatch_draw(int index) { return (Rect){24 + index * 62, 88, 56, 56}; }
static Rect swatch_hit(int index) { return (Rect){24 + index * 62, 82, 62, 70}; }
static Rect brush_draw(int index) { return (Rect){470 + index * 80, 88, 72, 56}; }
static Rect brush_hit(int index) { return (Rect){470 + index * 80, 82, 80, 70}; }

/* Preserve the canvas across screen navigation; save only the artwork, never the toolbar. */
void board_render(App *app)
{
    char line[96];
    ui_header(app,"创意画板");
    ui_button(app,(Rect){528,16,120,60},"清屏",false);
    ui_button(app,(Rect){656,16,120,60},"保存",true);
    for (int i = 0; i < 7; ++i) {
        Rect chip = swatch_draw(i);
        draw_rect(&app->screen,chip,i == app->color ? UI_TEAL : 0xDFE4DE);
        draw_rect(&app->screen,(Rect){chip.x+5,chip.y+5,46,46},g_colors[i]);
    }
    for (int i = 0; i < 3; ++i) {
        ui_button_sized(app,brush_draw(i),g_radiusName[i],app->radius == g_radii[i],22);
    }
    draw_rect(&app->screen,(Rect){23,155,754,288},0xD8DED7);
    draw_image(&app->screen,g_canvas,&app->drawing,false);
    draw_text(&app->screen,24,452,17,UI_MUTED,
        app->notice[0] ? app->notice : "白色是橡皮擦；保存后可在电子相册查看作品。");
    snprintf(line,sizeof(line),"x %d,y %d",app->tapX,app->tapY);
    draw_text(&app->screen,690,455,12,0x9C8F7B,line);
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

/* Paint the visible frame and remember the touched rectangle. The stroke is written
   to both buffers: app->drawing is the saved artwork, app->screen is what the panel
   shows, so the loop can push a few hundred pixels instead of repainting 800x480
   for every pen sample - that repaint was the lag. */
static void stamp(App *app, int x, int y, int radius)
{
    draw_disc(&app->drawing,x,y,radius,g_colors[app->color]);
    draw_disc(&app->screen,g_canvas.x+x,g_canvas.y+y,radius,g_colors[app->color]);
}

static void mark_segment(App *app, int x0, int y0, int x1, int y1, int radius)
{
    int pad = radius + 2;
    int left = (x0 < x1 ? x0 : x1) - pad, right = (x0 > x1 ? x0 : x1) + pad;
    int top = (y0 < y1 ? y0 : y1) - pad, bottom = (y0 > y1 ? y0 : y1) + pad;
    if (left < 0) { left = 0; }
    if (top < 0) { top = 0; }
    if (right > g_canvas.w - 1) { right = g_canvas.w - 1; }
    if (bottom > g_canvas.h - 1) { bottom = g_canvas.h - 1; }
    if (right < left || bottom < top) { return; }
    Rect area = {left + g_canvas.x, top + g_canvas.y, right - left + 1, bottom - top + 1};
    Rect merged = app->dirtyArea;
    if (merged.w <= 0 || merged.h <= 0) {
        app->dirtyArea = area;
        return;
    }
    int ax = merged.x < area.x ? merged.x : area.x;
    int ay = merged.y < area.y ? merged.y : area.y;
    int bx = merged.x + merged.w > area.x + area.w ? merged.x + merged.w : area.x + area.w;
    int by = merged.y + merged.h > area.y + area.h ? merged.y + merged.h : area.y + area.h;
    app->dirtyArea = (Rect){ax, ay, bx - ax, by - ay};
}

/* Toolbar hit test: seamless band first, then the nearest column centre as a fallback
   for a point just outside the band. Nothing inside 82..152 can fall through. */
static bool toolbar_hit(App *app, Event event)
{
    Rect rects[7];
    for (int i = 0; i < 7; ++i) { rects[i] = swatch_hit(i); }
    int index = ui_nearest(app,event,rects,7,40);
    if (index >= 0) {
        app->color = index;
        snprintf(app->notice,sizeof(app->notice),"%s", index == 6 ? "白色：橡皮擦" : "已换颜色，可以画了");
        app->dirty = true;
        return true;
    }
    for (int i = 0; i < 3; ++i) { rects[i] = brush_hit(i); }
    index = ui_nearest(app,event,rects,3,40);
    if (index >= 0) {
        app->radius = g_radii[index];
        snprintf(app->notice,sizeof(app->notice),"笔触：%s",g_radiusName[index]);
        app->dirty = true;
        return true;
    }
    return false;
}

/* Move samples are clipped; leaving the canvas stops the segment instead of bridging UI controls. */
void board_event(App *app, Event event)
{
    if (event.type == EVENT_DOWN && rect_has(g_canvas,event.x,event.y)) {
        app->painting = true;
        app->lastX = event.x - g_canvas.x; app->lastY = event.y - g_canvas.y;
        stamp(app,app->lastX,app->lastY,app->radius);
        mark_segment(app,app->lastX,app->lastY,app->lastX,app->lastY,app->radius);
        return;
    }
    if ((event.type == EVENT_MOVE || event.type == EVENT_UP) && app->painting) {
        if (rect_has(g_canvas,event.x,event.y)) {
            int x = event.x-g_canvas.x, y = event.y-g_canvas.y;
            draw_line(&app->drawing,app->lastX,app->lastY,x,y,app->radius,g_colors[app->color]);
            draw_line(&app->screen,g_canvas.x+app->lastX,g_canvas.y+app->lastY,
                g_canvas.x+x,g_canvas.y+y,app->radius,g_colors[app->color]);
            mark_segment(app,app->lastX,app->lastY,x,y,app->radius);
            app->lastX = x; app->lastY = y;
        } else { app->painting = false; }
        return;
    }
    if (event.type != EVENT_UP || rect_has(g_canvas,app->downX,app->downY)) { return; }
    if (toolbar_hit(app,event)) { return; }
    if (ui_clicked(app,event,(Rect){528,16,120,60})) {
        draw_rect(&app->drawing,(Rect){0,0,752,286},0xFFFFFF);
        snprintf(app->notice,sizeof(app->notice),"画布已清空");
    }
    if (ui_clicked(app,event,(Rect){656,16,120,60})) {
        snprintf(app->notice,sizeof(app->notice),"%s", save_drawing(app) ?
            "保存成功！返回相册即可查看作品。" : "保存失败，请检查目录权限或剩余空间。");
    }
    app->dirty = true;
}
