/* Login and registration screens with a numeric keypad, plus a success animation.
 * Added 2026-11-19 for the GEC6818 demo. Accounts live in data/accounts.txt,
 * one "user pass" pair per line, matching the numeric-only flow of the two
 * upstream projects (Xiao Linxiang register.c / Xie Donghong LoginAndRegisterSystem.c).
 */
#include "terminal.h"
#include <stdio.h>
#include <string.h>

#define MAX_DIGITS 6
#define ACCOUNT_FILE "data/accounts.txt"
#define FX_LAST_FRAME 58

/* Keypad legend: 1-9, delete, 0, confirm. */
static const char *const g_pad[12] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "删除", "0", "确认"
};

/* Keypad geometry: the classic 3x4 phone pad (1-9, delete, 0, confirm) on the right
   side of the screen, leaving a column for the fields, the status strip and the
   actions. Keys are 104x60 with an 8px gutter; the block's right edge lands on 760
   so it lines up with the password box above it. */
#define KEY_W 104
#define KEY_H 60
#define KEY_X0 432
#define KEY_STEP 112
#define KEY_Y0 190
#define KEY_ROW_STEP 68

static Rect key_rect(int index)
{
    Rect rect;
    rect.x = KEY_X0 + (index % 3) * KEY_STEP;
    rect.y = KEY_Y0 + (index / 3) * KEY_ROW_STEP;
    rect.w = KEY_W;
    rect.h = KEY_H;
    return rect;
}

/* Label on the left, value on the right. On the teal header band the field is a
   plain white plate with a teal underline when focused; on the beige pages it keeps
   a hairline frame. Same code, two surfaces, so the two screens stay related. */
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

static void draw_keypad(App *app)
{
    for (int i = 0; i < 12; ++i) {
        Rect full = key_rect(i);
        if (i == 9) {
            /* 删除 is a quiet action: same shape as a digit, muted fill. */
            draw_rect(&app->screen, full, 0xE6E0D3);
            int width = text_width(g_pad[i], 22);
            draw_text(&app->screen, full.x + (full.w - width) / 2,
                full.y + (full.h - 22) / 2 + 1, 22, UI_MUTED, g_pad[i]);
        } else {
            /* 确认 is the primary action; digits are the biggest glyphs on screen. */
            ui_button_style(app, full, g_pad[i], i == 11 ? 1 : 2, i == 11 ? 24 : 28);
        }
    }
}

/* Last gesture, shown on screen so a calibration problem is visible instead of
   guessed: the press point and the release point are printed side by side. */
static void draw_tap_readout(App *app, int x, int y)
{
    char line[80];
    snprintf(line, sizeof(line), "down %d,%d   up %d,%d", app->downX, app->downY, app->tapX, app->tapY);
    draw_text(&app->screen, x, y, 14, 0x8C9A94, line);
}

/* Feedback strip in the left column: which field is live, how many digits are in,
   or the last error message. */
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

/* Exact rectangle first; if nothing matches, take the nearest key centre within
   a small radius. Both the release point and the press point are tried, because
   the panel does not always report them at the same place. */
static int hit_point(int x, int y)
{
    for (int i = 0; i < 12; ++i) {
        if (rect_has(key_rect(i), x, y)) { return i; }
    }
    int best = -1;
    int64_t bestDistance = 40LL * 40LL;
    for (int i = 0; i < 12; ++i) {
        Rect rect = key_rect(i);
        int64_t dx = x - (rect.x + rect.w / 2);
        int64_t dy = y - (rect.y + rect.h / 2);
        int64_t distance = dx * dx + dy * dy;
        if (distance < bestDistance) { bestDistance = distance; best = i; }
    }
    return best;
}

static int hit_key(const App *app, Event event)
{
    int key = hit_point(event.x, event.y);
    if (key < 0) { key = hit_point(app->downX, app->downY); }
    return key;
}

/* Create a demo account on first boot so the board is not locked out. */
void login_init(void)
{
    FILE *file = fopen(ACCOUNT_FILE, "r");
    if (file) { fclose(file); return; }
    file = fopen(ACCOUNT_FILE, "w");
    if (!file) { return; }
    fprintf(file, "123456 123456\n");
    fclose(file);
}

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

static bool account_append(const char *user, const char *pass)
{
    FILE *file = fopen(ACCOUNT_FILE, "a");
    if (!file) { return false; }
    fprintf(file, "%s %s\n", user, pass);
    fclose(file);
    return true;
}

static char *active_buffer(App *app)
{
    if (app->login.field == 0) { return app->login.user; }
    if (app->login.field == 1) { return app->login.pass; }
    return app->login.confirm;
}

