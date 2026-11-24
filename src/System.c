/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   System.c
Author:      汪贤云        Version: 1.0.1       Date: 2026-11-19
Description: 应用骨架。负责图像与字体资源的建立和释放、触摸事件的统一入口与按页
             分发、页面跳转，以及所有界面共用的控件（页头、按钮、命中判定）。
             整个程序只有一个事件入口 app_event，任何页面的输入都从这里进入，
             因此不存在两个线程同时改界面的情况。
Others:      触摸命中判定采用「三点容错」：按下点、按下期间的中间采样点、抬起点，
             任意一点落在控件内即算命中。这块面板的按下与抬起不保证报同一个位置，
             只认抬起点会让一部分点击凭空丢失。
Function List:
  1. app_open          建立帧缓冲、画布、字体与数字牌，初始化各页状态
  2. app_close         释放全部资源
  3. app_event         触摸事件的唯一入口，按当前页分发
  4. app_tick          定时刷新，目前驱动主界面时钟与登录成功动画
  5. app_render        按当前页绘制一帧
  6. ui_clicked        宽松命中判定（三点任一命中）
  7. ui_clicked_strict 严格命中判定（按下点与抬起点都需在控件内）
  8. ui_nearest        一组小目标中取最近的一个，用于调色板这类窄间距控件
  9. ui_header(_sized) 统一的页头：返回按钮、标题、分隔线
 10. ui_button(_style) 统一的按钮外观与文字居中
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 建立单事件入口与页面分发，去除原工程的多线程结构
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 命中判定改为三点容错；返回键按页面分两级；注释改为中文
******************************************************************************/
#include "terminal.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

/* 全局配色。墨色用于正文，灰绿用于次要说明，米白为页面底色，
   青色为强调色（主要按钮、当前选项、图例） */
const uint32_t UI_INK = 0x203B3C;
const uint32_t UI_MUTED = 0x657878;
const uint32_t UI_BG = 0xF5F2EB;
const uint32_t UI_TEAL = 0x227C77;

/* 最近一次采样对应的面板原始值。设备层每个 SYN_REPORT 写入一次，
   需要显示「这个点到底从哪来」的页面读取它 */
static int g_lastRawX, g_lastRawY;

void app_note_raw(int rawX, int rawY) { g_lastRawX = rawX; g_lastRawY = rawY; }
void app_last_raw(int *rawX, int *rawY) { *rawX = g_lastRawX; *rawY = g_lastRawY; }

/* 确保目录存在。已经存在时不再创建，返回它确实是目录；
   权限不足导致后续写入失败时，会在保存那一处报出来。 */
