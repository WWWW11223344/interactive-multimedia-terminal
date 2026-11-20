/* Xie Donghong album adapted by Codex, 2026-11-19: bounded paths, paged cache and saved drawings. */
#include "terminal.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define THUMB_W 224
#define THUMB_H 136

/* Release page-local images; path strings live inline in Album. */
void album_close(Album *album)
{
    if (!album) { return; }
    for (int i = 0; i < 6; ++i) { image_free(&album->thumbs[i]); }
    image_free(&album->photo);
}

/* Enumerate regular BMP files only, with a fixed count and path limit. */
static bool scan_directory(Album *album, const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) { return false; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t length = strlen(entry->d_name);
        if (length < 4 || (strcmp(entry->d_name + length - 4, ".bmp") != 0 &&
            strcmp(entry->d_name + length - 4, ".BMP") != 0)) { continue; }
        char fullPath[PATH_CAPACITY];
        int written = snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        struct stat info;
        if (written < 0 || (size_t)written >= sizeof(fullPath) || stat(fullPath, &info) != 0 ||
            !S_ISREG(info.st_mode)) { continue; }
        if (album->count == ALBUM_LIMIT) { album->limited = true; continue; }
        snprintf(album->paths[album->count++], PATH_CAPACITY, "%s", fullPath);
    }
    closedir(dir);
    return true;
}

/* Most recent timestamp-named drawings sort first within their directory. */
static int compare_paths(const void *left, const void *right)
{
    return strcmp((const char *)right, (const char *)left);
}

/* Decode at most one page, then keep compact thumbnails instead of full-size source images. */
static void load_page(Album *album)
{
    for (size_t i = 0; i < 6; ++i) {
        image_free(&album->thumbs[i]);
        size_t index = album->page * 6 + i;
        Image original = {0};
        if (index < album->count && image_load(album->paths[index], &original)) {
            if (image_create(&album->thumbs[i], THUMB_W, THUMB_H)) {
                draw_rect(&album->thumbs[i], (Rect){0,0,THUMB_W,THUMB_H}, 0xE6EBE6);
                draw_image(&album->thumbs[i], (Rect){0,0,THUMB_W,THUMB_H}, &original, true);
            }
            image_free(&original);
        }
    }
}

/* Re-entering the album rescans drawings, so saving needs no cross-module list mutation. */
bool album_scan(App *app)
{
    Album *album = &app->album;
    album_close(album);
    *album = (Album){0};
    bool drawingsOk = scan_directory(album, app->saveDir);
    qsort(album->paths, album->count, sizeof(album->paths[0]), compare_paths);
    size_t drawings = album->count;
    bool photosOk = scan_directory(album, "res/Album");
    qsort(album->paths + drawings, album->count - drawings, sizeof(album->paths[0]), compare_paths);
    load_page(album);
    return drawingsOk && photosOk;
}

/* A corrupt or removed photo is represented explicitly and navigation remains available. */
static void load_photo(App *app)
{
    image_free(&app->album.photo);
    if (!image_load(app->album.paths[app->album.selected], &app->album.photo)) {
        snprintf(app->notice, sizeof(app->notice), "图片损坏、格式不支持或文件已移除");
    } else { app->notice[0] = '\0'; }
}

/* One hit cell per thumbnail, and the six cells tile the whole grid with no gutter.
 *
 * Before, the cells were 150px tall and separated by a 12px gap; the panel's Y can be
 * off by more than that, so a tap on the top row landed in the gap and did nothing -
 * "the top half cannot be opened, the bottom half can". Covering the area edge to edge
 * removes the dead band, and each cell is 162px tall so a tap 80px high or low still
 * resolves to the picture under the finger. The drawn thumbnail is inset inside the
 * same cell, so what the user aims at is what the code tests. */
static Rect thumb_cell(size_t index)
{
    return (Rect){24 + (int)(index % 3) * 252, 86 + (int)(index / 3) * 162, 252, 162};
}

