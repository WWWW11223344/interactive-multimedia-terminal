/* Off-screen renderer used to check layout work without the board.
 * Builds the real App, paints each page into app.screen, writes BMP files.
 * Codex, 2026-11-19. */
#include "terminal.h"
#include <stdio.h>
#include <string.h>

static void shot(App *app, Page page, int field, const char *user, const char *pass,
                 const char *confirm, const char *name)
{
    char path[64];
    app->page = page;
    app->login.field = field;
    snprintf(app->login.user, sizeof(app->login.user), "%s", user);
    snprintf(app->login.pass, sizeof(app->login.pass), "%s", pass);
    snprintf(app->login.confirm, sizeof(app->login.confirm), "%s", confirm);
    app->login.message[0] = '\0';
    app->missTap = false;   /* "未命中" 是上一页的残留，不能出现在截图里 */
    app->dirty = true;
    app_render(app);
    snprintf(path, sizeof(path), "shots/%s.bmp", name);
    if (!image_save(path, &app->screen)) { fprintf(stderr, "save failed: %s\n", path); }
    else { fprintf(stderr, "wrote %s\n", path); }
}

int main(void)
{
    App app;
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) {
        fprintf(stderr, "app_open failed\n");
        return 1;
    }
    shot(&app, PAGE_LOGIN, 0, "123", "", "", "login_empty");
    shot(&app, PAGE_LOGIN, 1, "123456", "", "", "login_typing");
    shot(&app, PAGE_REGISTER, 1, "654321", "1234", "", "register");
    shot(&app, PAGE_HOME, 0, "", "", "", "home");
    shot(&app, PAGE_GAMES, 0, "", "", "", "games");
    game_start(&app.game, 7u);
    app_event(&app, (Event){EVENT_DOWN, 496, 258});
    app_event(&app, (Event){EVENT_UP, 496, 258});
    shot(&app, PAGE_2048, 0, "", "", "", "game");
    /* 棋类页面走几手再截图：空盘看不出棋子大小、标记颜色和状态栏排版对不对。 */
    gomoku_start(&app.gomoku, 3u);
    {
        static const int opening[6] = {112, 113, 97, 127, 82, 142};
        for (int i = 0; i < 6; ++i) { gomoku_place(&app.gomoku, opening[i]); }
    }
    shot(&app, PAGE_GOMOKU, 0, "", "", "", "gomoku");
    toe_start(&app.toe);
    toe_place(&app.toe, 0); toe_place(&app.toe, 4);
    toe_place(&app.toe, 1); toe_place(&app.toe, 8);
    shot(&app, PAGE_TOE, 0, "", "", "", "toe");
    chess_start(&app.chess);
    chess_move(&app.chess, 7*9+1, 7*9+4);   /* 炮二平五 */
    chess_move(&app.chess, 0*9+7, 2*9+6);   /* 马二进三 */
    app.chess.sel = 9*9+1;                  /* 选中红马，让可走点也出现在截图里 */
    shot(&app, PAGE_CHESS, 0, "", "", "", "chess");
    if (album_scan(&app)) {
        shot(&app, PAGE_ALBUM, 0, "", "", "", "album");
        /* Tap the first thumbnail through the real event path, so the shot proves the
           hit test as well as the layout (this is the "top row cannot be opened" case). */
        app_event(&app, (Event){EVENT_DOWN, 150, 160});
        app_event(&app, (Event){EVENT_UP, 150, 160});
        fprintf(stderr, "album tap -> page %d, selected %zu\n", (int)app.page, app.album.selected);
        shot(&app, PAGE_PHOTO, 0, "", "", "", "photo");
    }
    shot(&app, PAGE_BOARD, 0, "", "", "", "board");
    app_close(&app);
    return 0;
}
