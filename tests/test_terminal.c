/* Codex，2026-11-19：针对遗留的合并、图像边界和界面集成缺陷的回归测试。 */
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

/* 像素总量可检测瓦片丢失、重复，以及无效移动后仍生成新块的问题。 */
static unsigned int tile_mass(const Game *game)
{
    unsigned int sum = 0;
    for (int i = 0; i < 16; ++i) { sum += game->cells[i] ? 1u << game->cells[i] : 0; }
    return sum;
}

/* 验证每个输入瓦片只合并一次，以及各方向、无操作和终局状态。 */
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

/* 对奇数宽度 BMP 行、高位颜色字节、畸形头部和截断进行往返测试。 */
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
    header[22] = 0xfe; header[23] = header[24] = header[25] = 0xff; /* 自顶向下，高度为 -2 */
    rewind(file); assert(fwrite(header,1,54,file) == 54);
    assert(fwrite(rows+12,1,12,file) == 12 && fwrite(rows,1,12,file) == 12); assert(fclose(file) == 0);
    assert(image_load(path,&loaded)); assert(memcmp(original.pixels,loaded.pixels,sizeof(colors)) == 0);
    image_free(&loaded);
    file = fopen(path,"wb"); assert(file); assert(fwrite(header,1,54,file) == 54); fclose(file);
    assert(!image_load(path,&loaded) && !loaded.pixels);
    file = fopen(path,"wb"); assert(file); fputs("not a bitmap",file); fclose(file);
    assert(!image_load(path,&loaded));
    /* 32 位高位颜色字节必须在 signed-char 目标平台上解码一致。 */
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

/* 模拟与真实触摸屏相同的按下/抬起边界。 */
static void click(App *app, int x, int y)
{
    app_event(app,(Event){EVENT_DOWN,x,y}); app_event(app,(Event){EVENT_UP,x,y});
}

/* 保存实际由 C 渲染的画面供目视检查，不使用单独的模拟界面。 */
static void screenshot(App *app, const char *name)
{
    char path[PATH_CAPACITY]; snprintf(path,sizeof(path),"docs/screenshots/%s.bmp",name);
    app_render(app); assert(image_save(path,&app->screen));
}

/* 演示账号就是登录界面宣传的那个账号。若文件中已存在则跳过：
   过去每次运行都追加一行，导致 data/accounts.txt 每个测试增加一行，
   累积到四十行完全相同的内容才被人发现。 */
static void ensure_demo_account(void)
{
    static char text[8192];
    size_t length = 0;
    FILE *file = fopen("data/accounts.txt","r");
    if (file) { length = fread(text,1,sizeof(text)-1,file); fclose(file); }
    text[length] = '\0';
    if (strstr(text,"678678 678678")) { return; }
    file = fopen("data/accounts.txt","a");
    if (file) { fputs("678678 678678\n",file); fclose(file); }
}

/* 在九宫格键盘上输入两次演示账号，按下 确认，然后让动画播放完毕：
   这与用户从开机到主界面的操作路径一致。选择 678678 是为了在 1/2/3 那一行
   不显示任何内容的开发板上也能输入；使用 123456 的六个按键路径由 login_flow_tests 覆盖。 */
static void sign_in(App *app)
{
    static const int keys[6] = {5,6,7,5,6,7};
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

/* 端到端测试：保存→重新扫描→缩略图→照片，以及游戏进入/开始/重新开始/导航。 */
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
    click(&app,235,110); /* 绿色 */
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
    click(&app,400,285); assert(app.page == PAGE_2048);
    click(&app,680,40); assert(app.game.started);
    for (int i = 0; i < 12; ++i) { click(&app,i%2 ? 680 : 490,300); click(&app,496,170); }
    screenshot(&app,"06-game");
    click(&app,680,40); assert(app.game.score == 0 && !app.game.over);
    /* 返回 只上升一级：离开游戏后回到列表，而不是主界面。 */
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    /* 五子棋：玩家落天元，电脑必须应手，所以盘上至少两子 */
    click(&app,400,341); assert(app.page == PAGE_GOMOKU);
    click(&app,30+7*25,96+7*25);
    {
        int stones = 0;
        for (int i = 0; i < GOMOKU_CELLS; ++i) { if (app.gomoku.cell[i]) { ++stones; } }
        assert(stones >= 2 && app.gomoku.turn == 1);
    }
    screenshot(&app,"07-gomoku");
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    /* 井字棋：玩家左上角落子，电脑立即应手 */
    click(&app,400,397); assert(app.page == PAGE_TOE);
    click(&app,100,148);                 /* 左上格中心（格心 112px，棋盘 44,92 起） */
    {
        int stones = 0;
        for (int i = 0; i < TOE_CELLS; ++i) { if (app.toe.cell[i]) { ++stones; } }
        assert(stones >= 2);
    }
    screenshot(&app,"08-toe");
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    /* 中国象棋：选中红兵前进一格，轮到黑方 */
    click(&app,400,453); assert(app.page == PAGE_CHESS);
    click(&app,30,100+6*37); assert(app.chess.sel == 6*9);      /* 红兵：格心 30,322 */
    click(&app,30,100+5*37); assert(app.chess.cell[5*9] == 7 && app.chess.side == -1);
    click(&app,30,100+3*37); assert(app.chess.sel == 3*9);      /* 黑卒可选中，过河后才能横走 */
    screenshot(&app,"09-chess");
    click(&app,50,40); assert(app.page == PAGE_GAMES);
    click(&app,50,40); assert(app.page == PAGE_HOME);
    click(&app,600,250); assert(app.page == PAGE_BOARD);
    click(&app,585,40); assert(app.drawing.pixels[0] == 0xFFFFFF);
    click(&app,50,40);
/* 任一端点都算数，因为开发板的按下与抬起采样很少一致：
   从卡片上开始的拖动会打开它。 */
    app_event(&app,(Event){EVENT_DOWN,110,250}); app_event(&app,(Event){EVENT_UP,400,50});
    assert(app.page == PAGE_ALBUM);
    click(&app,50,40); assert(app.page == PAGE_HOME);
    /* 取消的手势不会触发任何操作。 */
    app_event(&app,(Event){EVENT_DOWN,110,250}); app_event(&app,(Event){EVENT_CANCEL,0,0});
    app_event(&app,(Event){EVENT_UP,110,250}); assert(app.page == PAGE_HOME);
    /* 损坏的文件和数量上限下仍须能正常导航，且不能越界写坏路径缓冲区。 */
    for (int i = 0; i < 130; ++i) {
        char path[PATH_CAPACITY]; snprintf(path,sizeof(path),"%s/corrupt_%03d.bmp",directory,i);
        FILE *file = fopen(path,"wb"); assert(file); fputs("bad",file); fclose(file);
    }
    assert(album_scan(&app) && app.album.limited && app.album.count == ALBUM_LIMIT);
    app.page = PAGE_ALBUM;
    click(&app,100,140); assert(app.page == PAGE_PHOTO); /* 损坏条目：报错或显示有效画面 */
    app_render(&app); click(&app,520,445); app_render(&app);
    app_close(&app);
    puts("PASS: login/home/album/photo/2048/五子棋/井字棋/象棋/board navigation, brush-colour-clear, "
         "unique saves, album paging, game restart and drag cancellation");
}

/* 测试文件夹对本进程唯一；清理操作绝不会触及用户的作品。 */
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
