/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   LoginSystem.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 登录与注册页。账号密码均为 6 位数字，靠右侧 3x4 数字键盘输入，输满
             自动跳到下一格；文本框只显示不直接编辑。这块面板的顶部一行按键失灵，
             数字只能用 4~9（以及 0），所以演示账号取 678678 / 678678；若用 123456
             则根本输不出来。账号存放在 data/accounts.txt，每行一条「账号 密码」，
             启动时若演示账号不存在就以追加方式写入，用户注册的账号一律保留。
Others:      键盘命中先做精确矩形判定，再在键盘区内吸附到最近键心，容忍 8 像素的
             间隙与首行偏高的点击；页面还会显示按下点、抬起点与触摸原始值，把校准
             问题从主观描述变成可对比的数字。界面文案与演示账号均保持原样。
Function List:
  1. key_rect         计算第 index 个按键的矩形区域
  2. draw_field       绘制输入框，左侧标签右侧数值，兼容两种底色
  3. draw_keypad      绘制整个数字键盘
  4. draw_tap_readout 显示最近一次手势的按下点、抬起点与原始值
  5. draw_tap_mark    在抬起点画十字，标出映射结果
  6. draw_status      绘制状态条：已输入位数或错误信息
  7. hit_point        把屏幕坐标换算成按键下标，带边缘吸附
  8. hit_key          按键命中：先抬起点，再退回按下点
  9. account_exists   判断账号是否已存在于账号文件
 10. credential_ok    校验账号密码是否匹配账号文件
 11. account_append   把一组账号密码追加到账号文件
 12. login_init       首次启动时创建演示账号
 13. active_buffer    按当前焦点返回正在编辑的输入缓冲
 14. press_key        处理数字键与删除键输入
 15. do_login         执行登录校验并启动成功动画
 16. do_register      执行注册并返回登录页
 17. draw_column_rule 绘制左右两栏之间的分隔线
 18. draw_warning     未命中时显示坐标提示
 19. render_login     绘制登录页
 20. render_register  绘制注册页
 21. render_fx        绘制登录成功动画
 22. login_render     登录、注册与动画的绘制入口
 23. login_event      登录与注册页的事件处理
 24. login_fx_tick    逐帧推进成功动画，结束后跳转主界面
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#include "terminal.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>

/* 账号与密码的最大位数，输入满即自动跳格 */
#define MAX_DIGITS 6
/* 账号文件路径，每行一条「账号 密码」 */
#define ACCOUNT_FILE "data/accounts.txt"
/* 登录成功动画的最后一帧，播完跳到主界面 */
#define FX_LAST_FRAME 58

/* 演示账号必须能在顶部按键失灵的面板上输入，只能用位于其下方的 6、7、8。
   123456 看似是演示账号的常规选择，却恰恰是这块板上输不出来的，故取 678678。 */
#define DEMO_USER "678678"
#define DEMO_PASS "678678"

/* 键盘标签：1 至 9、删除、0、确认 */
static const char *const g_pad[12] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "删除", "0", "确认"
};

/* 键盘几何：经典的 3x4 电话键盘（1 至 9、删除、0、确认）位于屏幕右侧，左侧留出
   输入框、状态条与操作按钮。按键 104x60，间距 8 像素；键盘右边缘落在 760，
   与上方密码框对齐。 */
/* 按键宽度 */
#define KEY_W 104
/* 按键高度 */
#define KEY_H 60
/* 键盘左上角横坐标 */
#define KEY_X0 432
/* 横向步进，等于键宽加间距 */
#define KEY_STEP 112
/* 键盘左上角纵坐标 */
#define KEY_Y0 190
/* 纵向步进，等于键高加间距 */
#define KEY_ROW_STEP 68

/* 计算第 index 个按键的矩形区域，按 3 列 4 行排布 */
static Rect key_rect(int index)
{
    Rect rect;
    rect.x = KEY_X0 + (index % 3) * KEY_STEP;
    rect.y = KEY_Y0 + (index / 3) * KEY_ROW_STEP;
    rect.w = KEY_W;
    rect.h = KEY_H;
    return rect;
}

/* 绘制输入框：左侧标签、右侧数值。在青色页头上表现为白底加聚焦时的青色下划线，
   在米色页面上则保留细边框；同一段代码画两种外观，让两页保持关联。 */
