/* Codex-assisted integration, 2026-11-19: four screens, one event owner. */
#ifndef TERMINAL_H
#define TERMINAL_H
#include "display.h"
#include "game.h"
#include <time.h>
#define ALBUM_LIMIT 128
#define PATH_CAPACITY 512
typedef enum { PAGE_HOME, PAGE_ALBUM, PAGE_PHOTO, PAGE_GAME, PAGE_BOARD } Page;
typedef enum { EVENT_NONE, EVENT_DOWN, EVENT_MOVE, EVENT_UP, EVENT_CANCEL, EVENT_QUIT } EventType;
typedef struct { EventType type; int x, y; } Event;
typedef struct {
    char paths[ALBUM_LIMIT][PATH_CAPACITY];
    size_t count, page, selected;
    Image thumbs[6], photo;
    bool limited;
} Album;
typedef struct {
    Image screen, drawing, tiles[12];
    Album album;
    Game game;
    Page page;
    bool running, dirty, pressed, painting;
    int downX, downY, lastX, lastY, radius, color;
    time_t clockSecond;
    char notice[128], saveDir[PATH_CAPACITY];
} App;
/* App owns all image buffers; initialize/close once on the main thread. */
bool app_open(App *app, const char *saveDir);
void app_close(App *app);
void app_event(App *app, Event event);
void app_render(App *app);
void app_tick(App *app);
bool album_scan(App *app);
void album_close(Album *album);
void album_render(App *app);
void album_event(App *app, Event event);
void board_render(App *app);
void board_event(App *app, Event event);
void game_render(App *app);
void game_event(App *app, Event event);
void home_render(App *app);
void ui_header(App *app, const char *title);
void ui_button(App *app, Rect rect, const char *label, bool active);
bool ui_clicked(const App *app, Event event, Rect rect);
void app_home(App *app);
extern const uint32_t UI_INK, UI_MUTED, UI_BG, UI_TEAL;
#endif
