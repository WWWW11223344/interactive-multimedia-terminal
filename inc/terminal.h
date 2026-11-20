/* Codex-assisted integration, 2026-11-19: four screens, one event owner. */
#ifndef TERMINAL_H
#define TERMINAL_H
#include "display.h"
#include "game.h"
#include "boardgames.h"
#include <time.h>
/* Shown on the login screen and printed at startup: a photo of the screen or one line
   of console output then settles "is this the build I just copied?" without guessing. */
#define APP_BUILD "1120e"
#define ALBUM_LIMIT 128
#define PATH_CAPACITY 512
typedef enum { PAGE_LOGIN, PAGE_REGISTER, PAGE_HOME, PAGE_ALBUM, PAGE_PHOTO,
               PAGE_GAMES, PAGE_2048, PAGE_GOMOKU, PAGE_TOE, PAGE_CHESS, PAGE_BOARD } Page;
typedef enum { EVENT_NONE, EVENT_DOWN, EVENT_MOVE, EVENT_UP, EVENT_CANCEL, EVENT_QUIT } EventType;
typedef struct { EventType type; int x, y; } Event;
/* Numeric credentials only (6 digits), matching the upstream GEC6818 demos. */
typedef struct {
    char user[8], pass[8], confirm[8];
    int field;              /* 0 account, 1 password, 2 confirm */
    char message[128];
    int fxFrame;            /* 0 idle, 1..N plays the success animation */
} Login;
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
    /* Board games keep their own position so leaving and re-entering a game does not
       wipe the board; 重新开始 is the only way to clear one. */
    Gomoku gomoku;
    Toe toe;
    Chess chess;
    Login login;
    Page page;
    bool running, dirty, pressed, painting;
    int downX, downY, lastX, lastY, radius, color;
    /* Last release point, kept for the on-screen coordinate readout that turns a
       "button does not respond" report into a number. */
    int tapX, tapY;
    /* Last sample taken while the finger was down. Xie Donghong's projects act on any
       sample seen between press and release, so a hit may be reported at the press
       point, the release point, or anywhere in between; this keeps that middle sample
       available to the hit test. It is separate from lastX/lastY, which the 2048 page
       uses for the furthest point of a swipe. */
    int lastSampleX, lastSampleY;
    /* Last accepted 2048 direction (Direction value) so the screen can echo it. */
    int lastDir;
    /* Set when a tap matched no control, so the screen can show its coordinates
       instead of leaving the user to guess why nothing happened. */
    bool missTap;
    time_t clockSecond;
    /* Area of the screen that changed after an incremental draw; the loop presents
       only this rectangle instead of pushing 800x480 for every pen sample. */
    Rect dirtyArea;
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
void login_init(void);
void login_render(App *app);
void login_event(App *app, Event event);
void login_fx_tick(App *app);
void album_render(App *app);
void album_event(App *app, Event event);
void board_render(App *app);
void board_event(App *app, Event event);
void game_render(App *app);
void game_event(App *app, Event event);
void games_render(App *app);
void games_event(App *app, Event event);
void gomoku_render(App *app);
void gomoku_event(App *app, Event event);
void toe_render(App *app);
void toe_event(App *app, Event event);
void chess_render(App *app);
void chess_event(App *app, Event event);
void home_render(App *app);
void ui_header(App *app, const char *title);
void ui_header_sized(App *app, const char *title, int size);
void ui_button(App *app, Rect rect, const char *label, bool active);
bool ui_clicked(const App *app, Event event, Rect rect);
bool ui_clicked_strict(const App *app, Event event, Rect rect);
/* Index of the rect that was hit, or -1. Exact hits first (either endpoint), then the
   nearest centre within radius - for compact toolbars the panel's Y error would
   otherwise drop the tap between two chips. */
int ui_nearest(const App *app, Event event, const Rect *rects, int count, int radius);
void ui_button_sized(App *app, Rect rect, const char *label, bool active, int size);
/* style: 0 quiet action, 1 primary (teal), 2 keypad key (white). */
void ui_button_style(App *app, Rect rect, const char *label, int style, int size);
void app_home(App *app);
/* Back from a single game returns to the list, not all the way home. */
void app_games(App *app);
extern const uint32_t UI_INK, UI_MUTED, UI_BG, UI_TEAL;
#endif