/* Six touch-sized thumbnails and a separate fitted large view share one navigation model. */
void album_render(App *app)
{
    Album *album = &app->album;
    ui_header(app, app->page == PAGE_PHOTO ? "照片浏览" : "电子相册");
    char label[96];
    if (app->page == PAGE_PHOTO) {
        draw_rect(&app->screen,(Rect){24,90,752,324},0xE6EBE6);
        draw_image(&app->screen,(Rect){24,90,752,324},&album->photo,true);
        /* 64px tall and 40px apart at the bottom edge: the panel's error is vertical,
           so the buttons get height rather than width, and ui_nearest() below catches
           a release point that lands between them. */
        ui_button(app,(Rect){24,410,140,64},"缩略图",false);
        ui_button(app,(Rect){180,410,140,64},"上一张",false);
        ui_button(app,(Rect){560,410,140,64},"下一张",false);
        snprintf(label,sizeof(label),"%zu / %zu",album->selected+1,album->count);
        draw_text(&app->screen,366,432,22,UI_INK,label);
    } else {
        for (size_t i = 0; i < 6; ++i) {
            size_t index = album->page * 6 + i;
            if (index >= album->count) { break; }
            Rect cell = thumb_cell(i);
            int x = cell.x, y = cell.y;
            draw_rect(&app->screen,cell,0xFFFFFF);
            draw_image(&app->screen,(Rect){x+14,y+6,THUMB_W,THUMB_H},&album->thumbs[i],false);
            if (!album->thumbs[i].pixels) { draw_text(&app->screen,x+30,y+60,20,UI_MUTED,"图片不可读取"); }
            bool drawing = strncmp(album->paths[index],app->saveDir,strlen(app->saveDir)) == 0;
            snprintf(label,sizeof(label),"%s %02zu",drawing ? "画板作品" : "照片",index+1);
            draw_text(&app->screen,x+16,y+148,16,UI_MUTED,label);
        }
        if (!album->count) { draw_text(&app->screen,240,222,28,UI_MUTED,"暂无照片，先去画板创作吧"); }
        ui_button(app,(Rect){24,420,112,54},"上一页",false);
        ui_button(app,(Rect){664,420,112,54},"下一页",false);
        snprintf(label,sizeof(label),"第 %zu/%zu 页 · 共 %zu 张 · 点缩略图看大图",album->page+1,
            album->count ? (album->count+5)/6 : 1,album->count);
        draw_text(&app->screen,160,438,17,UI_INK,label);
        if (album->limited) { draw_text(&app->screen,160,462,14,UI_MUTED,"已达 128 张上限"); }
    }
    if (app->notice[0]) { draw_text(&app->screen,228,65,18,0xAD4E3D,app->notice); }
    /* Always show where the last tap landed: a silent tap is indistinguishable from a
       missing feature, and the number is what turns a report into a measurement. */
    snprintf(label,sizeof(label),"%s %d,%d",app->missTap ? "未命中" : "点选",app->tapX,app->tapY);
    draw_text(&app->screen,600,66,15,app->missTap ? 0xAD4E3D : 0x9C8F7B,label);
}

/* The cells tile the grid, so an exact test almost always decides; the nearest-centre
   fallback only covers a release point a little outside the grid (above the header rule
   or beside the last column). */
static int thumb_at(const Album *album, int x, int y)
{
    for (size_t i = 0; i < 6; ++i) {
        if (album->page*6+i >= album->count) { continue; }
        if (rect_has(thumb_cell(i), x, y)) { return (int)i; }
    }
    int best = -1;
    int64_t bestDistance = 80LL * 80LL;
    for (size_t i = 0; i < 6; ++i) {
        if (album->page*6+i >= album->count) { continue; }
        Rect cell = thumb_cell(i);
        int64_t dx = x - (cell.x + cell.w/2);
        int64_t dy = y - (cell.y + cell.h/2);
        int64_t distance = dx*dx + dy*dy;
        if (distance < bestDistance) { bestDistance = distance; best = (int)i; }
    }
    return best;
}

/* Page changes refresh only six thumbnails; large-photo navigation wraps at both ends. */
void album_event(App *app, Event event)
{
    Album *album = &app->album;
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;
    if (app->page == PAGE_PHOTO) {
        /* Targets are taller and lower than the drawn buttons: the bottom edge is where
           a finger naturally lands, and the panel reports a little above it. */
        static const Rect nav[3] = {{18,402,152,78},{174,402,152,78},{554,402,152,78}};
        int target = ui_nearest(app,event,nav,3,44);
        if (target == 0) {
            app->page = PAGE_ALBUM; app->notice[0] = '\0'; image_free(&album->photo);
        } else if (target == 1 && album->count) {
            album->selected = (album->selected + album->count - 1) % album->count; load_photo(app);
        } else if (target == 2 && album->count) {
            album->selected = (album->selected + 1) % album->count; load_photo(app);
        } else if (rect_has((Rect){24,90,752,324},event.x,event.y) ||
                   rect_has((Rect){24,90,752,324},app->lastSampleX,app->lastSampleY)) {
            /* Xie Donghong's album toggles: tapping the enlarged picture itself returns
               to the grid, so leaving the view never depends on finding a button. */
            app->page = PAGE_ALBUM; app->notice[0] = '\0'; image_free(&album->photo);
        } else { app->missTap = true; }
    } else {
        /* The two pager buttons sit on the bottom edge with 54px of height; the target
           band is taller and starts higher, so a finger landing at the very bottom
           still resolves. Everything else in the row is the thumbnail grid. */
        static const Rect pager[2] = {{18,402,124,78},{658,402,124,78}};
        int target = ui_nearest(app,event,pager,2,40);
        if (target == 0) {
            if (album->page) { --album->page; load_page(album); }
        } else if (target == 1) {
            if ((album->page+1)*6 < album->count) { ++album->page; load_page(album); }
        } else {
            /* All three points the gesture produced, in order of reliability. */
            int cell = thumb_at(album, event.x, event.y);
            if (cell < 0) { cell = thumb_at(album, app->lastSampleX, app->lastSampleY); }
            if (cell < 0) { cell = thumb_at(album, app->downX, app->downY); }
            if (cell >= 0) {
                album->selected = album->page*6 + (size_t)cell;
                app->page = PAGE_PHOTO; load_photo(app);
            } else { app->missTap = true; }
        }
    }
    app->dirty = true;
}