static bool create_directory(const char *path)
{
    struct stat info;

    if (stat(path, &info) == 0) { return S_ISDIR(info.st_mode); }
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

/* 初始化应用。
   参数：app 为唯一实例；saveDir 为画板作品的保存目录。
   返回：全部资源就绪返回真；任何一步失败都会先释放已分配的部分再返回假，
         不留半初始化的状态。 */
bool app_open(App *app, const char *saveDir)
{
    /* 目录路径必须能完整放进结构体字段，否则后面的 snprintf 会被截断 */
    if (!app || !saveDir || strlen(saveDir) >= sizeof(app->saveDir)) { return false; }

    /* 结构体整体清零，之后所有字段都处于确定的初始值；
       页面枚举、棋盘状态等都在下面显式赋值，不依赖清零结果 */
    *app = (App){0};

    /* 屏幕帧缓冲、画板画布、中文字体，三者缺一不可 */
    if (!image_create(&app->screen, SCREEN_WIDTH, SCREEN_HEIGHT) ||
        !image_create(&app->drawing, 752, 286) || !text_open("res/simfang.ttf")) {
        app_close(app);
        return false;
    }

    if (!create_directory("data") || !create_directory(saveDir)) {
        fprintf(stderr, "Cannot create save directory: %s\n", saveDir);
        app_close(app);
        return false;
    }
    snprintf(app->saveDir, sizeof(app->saveDir), "%s", saveDir);

    /* 画布初始为纯白，第一次进入画板看到的就是空白纸 */
    draw_rect(&app->drawing, (Rect){0, 0, 752, 286}, 0xFFFFFF);

    /* 默认笔刷取中等粗细，粗一号细一号由画板工具栏切换 */
    app->radius = 3;

    /* 2048 的 12 张数字牌预先载入内存，滑动时不再读文件 */
    for (unsigned int i = 0; i < 12; ++i) {
        char path[64];

        snprintf(path, sizeof(path), "res/game/%u.bmp", i);
        if (!image_load(path, &app->tiles[i])) {
            fprintf(stderr, "Missing game tile: %s\n", path);
            app_close(app);
            return false;
        }
    }

    /* 首次运行时创建演示账号；已有账号文件则只补缺失的那一个 */
    login_init();

    /* 三款棋类必须显式开局：结构体已被清零，若直接使用，五子棋会把「最后一手」
       标记画在第一个点上，象棋会把第 0 格的棋子当成已选中 */
    gomoku_start(&app->gomoku, (uint32_t)time(NULL));
    toe_start(&app->toe);
    chess_start(&app->chess);

    /* 2048 尚无有效滑动方向，用 -1 表示 */
    app->lastDir = -1;

    /* 程序从登录页开始，主循环立即运行并请求第一帧 */
    app->page = PAGE_LOGIN;
    app->running = true;
    app->dirty = true;

    /* 串口首行打印构建号，与屏幕右下角的标记对应，便于确认板上是哪一版 */
    fprintf(stderr, "terminal build %s\n", APP_BUILD);
    return true;
}

/* 释放资源。程序不使用线程，因此不存在等待线程退出的问题；
   任何一块图像缓冲允许为空，重复释放也安全。 */
void app_close(App *app)
{
    if (!app) { return; }
    album_close(&app->album);
    for (unsigned int i = 0; i < 12; ++i) { image_free(&app->tiles[i]); }
    image_free(&app->drawing);
    image_free(&app->screen);
    text_close();
    app->running = false;
}

/* 判断三个候选点中的一个是否落在矩形内。
   参数：which 为 0 取抬起点，1 取按下期间的中间采样点，2 取按下点。 */
static bool point_hit(const App *app, Event event, Rect rect, int which)
{
    if (which == 0) { return rect_has(rect, event.x, event.y); }
    if (which == 1) { return rect_has(rect, app->lastSampleX, app->lastSampleY); }
    return rect_has(rect, app->downX, app->downY);
}

/* 宽松命中判定：按下点、中间采样点、抬起点任意一个落在控件内即算点击。
   上游两个参考工程分别采用「比对按下与抬起」和「持续试探当前位置」两种做法，
   而这颗触摸芯片并不保证两点报同一位置，因此这里三点都认。 */
bool ui_clicked(const App *app, Event event, Rect rect)
{
    if (!app->pressed || event.type != EVENT_UP) { return false; }
    for (int which = 0; which < 3; ++which) {
        if (point_hit(app, event, rect, which)) { return true; }
    }
    return false;
}

/* 严格命中判定：按下点与抬起点都必须落在矩形内。
   用于画板这类带画布的页面——一笔恰好收在按钮上时，不应该顺手触发那个按钮。 */
bool ui_clicked_strict(const App *app, Event event, Rect rect)
{
    return app->pressed && event.type == EVENT_UP &&
           rect_has(rect, event.x, event.y) &&
           rect_has(rect, app->downX, app->downY);
}

/* 在一组矩形中找命中项，返回下标，未命中返回 -1。
   参数：rects 为矩形数组；count 为个数；radius 为就近判定的搜索半径（像素）。
   说明：工具栏把若干小目标排成一行，正是这块面板最不擅长的形状——一个
         42 像素宽的色块比驱动报告的纵向误差还窄，点偏一点就落进缝里什么
         都不发生。因此先做精确包含判定，未命中再退化为「半径内最近的矩形
         中心」，并依次用抬起点、中间点、按下点各试一轮。半径取间距的一半
         左右时，这一行里的每一点都能落到离手指最近的那个目标上。 */
int ui_nearest(const App *app, Event event, const Rect *rects, int count, int radius)
{
    if (!app->pressed || event.type != EVENT_UP) { return -1; }

    /* 第一轮：精确包含，任一候选点命中即返回 */
    for (int i = 0; i < count; ++i) {
        for (int which = 0; which < 3; ++which) {
            if (point_hit(app, event, rects[i], which)) { return i; }
        }
    }

    /* 第二轮：就近取中心，三个候选点按「抬起、中间、按下」的顺序各试一次 */
    int limit = radius * radius;
    for (int pass = 0; pass < 3; ++pass) {
        int x = pass == 0 ? event.x : pass == 1 ? app->lastSampleX : app->downX;
        int y = pass == 0 ? event.y : pass == 1 ? app->lastSampleY : app->downY;
        int best = -1;
        int bestDistance = limit;

        for (int i = 0; i < count; ++i) {
            int dx = x - (rects[i].x + rects[i].w / 2);
            int dy = y - (rects[i].y + rects[i].h / 2);
            int distance = dx * dx + dy * dy;

            if (distance < bestDistance) { bestDistance = distance; best = i; }
        }
        if (best >= 0) { return best; }
    }
    return -1;
}

/* 绘制一个按钮。
   参数：rect 为区域；label 为文字；style 为样式；size 为初始字号。
   说明：触摸目标比视觉尺寸略大——这颗面板的触点重心常落在视觉上沿下方
         20 到 60 像素处，页头按钮需要额外高度才容易点中。文字水平居中，
         放不下时自动缩小一号。
   style 取值：0 次要操作（米色底），1 主要操作（青色底白字），2 键盘按键（纯白底）。 */
void ui_button_style(App *app, Rect rect, const char *label, int style, int size)
{
    if (style == 1) {
        draw_rect(&app->screen, rect, UI_TEAL);
    } else {
        /* 数字键保持纯白，次要操作用暖米色填充，这样底部操作行不会被误认成
           又多了一排按键 */
        draw_rect(&app->screen, rect, style == 2 ? 0xFFFFFF : 0xF0EBDF);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, rect.w, 1}, 0xDCD6C9);
        draw_rect(&app->screen, (Rect){rect.x, rect.y + rect.h - 1, rect.w, 1}, 0xDCD6C9);
        draw_rect(&app->screen, (Rect){rect.x, rect.y, 1, rect.h}, 0xDCD6C9);
        draw_rect(&app->screen, (Rect){rect.x + rect.w - 1, rect.y, 1, rect.h}, 0xDCD6C9);
    }

    /* 文字放不下时缩小一号再排，仍放不下就靠左留 6 像素边距 */
    int width = text_width(label, size);
    if (width > rect.w - 16) {
        size -= 4;
        width = text_width(label, size);
    }
    int tx = rect.x + (rect.w - width) / 2;
    if (tx < rect.x + 6) { tx = rect.x + 6; }
    draw_text(&app->screen, tx, rect.y + (rect.h - size) / 2 + 1, size,
        style == 1 ? 0xFFFFFF : UI_INK, label);
}

