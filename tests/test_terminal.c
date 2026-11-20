/* Codex, 2026-11-19: regression tests for inherited merge, image-boundary and UI integration defects. */
#include "terminal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define process_id _getpid
#define remove_dir _rmdir
#else
#include <unistd.h>
#define process_id getpid
#define remove_dir rmdir
#endif

/* Pixel mass detects tile loss, duplication and spawns after unchanged moves. */
static unsigned int tile_mass(const Game *game)
{
    unsigned int sum = 0;
    for (int i = 0; i < 16; ++i) { sum += game->cells[i] ? 1u << game->cells[i] : 0; }
    return sum;
}

/* Verify one merge per input tile, all directions, no-op and terminal states. */
static void test_game(void)
{
    Game game = {.cells={1,1,1,1}, .started=true};
    assert(game_slide(&game,GAME_LEFT));
    assert(game.cells[0] == 2 && game.cells[1] == 2 && game.cells[2] == 0 && game.score == 8);
    game = (Game){.cells={1,1,2,0},.started=true};
    assert(game_slide(&game,GAME_LEFT));
    assert(game.cells[0] == 2 && game.cells[1] == 2 && game.score == 4);
    game = (Game){.cells={1,1,2,0},.started=true};
    assert(game_slide(&game,GAME_RIGHT));
    assert(game.cells[2] == 2 && game.cells[3] == 2 && game.score == 4);
    game = (Game){.started=true}; game.cells[0] = 1; game.cells[8] = 1;
    assert(game_slide(&game,GAME_DOWN)); assert(game.cells[12] == 2 && tile_mass(&game) == 4);
    game = (Game){.started=true}; game.cells[4] = 2; game.cells[12] = 2;
    assert(game_slide(&game,GAME_UP)); assert(game.cells[0] == 3 && tile_mass(&game) == 8);
    game = (Game){.cells={1,0,0,0},.started=true,.seed=123};
    Game old = game; assert(!game_move(&game,GAME_LEFT)); assert(memcmp(&old,&game,sizeof(game)) == 0);
    game = (Game){.cells={1,2,1,2,2,1,2,1,1,2,1,2,2,1,2,1},.started=true};
    assert(!game_can_move(&game)); assert(!game_move(&game,GAME_UP)); assert(game.over);
    game = (Game){.cells={10,10},.started=true,.seed=123};
    assert(game_move(&game,GAME_LEFT)); assert(game.won && game.cells[0] == 11 && game.score == 2048);
    assert(!game_move(&game,GAME_RIGHT));
    for (unsigned int seed = 1; seed <= 100; ++seed) {
        game_start(&game,seed);
        for (unsigned int step = 0; step < 200; ++step) {
            unsigned int mass = tile_mass(&game);
            bool changed = game_move(&game,(Direction)((step*7+seed)%4));
            unsigned int after = tile_mass(&game);
            assert(changed ? (game.won ? after == mass : after == mass+2 || after == mass+4) : after == mass);
        }
    }
    puts("PASS: 2048 merge/directions/no-op/score/win/loss and 20000 mass checks");
}

/* Round-trip odd-width BMP rows, high colour bytes, malformed headers and truncation. */
static void test_images(const char *directory)
{
    char path[PATH_CAPACITY]; snprintf(path,sizeof(path),"%s/padding.bmp",directory);
    Image original = {0}, loaded = {0};
    assert(image_create(&original,3,2));
    const uint32_t colors[] = {0xFE80FF,0x010203,0xBEEFAA,0xFFFFFFFF,0,0xAABBCC};
    memcpy(original.pixels,colors,sizeof(colors)); original.pixels[3] &= 0xFFFFFF;
    assert(image_save(path,&original)); assert(image_load(path,&loaded));
    assert(loaded.width == 3 && loaded.height == 2 && memcmp(original.pixels,loaded.pixels,sizeof(colors)) == 0);
    image_free(&loaded);
    FILE *file = fopen(path,"r+b"); assert(file);
    unsigned char header[54], rows[24];
    assert(fread(header,1,54,file) == 54 && fread(rows,1,24,file) == 24);
    header[22] = 0xfe; header[23] = header[24] = header[25] = 0xff; /* top-down height -2 */
    rewind(file); assert(fwrite(header,1,54,file) == 54);
    assert(fwrite(rows+12,1,12,file) == 12 && fwrite(rows,1,12,file) == 12); assert(fclose(file) == 0);
    assert(image_load(path,&loaded)); assert(memcmp(original.pixels,loaded.pixels,sizeof(colors)) == 0);
    image_free(&loaded);
    file = fopen(path,"wb"); assert(file); assert(fwrite(header,1,54,file) == 54); fclose(file);
    assert(!image_load(path,&loaded) && !loaded.pixels);
    file = fopen(path,"wb"); assert(file); fputs("not a bitmap",file); fclose(file);
    assert(!image_load(path,&loaded));
    /* A 32-bit high-byte colour must decode identically on signed-char targets. */
    unsigned char bmp32[58] = {'B','M',58,0,0,0,0,0,0,0,54,0,0,0,40,0,0,0,1,0,0,0,
        1,0,0,0,1,0,32,0,0,0,0,0,4,0,0,0};
    bmp32[54] = 0xEF; bmp32[55] = 0xCD; bmp32[56] = 0xAB; bmp32[57] = 0x80;
    file = fopen(path,"wb"); assert(file); assert(fwrite(bmp32,1,sizeof(bmp32),file) == sizeof(bmp32)); fclose(file);
    assert(image_load(path,&loaded) && loaded.pixels[0] == 0xABCDEF); image_free(&loaded);
    assert(!image_create(&loaded,4096,4096)); assert(!image_create(&loaded,-1,2));
    draw_rect(&original,(Rect){-2,-2,3,3},0xABCDEF); assert(original.pixels[0] == 0xABCDEF);
    image_free(&original); remove(path);
    puts("PASS: BMP padding/high bytes/top-down/truncation/invalid dimensions and edge clipping");
}