static void draw_field(App *app, Rect rect, const char *label, const char *value,
                       bool active, bool masked, bool onTeal)
{
    char shown[MAX_DIGITS + 1];
    size_t length = strlen(value), i;
    if (length > MAX_DIGITS) { length = MAX_DIGITS; }
    for (i = 0; i < length; ++i) { shown[i] = masked ? '*' : value[i]; }
    shown[length] = '\0';

    draw_rect(&app->screen, rect, 0xFFFFFF);
    if (onTeal) {
        draw_rect(&app->screen, (Rect){rect.x, rect.y + rect.h - 4, rect.w, 4},
            active ? 0xFFFFFF : 0x8CC0B9);
        if (active) { draw_rect(&app->screen, (Rect){rect.x, rect.y + rect.h - 6, rect.w, 2}, UI_TEAL); }
    } else {
        uint32_t border = active ? UI_TEAL : 0xC8CFC8;
        draw_rect(&app->screen, (Rect){rect.x, rect.y, rect.w, 2}, border);
        draw_rect(&app->screen, (Rect){rect.x, rect.y + rect.h - 2, rect.w, 2}, border);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, 2, rect.h}, border);
        draw_rect(&app->screen, (Rect){rect.x + rect.w - 2, rect.y, 2, rect.h}, border);
    }
    draw_text(&app->screen, rect.x + 14, rect.y + (rect.h - 16) / 2, 16,
        active ? UI_TEAL : 0x93A39C, label);
    draw_text(&app->screen, rect.x + 76, rect.y + (rect.h - 26) / 2, 26, UI_INK, shown);
}

/* 绘制整个数字键盘 */
static void draw_keypad(App *app)
{
    for (int i = 0; i < 12; ++i) {
        Rect full = key_rect(i);
        if (i == 9) {
            /* 删除是次要操作：形状与数字键相同，填充色更淡。 */
            draw_rect(&app->screen, full, 0xE6E0D3);
            int width = text_width(g_pad[i], 22);
            draw_text(&app->screen, full.x + (full.w - width) / 2,
                full.y + (full.h - 22) / 2 + 1, 22, UI_MUTED, g_pad[i]);
        } else {
            /* 确认是主要操作；数字是屏幕上字号最大的字形。 */
            ui_button_style(app, full, g_pad[i], i == 11 ? 1 : 2, i == 11 ? 24 : 28);
        }
    }
}

/* 显示最近一次手势：按下点、抬起点以及对应的原始值并排打印，让校准问题可见而
   不必猜测。按错位置和完全没反应从外部看是一样的，这六个数字能把两者区分开。 */
static void draw_tap_readout(App *app, int x, int y)
{
    char line[96];
    snprintf(line, sizeof(line), "down %d,%d  up %d,%d  raw %d,%d",
        app->downX, app->downY, app->tapX, app->tapY, app->rawX, app->rawY);
    draw_text(&app->screen, x, y, 13, 0x8C9A94, line);
}

/* 左栏的反馈条：显示当前焦点字段、已输入位数或最近的错误信息。 */
/* 在抬起点画十字，标出映射结果。上方的 readout 用数字描述同一件事，但十字能让
   我按了 1 却低了一行这种问题在截图里一眼可见，不必反复试。 */
static void draw_tap_mark(App *app)
{
    int x = app->tapX, y = app->tapY;
    if (x <= 0 && y <= 0) { return; }
    draw_rect(&app->screen, (Rect){x - 10, y - 1, 21, 3}, 0xD0342C);
    draw_rect(&app->screen, (Rect){x - 1, y - 10, 3, 21}, 0xD0342C);
}

/* 绘制状态条：有错误时红底显示错误信息，否则显示当前字段已输入的位数 */
static void draw_status(App *app, Rect rect, const char *fieldName, const char *buffer)
{
    char text[96];
    int length = (int)strlen(buffer);
    if (length > MAX_DIGITS) { length = MAX_DIGITS; }

    if (app->login.message[0]) {
        draw_rect(&app->screen, rect, 0xF7E4DF);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, 4, rect.h}, 0xAD4E3D);
        draw_text(&app->screen, rect.x + 16, rect.y + 14, 16, 0xAD4E3D, app->login.message);
    } else {
        draw_rect(&app->screen, rect, 0xE8E1D2);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, 4, rect.h}, UI_TEAL);
        snprintf(text, sizeof(text), "%s   %d / %d 位", fieldName, length, MAX_DIGITS);
        draw_text(&app->screen, rect.x + 16, rect.y + 14, 16, UI_INK, text);
    }
}

