/* Xie Donghong's screen dispatcher adapted by Codex, 2026-11-19.
 * Remove login/media and worker threads: one loop owns UI, clock, input and buffers.
 */
#include "terminal.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
const uint32_t UI_INK = 0x203B3C, UI_MUTED = 0x657878, UI_BG = 0xF5F2EB, UI_TEAL = 0x227C77;

/* Require a writable-directory location; permission errors surface at save time. */
static bool create_directory(const char *path)
{
    struct stat info;
    if (stat(path, &info) == 0) { return S_ISDIR(info.st_mode); }
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

/* All allocations are owned by App and rolled back through app_close on failure. */
bool app_open(App *app, const char *saveDir)
{
    if (!app || !saveDir || strlen(saveDir) >= sizeof(app->saveDir)) { return false; }
    *app = (App){0};
    if (!image_create(&app->screen, SCREEN_WIDTH, SCREEN_HEIGHT) ||
        !image_create(&app->drawing, 752, 286) || !text_open("res/simfang.ttf")) {
        app_close(app); return false;
    }
    if (!create_directory("data") || !create_directory(saveDir)) {
        fprintf(stderr, "Cannot create save directory: %s\n", saveDir);
        app_close(app); return false;
    }
    snprintf(app->saveDir, sizeof(app->saveDir), "%s", saveDir);
    draw_rect(&app->drawing, (Rect){0,0,752,286}, 0xFFFFFF);
    app->radius = 3;
    for (unsigned int i = 0; i < 12; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "res/game/%u.bmp", i);
        if (!image_load(path, &app->tiles[i])) {
            fprintf(stderr, "Missing game tile: %s\n", path);
            app_close(app); return false;
        }
    }
    app->page = PAGE_HOME; app->running = true; app->dirty = true;
    return true;
}

/* No threads survive cleanup; every optional image can be safely empty. */
void app_close(App *app)
{
    if (!app) { return; }
    album_close(&app->album);
    for (unsigned int i = 0; i < 12; ++i) { image_free(&app->tiles[i]); }
    image_free(&app->drawing); image_free(&app->screen); text_close();
    app->running = false;
}

/* Each button activates only when both endpoints are inside it. */
bool ui_clicked(const App *app, Event event, Rect rect)
{
    return app->pressed && event.type == EVENT_UP && rect_has(rect, event.x, event.y) &&
        rect_has(rect, app->downX, app->downY);
}

/* Shared large touch targets fit the 800x480 screen. */
void ui_button(App *app, Rect rect, const char *label, bool active)
{
    draw_rect(&app->screen, rect, active ? UI_TEAL : 0xE6EBE6);
    draw_text(&app->screen, rect.x+12, rect.y+11, 24, active ? 0xFFFFFF : UI_INK, label);
}

/* A consistent back target is available on all three functional screens. */
void ui_header(App *app, const char *title)
{
    draw_rect(&app->screen, (Rect){0,0,800,480}, UI_BG);
    ui_button(app, (Rect){24,18,88,44}, "返回", false);
    draw_text(&app->screen, 132, 20, 32, UI_INK, title);
    draw_rect(&app->screen, (Rect){24,77,752,1}, 0xD8DED7);
}

/* Preserve the canvas and game while releasing album thumbnail caches. */
void app_home(App *app)
{
    album_close(&app->album);
    app->page = PAGE_HOME; app->painting = false; app->dirty = true;
    app->notice[0] = '\0';
}

/* One event owner prevents input/UI data races and click-through on screen changes. */
void app_event(App *app, Event event)
{
    if (!app || !app->running) { return; }
    if (event.type == EVENT_QUIT) { app->running = false; return; }
    if (event.type == EVENT_CANCEL) { app->pressed = false; app->painting = false; return; }
    if (event.type == EVENT_NONE) { return; }
    if (event.type == EVENT_DOWN) {
        app->pressed = true; app->downX = event.x; app->downY = event.y;
    }
    if (!app->pressed) { return; }
    if (app->page != PAGE_HOME && ui_clicked(app, event, (Rect){24,18,88,44})) {
        app_home(app);
    } else {
        switch (app->page) {
            case PAGE_HOME:
                if (ui_clicked(app, event, (Rect){680,22,96,44})) { app->running = false; }
                for (int i = 0; i < 3; ++i) {
                    if (ui_clicked(app, event, (Rect){32+i*253,158,230,236})) {
                        app->page = i == 0 ? PAGE_ALBUM : i == 1 ? PAGE_GAME : PAGE_BOARD;
                        app->notice[0] = '\0';
                        if (i == 0 && !album_scan(app)) {
                            snprintf(app->notice, sizeof(app->notice), "相册目录无法读取");
                        }
                        app->dirty = true;
                        break;
                    }
                }
                break;
            case PAGE_ALBUM: case PAGE_PHOTO: album_event(app, event); break;
            case PAGE_GAME: game_event(app, event); break;
            case PAGE_BOARD: board_event(app, event); break;
            default: break;
        }
    }
    if (event.type == EVENT_UP) { app->pressed = false; app->painting = false; }
}

/* Clock refresh needs no worker thread; other screens redraw only after changes. */
void app_tick(App *app)
{
    time_t now = time(NULL);
    if (app->page == PAGE_HOME && now != app->clockSecond) {
        app->clockSecond = now; app->dirty = true;
    }
}

/* Render into the same software framebuffer for Linux, preview and integration tests. */
void app_render(App *app)
{
    switch (app->page) {
        case PAGE_HOME: home_render(app); break;
        case PAGE_ALBUM: case PAGE_PHOTO: album_render(app); break;
        case PAGE_GAME: game_render(app); break;
        case PAGE_BOARD: board_render(app); break;
        default: break;
    }
    app->dirty = false;
}