/* Drive the same press/release boundary as the real touchscreen. */
static void click(App *app, int x, int y)
{
    app_event(app,(Event){EVENT_DOWN,x,y}); app_event(app,(Event){EVENT_UP,x,y});
}

/* Save the actual C-rendered frame for visual review, with no separate mock UI. */
static void screenshot(App *app, const char *name)
{
    char path[PATH_CAPACITY]; snprintf(path,sizeof(path),"docs/screenshots/%s.bmp",name);
    app_render(app); assert(image_save(path,&app->screen));
}

/* The demo account is the one the login screen advertises; appending it keeps any
   account a previous run registered, and credential_ok stops at the first match. */
static void ensure_demo_account(void)
{
    FILE *file = fopen("data/accounts.txt","a");
    if (file) { fputs("123456 123456\n",file); fclose(file); }
}

/* Type 123456 twice on the nine-grid keypad, press 确认, then let the animation run:
   the same path a user takes from power-on to the home screen. */
static void sign_in(App *app)
{
    static const int keys[6] = {0,1,2,3,4,5};
    for (int round = 0; round < 2; ++round) {
        for (int i = 0; i < 6; ++i) {
            click(app,432 + (keys[i] % 3) * 112 + 52, 190 + (keys[i] / 3) * 68 + 30);
        }
    }
    click(app,432 + 2 * 112 + 52, 190 + 3 * 68 + 30);   /* 确认 */
    assert(app->login.fxFrame > 0);
    for (int frame = 0; frame <= 60; ++frame) { app_tick(app); }
    assert(app->page == PAGE_HOME);
}