/* 指定字号的按钮，active 为真表示主要操作 */
void ui_button_sized(App *app, Rect rect, const char *label, bool active, int size)
{
    ui_button_style(app, rect, label, active ? 1 : 0, size);
}

/* 默认 24 号字的按钮 */
void ui_button(App *app, Rect rect, const char *label, bool active)
{
    ui_button_style(app, rect, label, active ? 1 : 0, 24);
}

/* 绘制页头：铺底色、放返回按钮、写标题、画分隔线。
   参数：size 为标题字号，留出这个参数是因为游戏页标题右侧还有一段状态文字，
         需要标题窄一些才不挤。 */
void ui_header_sized(App *app, const char *title, int size)
{
    draw_rect(&app->screen, (Rect){0, 0, 800, 480}, UI_BG);
    ui_button(app, (Rect){20, 16, 132, 60}, "返回", false);
    draw_text(&app->screen, 168, 24 + (32 - size) / 2, size, UI_INK, title);
    draw_rect(&app->screen, (Rect){24, 82, 752, 1}, 0xD8DED7);
}

/* 默认 32 号字的页头 */
void ui_header(App *app, const char *title)
{
    ui_header_sized(app, title, 32);
}

/* 回到主界面。画布内容与对局状态都保留，只释放相册的缩略图缓存。 */
void app_home(App *app)
{
    album_close(&app->album);
    app->page = PAGE_HOME;
    app->painting = false;
    app->dirty = true;
    app->notice[0] = '\0';
}