/* 键盘区先做精确矩形命中，再对落在键盘区内的点取最近键心。手指落在 8 像素的
   间隙里，或在首行稍微偏上，仍能按到想按的键，这正是「1 没反应」和「1 有反应」
   的区别。键盘区之外返回 -1，左栏按钮因此保有各自的区域。 */
/* 键盘命中区左边界 */
#define PAD_LEFT (KEY_X0 - 8)
/* 键盘命中区右边界 */
#define PAD_RIGHT (KEY_X0 + 2 * KEY_STEP + KEY_W + 8)
/* 键盘命中区上边界 */
#define PAD_TOP (KEY_Y0 - 8)
/* 键盘命中区下边界 */
#define PAD_BOTTOM (KEY_Y0 + 3 * KEY_ROW_STEP + KEY_H + 8)

/* 把屏幕坐标换算成按键下标，未命中返回 -1 */
static int hit_point(int x, int y)
{
    int best = -1;
    int64_t bestDistance = 0;
    for (int i = 0; i < 12; ++i) {
        if (rect_has(key_rect(i), x, y)) { return i; }
    }
    if (x < PAD_LEFT || x > PAD_RIGHT || y < PAD_TOP || y > PAD_BOTTOM) { return -1; }
    for (int i = 0; i < 12; ++i) {
        Rect rect = key_rect(i);
        int64_t dx = x - (rect.x + rect.w / 2);
        int64_t dy = y - (rect.y + rect.h / 2);
        int64_t distance = dx * dx + dy * dy;
        if (best < 0 || distance < bestDistance) { bestDistance = distance; best = i; }
    }
    return best;
}

/* 按键命中：先按抬起点换算，未命中再退回按下点 */
static int hit_key(const App *app, Event event)
{
    int key = hit_point(event.x, event.y);
    if (key < 0) { key = hit_point(app->downX, app->downY); }
    return key;
}

/* 判断账号是否已存在于账号文件中 */
static bool account_exists(const char *user)
{
    FILE *file = fopen(ACCOUNT_FILE, "r");
    char name[32], secret[32];
    bool found = false;
    if (!file) { return false; }
    while (fscanf(file, "%31s %31s", name, secret) == 2) {
        if (strcmp(name, user) == 0) { found = true; break; }
    }
    fclose(file);
    return found;
}

/* 校验账号与密码是否匹配账号文件中的某一行 */
static bool credential_ok(const char *user, const char *pass)
{
    FILE *file = fopen(ACCOUNT_FILE, "r");
    char name[32], secret[32];
    bool ok = false;
    if (!file) { return false; }
    while (fscanf(file, "%31s %31s", name, secret) == 2) {
        if (strcmp(name, user) == 0 && strcmp(secret, pass) == 0) { ok = true; break; }
    }
    fclose(file);
    return ok;
}

/* 把一组账号密码以一行追加到账号文件末尾 */
static bool account_append(const char *user, const char *pass)
{
    FILE *file = fopen(ACCOUNT_FILE, "a");
    if (!file) { return false; }
    fprintf(file, "%s %s\n", user, pass);
    fclose(file);
    return true;
}

/* 创建演示账号，避免板子被锁在登录页外。采用追加而不是重写：data/accounts.txt
   在重新烧录后仍然存在，若整文件重写，旧版本留下的文件只会保留无法在本面板上
   输入的 123456；用户自己注册的账号则一律保留。 */
void login_init(void)
{
    if (!account_exists(DEMO_USER)) { account_append(DEMO_USER, DEMO_PASS); }
}

/* 按当前焦点返回正在编辑的输入缓冲 */
static char *active_buffer(App *app)
{
    if (app->login.field == 0) { return app->login.user; }
    if (app->login.field == 1) { return app->login.pass; }
    return app->login.confirm;
}