/* End-to-end save->rescan->thumbnail->photo, plus game/start/restart/navigation. */
static void test_app(const char *directory)
{
    static App app;
    ensure_demo_account();
    assert(app_open(&app,directory)); screenshot(&app,"00-login");
    assert(app.page == PAGE_LOGIN);
    sign_in(&app);
    screenshot(&app,"01-home");
    click(&app,600,260); assert(app.page == PAGE_BOARD);
    click(&app,90,110); assert(app.color == 1);
    click(&app,660,110); assert(app.radius == 7);
    app_event(&app,(Event){EVENT_DOWN,110,350});
    app_event(&app,(Event){EVENT_MOVE,220,220}); app_event(&app,(Event){EVENT_MOVE,330,350});
    app_event(&app,(Event){EVENT_UP,110,350});
    assert(app.drawing.pixels[(350-156)*752+110-24] == 0xCA554D);
    click(&app,235,110); /* green */
    app_event(&app,(Event){EVENT_DOWN,410,350});
    app_event(&app,(Event){EVENT_MOVE,410,225}); app_event(&app,(Event){EVENT_MOVE,590,225});
    app_event(&app,(Event){EVENT_MOVE,590,350}); app_event(&app,(Event){EVENT_UP,410,350});
    click(&app,700,40); assert(strstr(app.notice,"保存成功"));
    click(&app,700,40); assert(strstr(app.notice,"保存成功")); screenshot(&app,"02-whiteboard");
    click(&app,50,40); assert(app.page == PAGE_HOME);
    click(&app,110,250); assert(app.page == PAGE_ALBUM && app.album.count == 8);
    screenshot(&app,"03-album");
    click(&app,100,140); assert(app.page == PAGE_PHOTO && app.album.photo.width == 752);
    assert(memcmp(app.drawing.pixels,app.album.photo.pixels,752*286*sizeof(uint32_t)) == 0);
    screenshot(&app,"04-photo");
    click(&app,270,445); assert(app.album.selected == 7);
    click(&app,640,445); assert(app.album.selected == 0);
    click(&app,70,445); assert(app.page == PAGE_ALBUM);
    click(&app,705,445); assert(app.album.page == 1);
    click(&app,70,445); assert(app.album.page == 0);
    /* 相册 → 小游戏列表（四个入口）→ 2048 */
    click(&app,50,40); click(&app,350,250); assert(app.page == PAGE_GAMES);
    screenshot(&app,"05-games");
    click(&app,212,180); assert(app.page == PAGE_2048);
    click(&app,680,40); assert(app.game.started);
    for (int i = 0; i < 12; ++i) { click(&app,i%2 ? 680 : 490,300); click(&app,496,170); }
    screenshot(&app,"06-game");
    click(&app,680,40); assert(app.game.score == 0 && !app.game.over);
    /* 返回 climbs one level: leaving a game lands on the list, not on the home screen. */
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    /* 五子棋：玩家落天元，电脑必须应手，所以盘上至少两子 */
    click(&app,588,180); assert(app.page == PAGE_GOMOKU);
    click(&app,30+7*25,96+7*25);
    {
        int stones = 0;
        for (int i = 0; i < GOMOKU_CELLS; ++i) { if (app.gomoku.cell[i]) { ++stones; } }
        assert(stones >= 2 && app.gomoku.turn == 1);
    }
    screenshot(&app,"07-gomoku");
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    /* 井字棋：玩家左上角落子，电脑立即应手 */
    click(&app,212,348); assert(app.page == PAGE_TOE);
    click(&app,100,148);                 /* 左上格中心（格心 112px，棋盘 44,92 起） */
    {
        int stones = 0;
        for (int i = 0; i < TOE_CELLS; ++i) { if (app.toe.cell[i]) { ++stones; } }
        assert(stones >= 2);
    }
    screenshot(&app,"08-toe");
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    /* 中国象棋：选中红兵前进一格，轮到黑方 */
    click(&app,588,348); assert(app.page == PAGE_CHESS);
    click(&app,30,100+6*37); assert(app.chess.sel == 6*9);      /* 红兵：格心 30,322 */
    click(&app,30,100+5*37); assert(app.chess.cell[5*9] == 7 && app.chess.side == -1);
    click(&app,30,100+3*37); assert(app.chess.sel == 3*9);      /* 黑卒可选中，过河后才能横走 */
    screenshot(&app,"09-chess");
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    click(&app,50,40); assert(app.page == PAGE_HOME);
    click(&app,600,250); assert(app.page == PAGE_BOARD);
    click(&app,585,40); assert(app.drawing.pixels[0] == 0xFFFFFF);
    click(&app,50,40);
    /* Either endpoint counts, because the panel's press and release samples rarely
       agree: a drag that starts on a card opens it. */
    app_event(&app,(Event){EVENT_DOWN,110,250}); app_event(&app,(Event){EVENT_UP,400,50});
    assert(app.page == PAGE_ALBUM);
    click(&app,50,40); assert(app.page == PAGE_HOME);
    /* A cancelled gesture activates nothing at all. */
    app_event(&app,(Event){EVENT_DOWN,110,250}); app_event(&app,(Event){EVENT_CANCEL,0,0});
    app_event(&app,(Event){EVENT_UP,110,250}); assert(app.page == PAGE_HOME);
    /* Corrupt files and count limits must remain navigable, without overrunning path storage. */
    for (int i = 0; i < 130; ++i) {
        char path[PATH_CAPACITY]; snprintf(path,sizeof(path),"%s/corrupt_%03d.bmp",directory,i);
        FILE *file = fopen(path,"wb"); assert(file); fputs("bad",file); fclose(file);
    }
    assert(album_scan(&app) && app.album.limited && app.album.count == ALBUM_LIMIT);
    app.page = PAGE_ALBUM;
    click(&app,100,140); assert(app.page == PAGE_PHOTO); /* corrupt entry: error or valid drawing */
    app_render(&app); click(&app,520,445); app_render(&app);
    app_close(&app);
    puts("PASS: login/home/album/photo/2048/五子棋/井字棋/象棋/board navigation, brush-colour-clear, "
         "unique saves, album paging, game restart and drag cancellation");
}

/* Test folders are unique to this process; cleanup never touches the user's drawings. */
int main(void)
{
    char directory[128]; snprintf(directory,sizeof(directory),"data/test-%d",(int)process_id());
    static App bootstrap;
    assert(app_open(&bootstrap,directory)); app_close(&bootstrap);
    test_game(); test_images(directory); test_app(directory);
    DIR *dir = opendir(directory); assert(dir);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') { continue; }
        char path[PATH_CAPACITY];
        int length = snprintf(path,sizeof(path),"%s/%s",directory,entry->d_name);
        assert(length > 0 && (size_t)length < sizeof(path)); assert(remove(path) == 0);
    }
    closedir(dir); assert(remove_dir(directory) == 0);
    puts("ALL TESTS PASSED (host software simulation; hardware not exercised)");
    return 0;
}
