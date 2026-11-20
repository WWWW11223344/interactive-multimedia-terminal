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
    login_init();
    /* Boards are initialised explicitly: App is zero-filled above, and a zeroed
       position would show 五子棋 with its "last move" marker at the first point and
       象棋 with the piece on square 0 already selected. */
    gomoku_start(&app->gomoku, (uint32_t)time(NULL));
    toe_start(&app->toe);
    chess_start(&app->chess);
    app->lastDir = -1;
    app->page = PAGE_LOGIN; app->running = true; app->dirty = true;
    fprintf(stderr, "terminal build %s\n", APP_BUILD);
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

/* Both upstream projects (Xiao Linxiang touch.c / Xie Donghong touch.c) decide a tap
   from the samples taken while the finger is down: Xiao Linxiang compares the press
   point with the release point, Xie Donghong's main loop keeps re-testing the current
   position until one matches a control. The gslx680 does not guarantee that the press
   and the release report the same place - either sample can be a value from elsewhere -
   so a hit from ANY of the three points counts: the release, the last sample seen while
   pressed, and the press itself. */
static bool point_hit(const App *app, Event event, Rect rect, int which)
{
    if (which == 0) { return rect_has(rect, event.x, event.y); }
    if (which == 1) { return rect_has(rect, app->lastSampleX, app->lastSampleY); }
    return rect_has(rect, app->downX, app->downY);
}

bool ui_clicked(const App *app, Event event, Rect rect)
{
    if (!app->pressed || event.type != EVENT_UP) { return false; }
    for (int which = 0; which < 3; ++which) {
        if (point_hit(app, event, rect, which)) { return true; }
    }
    return false;
}

/* Pages with a drawing canvas keep the strict rule, so a stroke that happens to end
   over a control does not fire that control. */
bool ui_clicked_strict(const App *app, Event event, Rect rect)
{
    return app->pressed && event.type == EVENT_UP && rect_has(rect, event.x, event.y) &&
        rect_has(rect, app->downX, app->downY);
}

/* Toolbars put several small targets in one row, which is exactly the shape this panel
   handles worst: a 42px chip is narrower than the vertical error the driver reports, so
   a tap aimed at it lands in the gap and nothing happens. The exact test comes first,
   then the nearest centre within `radius`, trying the release point and then the press
   point. With a radius of about half the step, every point in the row resolves to the
   chip the finger is closest to instead of to nothing. */
int ui_nearest(const App *app, Event event, const Rect *rects, int count, int radius)
{
    if (!app->pressed || event.type != EVENT_UP) { return -1; }
    for (int i = 0; i < count; ++i) {
        for (int which = 0; which < 3; ++which) {
            if (point_hit(app, event, rects[i], which)) { return i; }
        }
    }
    int limit = radius * radius;
    for (int pass = 0; pass < 3; ++pass) {
        int x = pass == 0 ? event.x : pass == 1 ? app->lastSampleX : app->downX;
        int y = pass == 0 ? event.y : pass == 1 ? app->lastSampleY : app->downY;
        int best = -1, bestDistance = limit;
        for (int i = 0; i < count; ++i) {
            int dx = x - (rects[i].x + rects[i].w / 2);
            int dy = y - (rects[i].y + rects[i].h / 2);
            int distance = dx * dx + dy * dy;
            if (distance < bestDistance) { bestDistance = distance; best = i; }
        }
        if (best >= 0) { return best; }
    }
    return -1;
}

/* Touch targets are enlarged for the gslx680 panel: finger centroids land 20-60px
   below the visual top edge, so header buttons need the extra height to register.
   Labels are centred, and shrink one step if they would not fit.
   style: 0 = quiet secondary action, 1 = primary (teal), 2 = keypad key (white). */
void ui_button_style(App *app, Rect rect, const char *label, int style, int size)
{
    if (style == 1) {
        draw_rect(&app->screen, rect, UI_TEAL);
    } else {
        /* Keys stay pure white; secondary actions get a warm fill, so the bottom
           action row can never be mistaken for a third row of keys. */
        draw_rect(&app->screen, rect, style == 2 ? 0xFFFFFF : 0xF0EBDF);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, rect.w, 1}, 0xDCD6C9);
        draw_rect(&app->screen, (Rect){rect.x, rect.y + rect.h - 1, rect.w, 1}, 0xDCD6C9);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, 1, rect.h}, 0xDCD6C9);
        draw_rect(&app->screen, (Rect){rect.x + rect.w - 1, rect.y, 1, rect.h}, 0xDCD6C9);
    }
    int width = text_width(label, size);
    if (width > rect.w - 16) { size -= 4; width = text_width(label, size); }
    int tx = rect.x + (rect.w - width) / 2;
    if (tx < rect.x + 6) { tx = rect.x + 6; }
    draw_text(&app->screen, tx, rect.y + (rect.h - size) / 2 + 1, size,
        style == 1 ? 0xFFFFFF : UI_INK, label);
}

void ui_button_sized(App *app, Rect rect, const char *label, bool active, int size)
{
    ui_button_style(app, rect, label, active ? 1 : 0, size);
}

