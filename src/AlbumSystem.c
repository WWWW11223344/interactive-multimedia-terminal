/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   AlbumSystem.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 电子相册页面，包含网格缩略图与大图浏览两个视图。扫描画板保存目录与
             res/Album 目录中的 BMP 文件，按时间戳命名的文件名逆序排列，最新的
             作品排在前面；每页六张缩略图，只缓存当前页的小图，点开才加载大图。
             大图浏览支持上一张、下一张与回到网格，点大图本身也能返回。
Others:      缩略命中格无空隙铺满网格，每格 252×162 像素。此前每格高 150 像素、
             格间留 12 像素缝隙，而面板的纵向误差超过缝隙，点上排时落进缝隙毫无
             反应，表现为「上半屏打不开、下半屏能打开」；铺满后上下偏移 80 像素
             仍落在手指所指的那张图上，且缩略图内嵌在同一格内，所见即所测。
             底部按钮取高度而非宽度，另由 ui_nearest() 兜住落在按钮之间的抬起点。
Function List:
  1. album_close    释放本页加载的图片，路径字符串内嵌在 Album 中无需释放
  2. scan_directory 枚举目录中的 BMP 文件，带固定数量与路径长度上限
  3. compare_paths  qsort 比较函数，按路径名逆序排列使最新作品在前
  4. load_page      只解码当前页的六张缩略图，保留小图而非原图
  5. album_scan     重新扫描画板作品与照片，排序并加载首页
  6. load_photo     加载大图，损坏或移除时显式提示且仍可导航
  7. thumb_cell     求某个缩略格的命中区域，六格无空隙铺满网格
  8. album_render   绘制网格与大图两种视图
  9. thumb_at       把屏幕坐标换算成缩略格下标，带就近吸附
 10. album_event    处理翻页、大图导航与缩略图点选
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#include "terminal.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* 缩略图的目标宽度与高度 */
#define THUMB_W 224
#define THUMB_H 136

/* 释放本页加载的图片；路径字符串内嵌在 Album 结构里，无需单独释放 */
void album_close(Album *album)
{
    if (!album) { return; }
    for (int i = 0; i < 6; ++i) { image_free(&album->thumbs[i]); }
    image_free(&album->photo);
}

/* 只枚举普通 BMP 文件，数量与路径长度都有固定上限 */
static bool scan_directory(Album *album, const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) { return false; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t length = strlen(entry->d_name);
        /* 大小写两种后缀都接受 */
        if (length < 4 || (strcmp(entry->d_name + length - 4, ".bmp") != 0 &&
            strcmp(entry->d_name + length - 4, ".BMP") != 0)) { continue; }
        char fullPath[PATH_CAPACITY];
        int written = snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        struct stat info;
        /* 路径被截断、stat 失败或不是普通文件时跳过 */
        if (written < 0 || (size_t)written >= sizeof(fullPath) || stat(fullPath, &info) != 0 ||
            !S_ISREG(info.st_mode)) { continue; }
        /* 超过上限就不再收录，但要继续把目录读完 */
        if (album->count == ALBUM_LIMIT) { album->limited = true; continue; }
        snprintf(album->paths[album->count++], PATH_CAPACITY, "%s", fullPath);
    }
    closedir(dir);
    return true;
}

/* qsort 比较函数：按路径名逆序排列，时间戳命名的作品因此最新的在前 */
static int compare_paths(const void *left, const void *right)
{
    return strcmp((const char *)right, (const char *)left);
}

/* 一次只解码一页，且保留紧凑缩略图而不是原尺寸大图 */
static void load_page(Album *album)
{
    for (size_t i = 0; i < 6; ++i) {
        image_free(&album->thumbs[i]);
        size_t index = album->page * 6 + i;
        Image original = {0};
        /* 超出范围的页位保持空白；解码成功后缩放并立即释放原图 */
        if (index < album->count && image_load(album->paths[index], &original)) {
            if (image_create(&album->thumbs[i], THUMB_W, THUMB_H)) {
                draw_rect(&album->thumbs[i], (Rect){0,0,THUMB_W,THUMB_H}, 0xE6EBE6);
                draw_image(&album->thumbs[i], (Rect){0,0,THUMB_W,THUMB_H}, &original, true);
            }
            image_free(&original);
        }
    }
}