/* 处理一次按键：数字键追加字符，删除键退格或退回上一输入框；确认键由调用方处理 */
static void press_key(App *app, int index)
{
    char *buffer = active_buffer(app);
    size_t length = strlen(buffer);
    if (index == 9) {
        /* 先退格；当前框已空时再退回上一输入框，这样不必点文本框也能改错。 */
        if (length > 0) {
            buffer[length - 1] = '\0';
        } else if (app->login.field > 0) {
            --app->login.field;
        }
    } else {
        char digit = (index == 10) ? '0' : (char)('1' + index);
        if (length < MAX_DIGITS) {
            buffer[length] = digit;
            buffer[length + 1] = '\0';
            /* 自动跳格：输满 6 位就把焦点移到下一框。本面板的文本框点不准，
               因此只显示不直接编辑。 */
            if (strlen(buffer) == MAX_DIGITS) {
                if (app->page == PAGE_LOGIN && app->login.field == 0) {
                    app->login.field = 1;
                } else if (app->page == PAGE_REGISTER && app->login.field < 2) {
                    ++app->login.field;
                }
            }
        }
    }
    app->login.message[0] = '\0';
    app->dirty = true;
}

/* 登录校验：账号密码都必须是 6 位数字，匹配成功则启动成功动画 */
static void do_login(App *app)
{
    Login *login = &app->login;
    if (strlen(login->user) != MAX_DIGITS || strlen(login->pass) != MAX_DIGITS) {
        snprintf(login->message, sizeof(login->message), "账号和密码都必须是 6 位数字");
        app->dirty = true;
        return;
    }
    if (credential_ok(login->user, login->pass)) {
        /* 只记账号，不记密码——日志文件可能被拷走，不能留下明文口令 */
        log_write(LOG_INFO, "login succeeded: user=%s", login->user);
        login->message[0] = '\0';
        login->fxFrame = 1;
    } else {
        log_write(LOG_WARN, "login failed: user=%s", login->user);
        snprintf(login->message, sizeof(login->message), "账号或密码错误，请重新输入");
    }
    app->dirty = true;
}

/* 注册：校验三项均为 6 位、两次密码一致且账号未占用，成功后返回登录页 */
static void do_register(App *app)
{
    Login *login = &app->login;
    if (strlen(login->user) != MAX_DIGITS || strlen(login->pass) != MAX_DIGITS ||
        strlen(login->confirm) != MAX_DIGITS) {
        snprintf(login->message, sizeof(login->message), "三项都要填满，每项 6 位数字");
        app->dirty = true;
        return;
    }
    if (strcmp(login->pass, login->confirm) != 0) {
        snprintf(login->message, sizeof(login->message), "两次输入的密码不一致");
        app->dirty = true;
        return;
    }
    if (account_exists(login->user)) {
        snprintf(login->message, sizeof(login->message), "该账号已存在，请换一个");
        app->dirty = true;
        return;
    }
    if (!account_append(login->user, login->pass)) {
        log_write(LOG_ERROR, "register failed: cannot write account file, user=%s", login->user);
        snprintf(login->message, sizeof(login->message), "注册失败，无法写入账号文件");
        app->dirty = true;
        return;
    }
    log_write(LOG_INFO, "register succeeded: user=%s", login->user);
    login->field = 1;
    login->pass[0] = '\0';
    login->confirm[0] = '\0';
    snprintf(login->message, sizeof(login->message), "注册成功，请输入密码登录");
    app->page = PAGE_LOGIN;
    app->dirty = true;
}

/* 键盘在右、表单与操作在左，中间用一条细线分隔，让页面读起来是两栏而不是一堆
   方框。登录与注册两页共用同一套几何。 */
/* 左右分栏竖线的横坐标 */
#define COLUMN_RULE_X 416

/* 绘制左右两栏之间的竖向分隔线 */
static void draw_column_rule(App *app)
{
    draw_rect(&app->screen, (Rect){COLUMN_RULE_X, 190, 1, 274}, 0xE0DBCE);
}

/* 仅在点击未命中任何控件时显示：坐标是区分命中框画偏与触摸上报错位的最快线索。 */
static void draw_warning(App *app, int x, int y)
{
    char line[64];
    if (!app->missTap) { return; }
    snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
    draw_text(&app->screen, x, y, 15, 0xAD4E3D, line);
}