void ui_button(App *app, Rect rect, const char *label, bool active)
{
    ui_button_style(app, rect, label, active ? 1 : 0, 24);
}

/* A consistent back target is available on all three functional screens. The size
   parameter leaves room for a status strip beside the title on the game screen. */
void ui_header_sized(App *app, const char *title, int size)
{
    draw_rect(&app->screen, (Rect){0,0,800,480}, UI_BG);
    ui_button(app, (Rect){20,16,132,60}, "返回", false);
    draw_text(&app->screen, 168, 24 + (32 - size) / 2, size, UI_INK, title);
    draw_rect(&app->screen, (Rect){24,82,752,1}, 0xD8DED7);
}

void ui_header(App *app, const char *title)
{
    ui_header_sized(app, title, 32);
}

/* Preserve the canvas and game while releasing album thumbnail caches. */
void app_home(App *app)
{
    album_close(&app->album);
    app->page = PAGE_HOME; app->painting = false; app->dirty = true;
    app->notice[0] = '\0';
}

/* The game list sits between the home screen and a single game, so 返回 has to climb
   one level: leaving a game lands on the list, leaving the list lands on home. */
void app_games(App *app)
{
    if (!app) { return; }
    app->page = PAGE_GAMES; app->painting = false; app->dirty = true;
    app->missTap = false;
}

static bool is_board_game(Page page)
{
    return page == PAGE_2048 || page == PAGE_GOMOKU || page == PAGE_TOE || page == PAGE_CHESS;
}

/* The drawing page keeps the strict rule; every other page accepts either endpoint. */
static bool back_hit(const App *app, Event event)
{
    Rect back = {20,16,132,60};
    return app->page == PAGE_BOARD ? ui_clicked_strict(app, event, back)
                                   : ui_clicked(app, event, back);
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
        app->lastSampleX = event.x; app->lastSampleY = event.y;
    }
    if (event.type == EVENT_MOVE) { app->lastSampleX = event.x; app->lastSampleY = event.y; }
    if (event.type == EVENT_UP) {
        app->tapX = event.x; app->tapY = event.y;
        /* One line per tap on the console: the three points the hit test actually uses.
           A "the button does nothing" report becomes a measurement when the press, the
           middle sample and the release can be compared side by side. */
        fprintf(stderr, "tap down=%d,%d mid=%d,%d up=%d,%d page=%d\n",
            app->downX, app->downY, app->lastSampleX, app->lastSampleY, event.x, event.y, (int)app->page);
    }
    if (!app->pressed) { return; }
    /* The register screen goes back to login, not to the home screen. */
    if (app->page == PAGE_REGISTER && back_hit(app, event)) {
        app->login.message[0] = '\0';
        app->login.pass[0] = '\0';
        app->login.confirm[0] = '\0';
        app->login.field = 0;
        app->page = PAGE_LOGIN;
        app->dirty = true;
    } else if (app->page != PAGE_HOME && app->page != PAGE_LOGIN && app->page != PAGE_REGISTER &&
               back_hit(app, event)) {
        if (is_board_game(app->page)) { app_games(app); } else { app_home(app); }
    } else {
        switch (app->page) {
            case PAGE_LOGIN: case PAGE_REGISTER: login_event(app, event); break;
            case PAGE_HOME:
                if (ui_clicked(app, event, (Rect){648,16,132,60})) { app->running = false; }
                for (int i = 0; i < 3; ++i) {
                    if (ui_clicked(app, event, (Rect){32+i*253,158,230,236})) {
                        /* card 1 is the game list, which is where the four games live. */
                        app->page = i == 0 ? PAGE_ALBUM : i == 1 ? PAGE_GAMES : PAGE_BOARD;
                        app->notice[0] = '\0';
                        app->missTap = false;
                        if (i == 0 && !album_scan(app)) {
                            snprintf(app->notice, sizeof(app->notice), "相册目录无法读取");
                        }
                        app->dirty = true;
                        break;
                    }
                }
                break;
            case PAGE_ALBUM: case PAGE_PHOTO: album_event(app, event); break;
            case PAGE_GAMES: games_event(app, event); break;
            case PAGE_2048: game_event(app, event); break;
            case PAGE_GOMOKU: gomoku_event(app, event); break;
            case PAGE_TOE: toe_event(app, event); break;
            case PAGE_CHESS: chess_event(app, event); break;
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
    login_fx_tick(app);
}

/* Render into the same software framebuffer for Linux, preview and integration tests. */
void app_render(App *app)
{
    switch (app->page) {
        case PAGE_LOGIN: case PAGE_REGISTER: login_render(app); break;
        case PAGE_HOME: home_render(app); break;
        case PAGE_ALBUM: case PAGE_PHOTO: album_render(app); break;
        case PAGE_GAMES: games_render(app); break;
        case PAGE_2048: game_render(app); break;
        case PAGE_GOMOKU: gomoku_render(app); break;
        case PAGE_TOE: toe_render(app); break;
        case PAGE_CHESS: chess_render(app); break;
        case PAGE_BOARD: board_render(app); break;
        default: break;
    }
    app->dirty = false;
}
