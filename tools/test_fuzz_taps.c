/* 对每一页进行随机点击扫射：不崩溃、不出现越界的页面，相册为空时也不发生除零。
 * 刷入新版本前的一道廉价保险。
 * Codex, 2026-11-21. */
#include "terminal.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int g_seed = 20261121u;

/* 扫射迟早会点到 保存；给它一个私有目录，以免碰到用户自己的画作，
   并在离开前删掉它写下的内容。 */
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
            /* 把扫射限制在被测试的页面上：否则一次 返回 点击会把后续点击带到主界面，
               从而掩盖覆盖盲区。 */
            app.page = pages[pass];
            int x = next_int(800), y = next_int(480);
            app_event(&app, (Event){EVENT_DOWN, x, y});
            if (next_int(3) == 0) { app_event(&app, (Event){EVENT_MOVE, next_int(800), next_int(480)}); }
            app_event(&app, (Event){EVENT_UP, x, y});
            app_tick(&app);
            if (next_int(40) == 0) { app_render(&app); }
            if (app.login.fxFrame > 0) { app_render(&app); }   /* 动画替换了页面 */
            if (app.page > PAGE_BOARD) { fprintf(stderr, "FAIL page out of range: %d\n", (int)app.page); return 1; }
            /* 退出动画，让后续点击重新落在真实页面上。 */
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