/* 回到小游戏列表。列表夹在主界面与单个游戏之间，因此返回键要分两级：
   从游戏里返回落在列表，从列表返回才回到主界面。 */
void app_games(App *app)
{
    if (!app) { return; }
    app->page = PAGE_GAMES;
    app->painting = false;
    app->dirty = true;
    app->missTap = false;
}

/* 把页面枚举翻译成可读名称，供日志记录页面切换。
   数组下标与 Page 枚举一一对应，新增页面时必须同步补一项。 */
static const char *page_name(Page page)
{
    static const char *const names[] = {
        "登录", "注册", "主页", "相册", "看图", "游戏列表",
        "2048", "五子棋", "井字棋", "象棋", "画板"
    };
    if (page < 0 || page >= (Page)(sizeof(names) / sizeof(names[0]))) { return "未知"; }
    return names[page];
}

/* 判断某个页面是否属于「单个游戏」。用于决定返回键是回列表还是回主界面。 */
static bool is_board_game(Page page)
{
    return page == PAGE_2048 || page == PAGE_GOMOKU || page == PAGE_TOE || page == PAGE_CHESS;
}

/* 判断返回键是否被点到。画板页用严格判定，其余页面用宽松判定。 */
static bool back_hit(const App *app, Event event)
{
    Rect back = {20, 16, 132, 60};

    return app->page == PAGE_BOARD ? ui_clicked_strict(app, event, back)
                                   : ui_clicked(app, event, back);
}

/* 触摸事件的唯一入口。
   说明：所有页面共用这一处入口，界面数据只被一个主循环修改，
         因此不需要加锁，也不会出现切换页面瞬间的穿透点击。 */