/* 重新进入相册会重扫作品目录，因此保存作品无需跨模块改动列表 */
bool album_scan(App *app)
{
    Album *album = &app->album;
    album_close(album);
    *album = (Album){0};
    /* 先扫画板作品并排序 */
    bool drawingsOk = scan_directory(album, app->saveDir);
    qsort(album->paths, album->count, sizeof(album->paths[0]), compare_paths);
    size_t drawings = album->count;
    /* 再扫相册目录中的照片，接在作品之后单独排序 */
    bool photosOk = scan_directory(album, "res/Album");
    qsort(album->paths + drawings, album->count - drawings, sizeof(album->paths[0]), compare_paths);
    load_page(album);
    return drawingsOk && photosOk;
}

/* 大图损坏或文件被移除时显式提示，翻页导航仍然可用 */
static void load_photo(App *app)
{
    image_free(&app->album.photo);
    if (!image_load(app->album.paths[app->album.selected], &app->album.photo)) {
        snprintf(app->notice, sizeof(app->notice), "图片损坏、格式不支持或文件已移除");
    } else { app->notice[0] = '\0'; }
}

/* 求某个缩略格的命中区域：一格对应一张缩略图，六格无空隙铺满整个网格。
   说明：原来每格高 150 像素且格间留 12 像素缝隙，而面板的纵向误差可能超过这个缝隙，
         点在上排时会落进缝隙里毫无反应，表现为「上半屏打不开、下半屏能打开」。
         现在六格铺满、每格高 162 像素，点击上下偏移 80 像素仍落在手指所指的那张图上；
         绘制的缩略图内嵌在同一格里，因此看到的和代码判定的是同一块区域。 */
static Rect thumb_cell(size_t index)
{
    return (Rect){24 + (int)(index % 3) * 252, 86 + (int)(index / 3) * 162, 252, 162};
}

/* 六张触摸尺寸的缩略图与单独适配的大图视图共用同一套导航模型 */
void album_render(App *app)
{
    Album *album = &app->album;
    ui_header(app, app->page == PAGE_PHOTO ? "照片浏览" : "电子相册");
    char label[96];
    if (app->page == PAGE_PHOTO) {
        draw_rect(&app->screen,(Rect){24,90,752,324},0xE6EBE6);
        draw_image(&app->screen,(Rect){24,90,752,324},&album->photo,true);
        /* 底部按钮高 64 像素、间隔 40 像素：面板误差在纵向，所以按钮取高度而非宽度；
           下面的 ui_nearest() 会兜住落在两个按钮之间的抬起点。 */
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
            /* 缩略图解码失败时给出文字提示 */
            if (!album->thumbs[i].pixels) { draw_text(&app->screen,x+30,y+60,20,UI_MUTED,"图片不可读取"); }
            /* 路径落在保存目录下即视为画板作品，否则视为照片 */
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
    /* 总是回显最后一次点击的坐标：无声的点击和功能缺失难以区分，而坐标能把
       「点不动」变成可度量的现象。 */
    snprintf(label,sizeof(label),"%s %d,%d",app->missTap ? "未命中" : "点选",app->tapX,app->tapY);
    draw_text(&app->screen,600,66,15,app->missTap ? 0xAD4E3D : 0x9C8F7B,label);
}

/* 屏幕坐标换算成缩略格下标。
   说明：六格铺满网格，精确包含几乎总能命中；就近吸附只兜住稍微落在网格外的抬起点，
         比如页眉分隔线上方或最后一列右侧。 */
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

/* 翻页只刷新六张缩略图；大图导航在首尾两端循环 */
void album_event(App *app, Event event)
{
    Album *album = &app->album;
    if (event.type != EVENT_UP) { return; }
    app->missTap = false;
    if (app->page == PAGE_PHOTO) {
        /* 命中区域比画出的按钮更高、更靠下：手指自然落在底边，而面板上报的点
           会略高于实际位置，因此把目标区往下、往高扩。 */
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
            /* 谢东鸿的相册支持点大图本身返回网格，退出大图视图不必先找到按钮。 */
            app->page = PAGE_ALBUM; app->notice[0] = '\0'; image_free(&album->photo);
        } else { app->missTap = true; }
    } else {
        /* 两个翻页按钮位于底边、高 54 像素；命中带更高、起点更靠上，因此手指
           落在最底部仍能被判定。这一行其余位置都属于缩略图网格。 */
        static const Rect pager[2] = {{18,402,124,78},{658,402,124,78}};
        int target = ui_nearest(app,event,pager,2,40);
        if (target == 0) {
            if (album->page) { --album->page; load_page(album); }
        } else if (target == 1) {
            if ((album->page+1)*6 < album->count) { ++album->page; load_page(album); }
        } else {
            /* 按可靠程度依次尝试这次手势产生的三个坐标点。 */
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
