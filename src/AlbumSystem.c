/* Xie Donghong album adapted by Codex, 2026-11-19: bounded paths, paged cache and saved drawings. */
#include "terminal.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
            if (image_create(&album->thumbs[i], 224, 116)) {
                draw_rect(&album->thumbs[i], (Rect){0,0,224,116}, 0xE6EBE6);
                draw_image(&album->thumbs[i], (Rect){0,0,224,116}, &original, true);
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

/* Six touch-sized thumbnails and a separate fitted large view share one navigation model. */
void album_render(App *app)
{
    Album *album = &app->album;
    ui_header(app, app->page == PAGE_PHOTO ? "照片浏览" : "电子相册");
    char label[96];
    if (app->page == PAGE_PHOTO) {
        draw_rect(&app->screen,(Rect){24,90,752,324},0xE6EBE6);
        draw_image(&app->screen,(Rect){24,90,752,324},&album->photo,true);
        ui_button(app,(Rect){24,426,130,44},"缩略图",false);
        ui_button(app,(Rect){218,426,112,44},"上一张",false);
        ui_button(app,(Rect){474,426,112,44},"下一张",false);
        snprintf(label,sizeof(label),"%zu / %zu",album->selected+1,album->count);
        draw_text(&app->screen,353,438,20,UI_INK,label);
    } else {
        for (size_t i = 0; i < 6; ++i) {
            size_t index = album->page * 6 + i;
            if (index >= album->count) { break; }
            int x = 24 + (int)(i % 3) * 256, y = 94 + (int)(i / 3) * 162;
            draw_rect(&app->screen,(Rect){x,y,240,150},0xFFFFFF);
            draw_image(&app->screen,(Rect){x+8,y+7,224,116},&album->thumbs[i],true);
            if (!album->thumbs[i].pixels) { draw_text(&app->screen,x+10,y+40,20,UI_MUTED,"图片不可读取"); }
            bool drawing = strncmp(album->paths[index],app->saveDir,strlen(app->saveDir)) == 0;
            snprintf(label,sizeof(label),"%s %02zu",drawing ? "画板作品" : "照片",index+1);
            draw_text(&app->screen,x+10,y+126,19,UI_MUTED,label);
        }
        if (!album->count) { draw_text(&app->screen,240,222,28,UI_MUTED,"暂无照片，先去画板创作吧"); }
        ui_button(app,(Rect){24,426,112,44},"上一页",false);
        ui_button(app,(Rect){664,426,112,44},"下一页",false);
        snprintf(label,sizeof(label),"第 %zu / %zu 页 · 共 %zu 张%s",album->page+1,
            album->count ? (album->count+5)/6 : 1,album->count,album->limited ? " (已达上限)" : "");
        draw_text(&app->screen,218,438,20,UI_INK,label);
    }
    if (app->notice[0]) { draw_text(&app->screen,228,65,18,0xAD4E3D,app->notice); }
}

/* Page changes refresh only six thumbnails; large-photo navigation wraps at both ends. */
void album_event(App *app, Event event)
{
    Album *album = &app->album;
    if (event.type != EVENT_UP) { return; }
    if (app->page == PAGE_PHOTO) {
        if (ui_clicked(app,event,(Rect){24,426,130,44})) {
            app->page = PAGE_ALBUM; app->notice[0] = '\0'; image_free(&album->photo);
        } else if (ui_clicked(app,event,(Rect){218,426,112,44}) && album->count) {
            album->selected = (album->selected + album->count - 1) % album->count; load_photo(app);
        } else if (ui_clicked(app,event,(Rect){474,426,112,44}) && album->count) {
            album->selected = (album->selected + 1) % album->count; load_photo(app);
        }
    } else {
        if (ui_clicked(app,event,(Rect){24,426,112,44}) && album->page) {
            --album->page; load_page(album);
        } else if (ui_clicked(app,event,(Rect){664,426,112,44}) && (album->page+1)*6 < album->count) {
            ++album->page; load_page(album);
        } else {
            for (size_t i = 0; i < 6; ++i) {
                Rect rect = {24+(int)(i%3)*256,94+(int)(i/3)*162,240,150};
                if (ui_clicked(app,event,rect) && album->page*6+i < album->count) {
                    album->selected = album->page*6+i;
                    app->page = PAGE_PHOTO; load_photo(app); break;
                }
            }
        }
    }
    app->dirty = true;
}