static void press_key(App *app, int index)
{
    char *buffer = active_buffer(app);
    size_t length = strlen(buffer);
    if (index == 9) {
        /* Backspace first, then step back to the previous field once empty, so a
           mistake can be corrected without touching the text boxes. */
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
            /* Auto-advance: a full field moves focus to the next one. The text boxes
               on this panel cannot be tapped reliably, so they are display-only. */
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

static void do_login(App *app)
{
    Login *login = &app->login;
    if (strlen(login->user) != MAX_DIGITS || strlen(login->pass) != MAX_DIGITS) {
        snprintf(login->message, sizeof(login->message), "账号和密码都必须是 6 位数字");
        app->dirty = true;
        return;
    }
    if (credential_ok(login->user, login->pass)) {
        login->message[0] = '\0';
        login->fxFrame = 1;
    } else {
        snprintf(login->message, sizeof(login->message), "账号或密码错误，请重新输入");
    }
    app->dirty = true;
}

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
        snprintf(login->message, sizeof(login->message), "注册失败，无法写入账号文件");
        app->dirty = true;
        return;
    }
    login->field = 1;
    login->pass[0] = '\0';
    login->confirm[0] = '\0';
    snprintf(login->message, sizeof(login->message), "注册成功，请输入密码登录");
    app->page = PAGE_LOGIN;
    app->dirty = true;
}

/* The pad sits on the right, the form and the actions on the left; a hairline
   separates the two zones so the page reads as two columns instead of a pile of
   boxes. Both listing screens use the same geometry. */
#define COLUMN_RULE_X 416

static void draw_column_rule(App *app)
{
    draw_rect(&app->screen, (Rect){COLUMN_RULE_X, 190, 1, 274}, 0xE0DBCE);
}

/* Shown only when a tap matched nothing: the coordinates are the fastest way to
   tell a mis-aligned hit box from a panel that reports somewhere else entirely. */
static void draw_warning(App *app, int x, int y)
{
    char line[64];
    if (!app->missTap) { return; }
    snprintf(line, sizeof(line), "未命中 %d,%d", app->tapX, app->tapY);
    draw_text(&app->screen, x, y, 15, 0xAD4E3D, line);
}

static void render_login(App *app)
{
    Login *login = &app->login;
    draw_rect(&app->screen, (Rect){0, 0, 800, 480}, UI_BG);
    /* Hero band: title, one-line hint, and the two fields sit on it. */
    draw_rect(&app->screen, (Rect){0, 0, 800, 188}, UI_TEAL);
    draw_rect(&app->screen, (Rect){0, 188, 800, 4}, 0x1A5F5B);
    draw_text(&app->screen, 40, 20, 34, 0xFFFFFF, "触控休闲娱乐终端");
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
    draw_text(&app->screen, 40, 396, 15, UI_MUTED, "演示账号 123456 / 123456");
    draw_tap_readout(app, 40, 420);
    draw_warning(app, 40, 444);
    /* Build stamp: proof of which binary is running, visible in any screenshot. */
    draw_text(&app->screen, 706, 460, 13, 0x9C8F7B, "build " APP_BUILD);
}

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

    /* The rules, stated once, under the action. */
    draw_rect(&app->screen, (Rect){40, 322, 360, 64}, 0xEDE7DA);
    draw_rect(&app->screen, (Rect){40, 322, 4, 64}, UI_TEAL);
    draw_text(&app->screen, 60, 331, 15, UI_INK, "账号与密码均为 6 位数字");
    draw_text(&app->screen, 60, 355, 15, UI_MUTED, "重复账号会被拒绝，注册后可立即登录");
    draw_tap_readout(app, 40, 398);
    draw_warning(app, 40, 422);
}

/* Success animation: a teal band opens from the middle, a check mark draws itself,
 * then the greeting fades in. All shapes are rectangles and short lines so the
 * GEC6818 software renderer keeps a steady frame rate. */
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

void login_render(App *app)
{
    if (app->login.fxFrame > 0) { render_fx(app); return; }
    if (app->page == PAGE_REGISTER) { render_register(app); return; }
    render_login(app);
}

void login_event(App *app, Event event)
{
    Login *login = &app->login;
    int key;
    if (login->fxFrame > 0) { return; }
    if (event.type == EVENT_DOWN) { app->dirty = true; }
    if (event.type != EVENT_UP) { return; }

    app->missTap = false;

    if (app->page == PAGE_LOGIN) {
        /* The text boxes are display-only: auto-advance moves the caret, and 删除 on
           an empty field steps back one. They still answer a deliberate tap, but only
           when the press and the release agree, so a digit lifted slightly high over
           the hero band can never be eaten by the box above the pad. */
        if (ui_clicked_strict(app, event, (Rect){40, 100, 356, 58})) {
            login->field = 0; app->dirty = true; return;
        }
        if (ui_clicked_strict(app, event, (Rect){404, 100, 356, 58})) {
            login->field = 1; app->dirty = true; return;
        }
        /* Actions carry 6px of slack; they end at x=400, 92px clear of the nearest
           key centre, so the pad below can never steal them. */
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

/* Called from app_tick; returns silently when no animation is running. */
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