void app_event(App *app, Event event)
{
    if (!app || !app->running) { return; }

    /* 事件处理前的页面。结尾与当前页比较，有变化就记一条切换日志，
       这样所有页面的跳转都集中在这里记录，不必在每个页面里重复埋点 */
    Page pageBefore = app->page;

    /* 退出请求：由主循环检测 running 后结束 */
    if (event.type == EVENT_QUIT) { app->running = false; return; }

    /* 手势取消：清掉按下与落笔状态，不派发给页面，避免半截笔画留在画布上 */
    if (event.type == EVENT_CANCEL) {
        app->pressed = false;
        app->painting = false;
        return;
    }
    if (event.type == EVENT_NONE) { return; }

    /* 按下：记录起点，同时把它作为「中间采样点」的初值。
       手指原地点击时不会产生移动事件，若不给初值，中间点会停留在上一次抬起的位置 */
    if (event.type == EVENT_DOWN) {
        app->pressed = true;
        app->downX = event.x;
        app->downY = event.y;
        app->lastSampleX = event.x;
        app->lastSampleY = event.y;
    }

    /* 移动：只更新中间采样点，实际绘制由各页面在 MOVE 分支里完成 */
    if (event.type == EVENT_MOVE) {
        app->lastSampleX = event.x;
        app->lastSampleY = event.y;
    }

    /* 抬起：记录坐标与原始值，并打印一行可对比的日志。
       命中判定真正用到的三个点和它们背后的面板原始值同时出现，
       一句「按钮没反应」就能变成一组可以比对的数字。 */
    if (event.type == EVENT_UP) {
        app->tapX = event.x;
        app->tapY = event.y;
        app_last_raw(&app->rawX, &app->rawY);
        fprintf(stderr, "tap down=%d,%d mid=%d,%d up=%d,%d raw=%d,%d page=%d\n",
            app->downX, app->downY, app->lastSampleX, app->lastSampleY, event.x, event.y,
            app->rawX, app->rawY, (int)app->page);
    }

    if (!app->pressed) { return; }

    /* 返回键优先处理。注册页的返回落在登录页，不直接回主界面 */
    if (app->page == PAGE_REGISTER && back_hit(app, event)) {
        app->login.message[0] = '\0';
        app->login.pass[0] = '\0';
        app->login.confirm[0] = '\0';
        app->login.field = 0;
        app->page = PAGE_LOGIN;
        app->dirty = true;
    } else if (app->page != PAGE_HOME && app->page != PAGE_LOGIN &&
               app->page != PAGE_REGISTER && back_hit(app, event)) {
        /* 单个游戏返回列表，其余页面返回主界面 */
        if (is_board_game(app->page)) { app_games(app); } else { app_home(app); }
    } else {
        /* 其余事件按当前页面分发 */
        switch (app->page) {
            case PAGE_LOGIN:
            case PAGE_REGISTER:
                login_event(app, event);
                break;
            case PAGE_HOME:
                /* 退出按钮 */
                if (ui_clicked(app, event, (Rect){648, 16, 132, 60})) { app->running = false; }
                /* 三张入口卡片：依次为相册、小游戏列表、画板。
                   点击卡片上下沿都算命中，卡片中部恰好落在触摸失效带里 */
                for (int i = 0; i < 3; ++i) {
                    if (ui_clicked(app, event, (Rect){32 + i * 253, 158, 230, 236})) {
                        app->page = i == 0 ? PAGE_ALBUM : i == 1 ? PAGE_GAMES : PAGE_BOARD;
                        app->notice[0] = '\0';
                        app->missTap = false;
                        /* 只有相册需要先扫描目录，失败时给出提示而不是空列表 */
                        if (i == 0 && !album_scan(app)) {
                            snprintf(app->notice, sizeof(app->notice), "相册目录无法读取");
                        }
                        app->dirty = true;
                        break;
                    }
                }
                break;
            case PAGE_ALBUM:
            case PAGE_PHOTO:
                album_event(app, event);
                break;
            case PAGE_GAMES:
                games_event(app, event);
                break;
            case PAGE_2048:
                game_event(app, event);
                break;
            case PAGE_GOMOKU:
                gomoku_event(app, event);
                break;
            case PAGE_TOE:
                toe_event(app, event);
                break;
            case PAGE_CHESS:
                chess_event(app, event);
                break;
            case PAGE_BOARD:
                board_event(app, event);
                break;
            default:
                break;
        }
    }

    /* 一次点击结束，清掉按下与落笔状态，等待下一次按下 */
    if (event.type == EVENT_UP) {
        app->pressed = false;
        app->painting = false;
    }

    /* 页面切换日志：只记真正变化的事件，避免每次采样都写一行把日志刷满 */
    if (app->page != pageBefore) {
        log_write(LOG_INFO, "page: %s -> %s", page_name(pageBefore), page_name(app->page));
    }
}

/* 定时刷新。
   说明：主界面的时钟需要每秒重绘一次，其余页面只在内容变化时重绘。
         登录成功动画由帧计数驱动，也在这里推进。 */
void app_tick(App *app)
{
    time_t now = time(NULL);

    /* 主界面：秒数变了才重绘，避免每轮循环都画一遍时钟 */
    if (app->page == PAGE_HOME && now != app->clockSecond) {
        app->clockSecond = now;
        app->dirty = true;
    }
    login_fx_tick(app);
}

/* 绘制当前页面。
   说明：Linux 实机、Windows 预览与集成测试共用同一套软件渲染，
         因此三者画面完全一致，测试里保存的截图就是用户看到的画面。 */
void app_render(App *app)
{
    switch (app->page) {
        case PAGE_LOGIN:
        case PAGE_REGISTER:
            login_render(app);
            break;
        case PAGE_HOME:
            home_render(app);
            break;
        case PAGE_ALBUM:
        case PAGE_PHOTO:
            album_render(app);
            break;
        case PAGE_GAMES:
            games_render(app);
            break;
        case PAGE_2048:
            game_render(app);
            break;
        case PAGE_GOMOKU:
            gomoku_render(app);
            break;
        case PAGE_TOE:
            toe_render(app);
            break;
        case PAGE_CHESS:
            chess_render(app);
            break;
        case PAGE_BOARD:
            board_render(app);
            break;
        default:
            break;
    }
    app->dirty = false;
}