/* 绘制登录页：青色页头、两栏布局、数字键盘与演示账号提示 */
static void render_login(App *app)
{
    Login *login = &app->login;
    draw_rect(&app->screen, (Rect){0, 0, 800, 480}, UI_BG);
    /* 页头：标题、一行提示与两个输入框都放在上面。 */
    draw_rect(&app->screen, (Rect){0, 0, 800, 188}, UI_TEAL);
    draw_rect(&app->screen, (Rect){0, 188, 800, 4}, 0x1A5F5B);
    draw_text(&app->screen, 40, 20, 34, 0xFFFFFF, "交互式多媒体终端");
    draw_text(&app->screen, 40, 64, 15, 0xBAD9D5, "点右侧数字键输入，满 6 位自动跳下一格");

    draw_field(app, (Rect){40, 100, 356, 58}, "账号", login->user, login->field == 0, false, true);
    draw_field(app, (Rect){404, 100, 356, 58}, "密码", login->pass, login->field == 1, true, true);

    draw_keypad(app);
    draw_column_rule(app);

    draw_status(app, (Rect){40, 190, 360, 46},
        login->field == 0 ? "账号" : "密码",
        login->field == 0 ? login->user : login->pass);

    ui_button(app, (Rect){40, 250, 360, 60}, "登 录", true);
    ui_button(app, (Rect){40, 322, 360, 60}, "注 册", false);
    draw_text(&app->screen, 40, 396, 15, UI_MUTED, "演示账号 " DEMO_USER " / " DEMO_PASS);
    draw_tap_readout(app, 40, 420);
    draw_warning(app, 40, 444);
    draw_tap_mark(app);
    /* 构建号：标明正在运行的是哪一版程序，任何截图里都能看到。 */
    draw_text(&app->screen, 706, 460, 13, 0x9C8F7B, "build " APP_BUILD);
}

/* 绘制注册页：三个输入框、数字键盘与输入规则说明 */
static void render_register(App *app)
{
    Login *login = &app->login;
    ui_header(app, "注册新账号");
    draw_field(app, (Rect){40, 96, 232, 50}, "账号", login->user, login->field == 0, false, false);
    draw_field(app, (Rect){284, 96, 232, 50}, "密码", login->pass, login->field == 1, true, false);
    draw_field(app, (Rect){528, 96, 232, 50}, "确认", login->confirm, login->field == 2, true, false);

    draw_keypad(app);
    draw_column_rule(app);

    if (login->field == 2) { draw_status(app, (Rect){40, 190, 360, 46}, "确认", login->confirm); }
    else if (login->field == 1) { draw_status(app, (Rect){40, 190, 360, 46}, "密码", login->pass); }
    else { draw_status(app, (Rect){40, 190, 360, 46}, "账号", login->user); }

    ui_button(app, (Rect){40, 250, 360, 60}, "提交注册", true);

    /* 输入规则在操作按钮下方一次说明清楚。 */
    draw_rect(&app->screen, (Rect){40, 322, 360, 64}, 0xEDE7DA);
    draw_rect(&app->screen, (Rect){40, 322, 4, 64}, UI_TEAL);
    draw_text(&app->screen, 60, 331, 15, UI_INK, "账号与密码均为 6 位数字");
    draw_text(&app->screen, 60, 355, 15, UI_MUTED, "重复账号会被拒绝，注册后可立即登录");
    draw_tap_readout(app, 40, 398);
    draw_warning(app, 40, 422);
    draw_tap_mark(app);
}

/* 登录成功动画：青色色带从中间展开，对勾逐笔画出，随后淡入欢迎语。全部由矩形和
   短线段构成，保证 GEC6818 的软件渲染帧率稳定。 */
