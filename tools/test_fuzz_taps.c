/* Randomised tap sweep over every page: no crash, no out-of-range page, no division by
 * zero when the album is empty. Cheap insurance before flashing a new build.
 * Codex, 2026-11-21. */
#include "terminal.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int g_seed = 20261121u;

/* The sweep will hit 保存 sooner or later; give it a private directory so the user's
   own drawings are never touched, and delete what it wrote before leaving. */
#define FUZZ_DIR "data/test-fuzz"

static void clear_fuzz_dir(void)
{
    DIR *dir = opendir(FUZZ_DIR);
    if (!dir) { return; }
    struct dirent *entry;
    char path[PATH_CAPACITY];
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') { continue; }
        snprintf(path, sizeof(path), "%s/%s", FUZZ_DIR, entry->d_name);
        remove(path);
    }
    closedir(dir);
}

static int next_int(int limit)
{
    g_seed ^= g_seed << 13; g_seed ^= g_seed >> 17; g_seed ^= g_seed << 5;
    return (int)(g_seed % (unsigned int)limit);
}

int main(void)
{
    App app;
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, FUZZ_DIR)) { fprintf(stderr, "app_open failed\n"); return 2; }
    clear_fuzz_dir();
    const Page pages[11] = {PAGE_LOGIN, PAGE_REGISTER, PAGE_HOME, PAGE_ALBUM, PAGE_PHOTO,
                            PAGE_GAMES, PAGE_2048, PAGE_GOMOKU, PAGE_TOE, PAGE_CHESS, PAGE_BOARD};
    for (int pass = 0; pass < 11; ++pass) {
        if (pass == 3) { album_scan(&app); }
        app.page = pages[pass];
        app.dirty = true;
        for (int i = 0; i < 600; ++i) {
            /* Keep the sweep on the page under test: a 返回 tap would otherwise move
               the remaining taps to the home screen and hide coverage gaps. */
            app.page = pages[pass];
            int x = next_int(800), y = next_int(480);
            app_event(&app, (Event){EVENT_DOWN, x, y});
            if (next_int(3) == 0) { app_event(&app, (Event){EVENT_MOVE, next_int(800), next_int(480)}); }
            app_event(&app, (Event){EVENT_UP, x, y});
            app_tick(&app);
            if (next_int(40) == 0) { app_render(&app); }
            if (app.login.fxFrame > 0) { app_render(&app); }   /* the animation replaces the page */
            if (app.page > PAGE_BOARD) { fprintf(stderr, "FAIL page out of range: %d\n", (int)app.page); return 1; }
            /* Leave the animation so the next taps land on a real page again. */
            if (app.login.fxFrame > 55) { for (int f = 0; f < 8; ++f) { app_tick(&app); } }
            if (app.page == PAGE_PHOTO && app.album.count == 0) { app.page = PAGE_ALBUM; }
        }
        app_render(&app);
        fprintf(stderr, "ok   %d taps on page %d\n", 600, (int)pages[pass]);
    }
    app_close(&app);
    clear_fuzz_dir();
    fprintf(stderr, "all checks passed\n");
    return 0;
}
