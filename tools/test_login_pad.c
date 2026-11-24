/* 九宫格键盘的无界面检查：每个键都能点到，输入六位数字后输入框会自动前进，
 * 删除 会回退一步，位置略有偏移的点击仍然落在
 * 它原本瞄准的那个键上。Codex, 2026-11-19. */
#include "terminal.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *what)
{
    if (condition) { fprintf(stderr, "ok   %s\n", what); }
    else { fprintf(stderr, "FAIL %s\n", what); ++failures; }
}

static void tap(App *app, int x, int y)
{
    app_event(app, (Event){EVENT_DOWN, x, y});
    app_event(app, (Event){EVENT_UP, x, y});
}

/* 与 LoginSystem.c 中 key_rect() 相同的几何布局：432 + column*112, 190 + row*68。 */
static int key_x(int index) { return 432 + (index % 3) * 112 + 52; }
static int key_y(int index) { return 190 + (index / 3) * 68 + 30; }

static void reset(App *app)
{
    app->page = PAGE_LOGIN;
    app->login.field = 0;
    app->login.user[0] = '\0';
    app->login.pass[0] = '\0';
    app->login.confirm[0] = '\0';
    app->login.message[0] = '\0';
    app->login.fxFrame = 0;
}

int main(void)
{
    App app;
    char what[96];
    memset(&app, 0, sizeof(app));
    if (!app_open(&app, "data/drawings")) { fprintf(stderr, "app_open failed\n"); return 2; }

    /* 数字 1-9 位于键 0-8 上。 */
    for (int i = 0; i < 9; ++i) {
        reset(&app);
        tap(&app, key_x(i), key_y(i));
        snprintf(what, sizeof(what), "key %d types '%c'", i, '1' + i);
        expect(app.login.user[0] == (char)('1' + i) && app.login.user[1] == '\0', what);
    }

    reset(&app);
    tap(&app, key_x(10), key_y(10));
    expect(app.login.user[0] == '0' && app.login.user[1] == '\0', "key 10 (0) types '0'");

    /* 六位数字填满 账号 后，无需点击输入框就把光标移到 密码。 */
    reset(&app);
    for (int i = 0; i < 6; ++i) { tap(&app, key_x(i), key_y(i)); }
    expect(strcmp(app.login.user, "123456") == 0 && app.login.field == 1,
        "six digits fill 账号 and advance to 密码");

    tap(&app, key_x(6), key_y(6));
    tap(&app, key_x(7), key_y(7));
    tap(&app, key_x(8), key_y(8));
    expect(strcmp(app.login.pass, "789") == 0, "the next digits go into 密码");

    tap(&app, key_x(9), key_y(9));
    expect(strcmp(app.login.pass, "78") == 0, "删除 removes the last digit");

    tap(&app, key_x(10), key_y(10));
    expect(strcmp(app.login.pass, "780") == 0, "0 is accepted after 删除");

    /* 在空输入框上按 删除 会回退一个输入框，而不是什么都不做。 */
    reset(&app);
    app.login.field = 1;
    tap(&app, key_x(9), key_y(9));
    expect(app.login.field == 0, "删除 on an empty field steps back to 账号");

    /* 用演示账号按 确认 会启动成功动画。 */
    reset(&app);
    for (int i = 0; i < 6; ++i) { tap(&app, key_x(i), key_y(i)); }
    for (int i = 0; i < 6; ++i) { tap(&app, key_x(i), key_y(i)); }
    expect(app.login.field == 1 && strcmp(app.login.pass, "123456") == 0, "both fields hold 123456");
    tap(&app, key_x(11), key_y(11));
    expect(app.login.fxFrame > 0, "确认 on the demo account logs in");

    /* 面板上报的位置可能偏离视觉上的键几十个像素；吸附半径
       仍必须解析出手指下的那个键。 */
    reset(&app);
    tap(&app, key_x(4), key_y(4) + 26);
    expect(app.login.user[0] == '5', "key 5 still fires with the point 26px low");

    reset(&app);
    tap(&app, key_x(4), key_y(4) - 26);
    expect(app.login.user[0] == '5', "key 5 still fires with the point 26px high");

    /* 左列的 登录 按钮不会被键盘吞掉。 */
    reset(&app);
    strcpy(app.login.user, "123456");
    strcpy(app.login.pass, "123456");
    app.login.field = 1;
    tap(&app, 220, 280);
    expect(app.login.fxFrame > 0, "the 登录 button starts the same animation");

    /* 注册 进入注册界面，随后键盘依次走 账号 -> 密码 -> 确认。 */
    reset(&app);
    tap(&app, 220, 352);
    expect(app.page == PAGE_REGISTER, "注册 opens the register screen");
    for (int i = 0; i < 6; ++i) { tap(&app, key_x(i), key_y(i)); }
    expect(app.login.field == 1 && strcmp(app.login.user, "123456") == 0, "账号 filled first on 注册");
    for (int i = 0; i < 6; ++i) { tap(&app, key_x(i), key_y(i)); }
    expect(app.login.field == 2 && strcmp(app.login.pass, "123456") == 0, "密码 filled second on 注册");
    for (int i = 0; i < 6; ++i) { tap(&app, key_x(i), key_y(i)); }
    expect(app.login.field == 2 && strlen(app.login.confirm) == 6, "确认 takes the last six digits");

    /* 通过顶部的 返回 离开注册界面，该按钮必须存在。 */
    tap(&app, 86, 46);
    expect(app.page == PAGE_LOGIN, "header 返回 leaves the register screen");

    app_close(&app);
    fprintf(stderr, failures ? "%d failure(s)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
