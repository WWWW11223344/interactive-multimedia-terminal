/* 登录流程的无界面检查：无窗口、无硬件，只检查状态转换。
 *
 * 按键中心坐标来自 LoginSystem.c 中的 KEY_X0 / KEY_Y0 / KEY_STEP / KEY_ROW_STEP。
 * 它们曾经按屏幕左侧的键盘硬编码；后来键盘移到了右侧，而本文件在整个版本周期里
 * 一直点在空白处，由于从未注册到 ctest，还打印出了错误结果。现在它使用断言，并已注册。
 *
 * 覆盖两条流程：123456（六个不同的键，覆盖每一列和两行）以及演示账号 678678，
 * 后者存在是为了能在顶部键盘行不显示任何内容的开发板上输入登录信息。
 */
#include "terminal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void click(App *app, int x, int y)
{
    app_event(app, (Event){EVENT_DOWN, x, y});
    app_event(app, (Event){EVENT_UP, x, y});
}

/* 索引：0-8 = 1-9，9 = 删除，10 = 0，11 = 确认。 */
static void key(App *app, int index)
{
    click(app, 432 + (index % 3) * 112 + 52, 190 + (index / 3) * 68 + 30);
}

static void type_keys(App *app, const int *keys, int count)
{
    for (int i = 0; i < count; ++i) { key(app, keys[i]); }
}

static void clear_fields(App *app)
{
    app->login.user[0] = '\0';
    app->login.pass[0] = '\0';
    app->login.confirm[0] = '\0';
    app->login.field = 0;
    app->login.message[0] = '\0';
    app->login.fxFrame = 0;
    app->page = PAGE_LOGIN;
}

/* 123456 账号在此追加，使本测试不依赖其他测试先运行；若文件中已存在则跳过，
   以免重复运行时不断追加。 */
static void ensure_account(void)
{
    static char text[8192];
    size_t length = 0;
    FILE *file = fopen("data/accounts.txt", "r");
    if (file) { length = fread(text, 1, sizeof(text) - 1, file); fclose(file); }
    text[length] = '\0';
    if (strstr(text, "123456 123456")) { return; }
    file = fopen("data/accounts.txt", "a");
    if (file) { fputs("123456 123456\n", file); fclose(file); }
}

int main(void)
{
    static App app;
    static const int digits[6] = {0, 1, 2, 3, 4, 5};   /* 1 2 3 4 5 6 */
    static const int demo[6] = {5, 6, 7, 5, 6, 7};     /* 6 7 8 6 7 8 */

    ensure_account();
    if (!app_open(&app, "data/test-login")) { printf("OPEN FAIL\n"); return 1; }
    assert(app.page == PAGE_LOGIN);
    assert(strcmp(app.login.user, "") == 0);

    /* 六位数字填满账号输入框，第六位会自动把光标移到下一栏：
       该界面的文本框仅用于显示。 */
    type_keys(&app, digits, 6);
    assert(strcmp(app.login.user, "123456") == 0);
    assert(app.login.field == 1);

    type_keys(&app, digits, 6);
    assert(strcmp(app.login.pass, "123456") == 0);

    key(&app, 11);
    assert(app.login.fxFrame > 0);
    assert(app.login.message[0] == '\0');

    for (int frame = 0; frame <= 60; ++frame) { app_tick(&app); }
    assert(app.page == PAGE_HOME);

    /* 密码错误时会停留在登录界面并给出原因。 */
    clear_fields(&app);
    type_keys(&app, digits, 6);
    type_keys(&app, digits, 6);
    app.login.pass[5] = '9';
    key(&app, 11);
    assert(app.page == PAGE_LOGIN);
    assert(app.login.fxFrame == 0);
    assert(strlen(app.login.message) > 0);

    /* 登录界面宣传的账号必须真的能登录。它只由 6/7/8 组成，
       因此即使在 1/2/3 那一行不显示任何内容的开发板上也能输入。 */
    clear_fields(&app);
    type_keys(&app, demo, 6);
    assert(strcmp(app.login.user, "678678") == 0);
    type_keys(&app, demo, 6);
    key(&app, 11);
    assert(app.login.fxFrame > 0);

    app_close(&app);
    puts("login flow: ok");
    return 0;
}