static void render_fx(App *app)
{
    int frame = app->login.fxFrame;
    int band = frame * 42;
    if (band > 480) { band = 480; }

    draw_rect(&app->screen, (Rect){0, 0, 800, 480}, UI_BG);
    draw_rect(&app->screen, (Rect){0, (480 - band) / 2, 800, band}, UI_TEAL);

    if (band >= 300) {
        int step = frame - 8;
        int first = step * 10;
        if (first < 0) { first = 0; }
        if (first > 46) { first = 46; }
        draw_line(&app->screen, 352, 244, 352 + first, 244 + first, 6, 0xFFFFFF);
        if (step >= 5) {
            int second = (step - 5) * 10;
            if (second > 88) { second = 88; }
            draw_line(&app->screen, 398, 290, 398 + second, 290 - second, 6, 0xFFFFFF);
        }
    }
    if (frame >= 18) {
        draw_text(&app->screen, 328, 330, 36, 0xFFFFFF, "登录成功");
    }
    if (frame >= 24) {
        char welcome[64];
        snprintf(welcome, sizeof(welcome), "欢迎回来，%s", app->login.user);
        draw_text(&app->screen, 310, 384, 20, 0xE8F5F2, welcome);
    }
}

/* 绘制入口：有动画则画动画，否则按当前页面画登录或注册页 */
void login_render(App *app)
{
    if (app->login.fxFrame > 0) { render_fx(app); return; }
    if (app->page == PAGE_REGISTER) { render_register(app); return; }
    render_login(app);
}

/* 登录与注册页的事件处理：先判文本框与按钮，最后才交给数字键盘 */
void login_event(App *app, Event event)
{
    Login *login = &app->login;
    int key;
    if (login->fxFrame > 0) { return; }
    if (event.type == EVENT_DOWN) { app->dirty = true; }
    if (event.type != EVENT_UP) { return; }

    app->missTap = false;

    if (app->page == PAGE_LOGIN) {
        /* 文本框只显示不直接编辑：自动跳格移动焦点，空框按删除退回上一格。它们仍会
           响应明确的点击，但要求按下与抬起两点一致，因此在页头上稍微抬高的数字键
           点击不会被键盘上方的输入框吃掉。 */
        if (ui_clicked_strict(app, event, (Rect){40, 100, 356, 58})) {
            login->field = 0; app->dirty = true; return;
        }
        if (ui_clicked_strict(app, event, (Rect){404, 100, 356, 58})) {
            login->field = 1; app->dirty = true; return;
        }
        /* 操作按钮有 6 像素余量，右边缘止于 x=400，离最近的键心还有 92 像素，
           因此下方键盘不会抢走点击。 */
        if (ui_clicked(app, event, (Rect){40, 244, 360, 68})) { do_login(app); return; }
        if (ui_clicked(app, event, (Rect){40, 316, 360, 72})) {
            login->user[0] = '\0';
            login->pass[0] = '\0';
            login->confirm[0] = '\0';
            login->field = 0;
            login->message[0] = '\0';
            app->page = PAGE_REGISTER;
            app->dirty = true;
            return;
        }
        key = hit_key(app, event);
        if (key >= 0) {
            if (key == 11) { do_login(app); } else { press_key(app, key); }
            return;
        }
        app->missTap = true;
        app->dirty = true;
        return;
    }

    if (app->page == PAGE_REGISTER) {
        if (ui_clicked_strict(app, event, (Rect){40, 96, 232, 50})) {
            login->field = 0; app->dirty = true; return;
        }
        if (ui_clicked_strict(app, event, (Rect){284, 96, 232, 50})) {
            login->field = 1; app->dirty = true; return;
        }
        if (ui_clicked_strict(app, event, (Rect){528, 96, 232, 50})) {
            login->field = 2; app->dirty = true; return;
        }
        if (ui_clicked(app, event, (Rect){40, 244, 360, 68})) { do_register(app); return; }
        key = hit_key(app, event);
        if (key >= 0) {
            if (key == 11) { do_register(app); } else { press_key(app, key); }
            return;
        }
        app->missTap = true;
        app->dirty = true;
    }
}

/* 由 app_tick 调用，逐帧推进成功动画；播完后清空输入并跳到主界面。
   没有动画运行时直接返回。 */
void login_fx_tick(App *app)
{
    if (app->login.fxFrame <= 0) { return; }
    ++app->login.fxFrame;
    if (app->login.fxFrame > FX_LAST_FRAME) {
        app->login.fxFrame = 0;
        app->login.user[0] = '\0';
        app->login.pass[0] = '\0';
        app->login.confirm[0] = '\0';
        app->login.field = 0;
        app->login.message[0] = '\0';
        app->page = PAGE_HOME;
        app->clockSecond = 0;
    }
    app->dirty = true;
}
