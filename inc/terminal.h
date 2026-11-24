/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   terminal.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-19
Description: 交互式多媒体终端的总头文件。集中定义页面枚举、触摸事件结构，以及
             登录、相册、棋类等页面所需的数据结构；同时声明各页面的绘制与事件
             入口。其余模块之间不互相包含，接口一律由本文件统一导出，避免头文件
             交叉依赖。
Others:      目标平台 GEC6818，ARMv7-A 内核，屏幕 800x480、32 位色深，静态链接
             musl 以摆脱对板端 glibc 版本的依赖。
Function List:
  1. app_open      初始化应用与全部图像缓冲
  2. app_event     把一次触摸事件分发到当前页面
  3. app_render    绘制当前页面到帧缓冲
  4. ui_*          公共界面控件：页头、按钮、命中判定
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 初版，建立相册、画板、游戏三个功能页与事件分发
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 增加登录页与成功动画；游戏页拆为列表页加四个游戏；
                   注释按华为编程规范改写为中文格式
******************************************************************************/
#ifndef TERMINAL_H
#define TERMINAL_H

#include "display.h"
#include "game.h"
#include "boardgames.h"
#include <time.h>

/* 构建号。登录页右下角与程序启动时的串口首行都会输出它，用于确认板子上跑的
   到底是不是刚拷进去的那一版——拍一张屏幕照片或看一眼串口即可判断，不必猜。 */
#define APP_BUILD "1120q"

/* 相册最多扫描的图片数量，超出部分只提示不加载，避免小内存板上一次分配过多 */
#define ALBUM_LIMIT 128

/* 单个文件路径缓冲的长度。板端路径形如 data/drawings/drawing-20261120-193000.bmp，
   512 字节足够且有富余 */
#define PATH_CAPACITY 512

/* 页面标识。一次只显示一页，事件也只交给当前页处理 */
typedef enum {
    PAGE_LOGIN,      /* 登录页 */
    PAGE_REGISTER,   /* 注册页 */
    PAGE_HOME,       /* 主界面，三个功能入口 */
    PAGE_ALBUM,      /* 电子相册，缩略图浏览 */
    PAGE_PHOTO,      /* 相册内放大查看单张照片 */
    PAGE_GAMES,      /* 小游戏列表，四款游戏入口 */
    PAGE_2048,       /* 2048 小游戏 */
    PAGE_GOMOKU,     /* 五子棋 */
    PAGE_TOE,        /* 井字棋 */
    PAGE_CHESS,      /* 中国象棋 */
    PAGE_BOARD       /* 创意画板 */
} Page;

/* 触摸事件类型。EVENT_NONE 用于超时返回，EVENT_QUIT 由退出按钮或信号触发 */
typedef enum {
    EVENT_NONE,      /* 无事件，轮询超时 */
    EVENT_DOWN,      /* 手指按下 */
    EVENT_MOVE,      /* 手指移动，画板与 2048 会用到 */
    EVENT_UP,        /* 手指抬起，多数页面以此判定点击 */
    EVENT_CANCEL,    /* 手势被取消，例如串口收到中断信号 */
    EVENT_QUIT       /* 请求退出主循环 */
} EventType;

/* 一次触摸事件。坐标一律为屏幕像素，不含触摸屏原始值；原始值需要时通过
   platform_last_raw() 单独索取，这样测试与截图工具只要给三个数就能构造事件，
   不必依赖设备层。 */
typedef struct {
    EventType type;  /* 事件类型 */
    int       x;     /* 屏幕横坐标，0 到 799 */
    int       y;     /* 屏幕纵坐标，0 到 479 */
} Event;

/* 登录与注册页的输入状态。
   账号与密码均限定为 6 位数字，与 GEC6818 上游两个参考项目的做法保持一致；
   文本框只显示不直接编辑，靠数字键盘输入并自动跳格。 */
typedef struct {
    char user[8];        /* 账号，最多 6 位数字，余下留给字符串结束符 */
    char pass[8];        /* 密码，输入满 6 位后自动跳到下一个输入框 */
    char confirm[8];     /* 确认密码，仅注册页使用，登录页忽略 */
    int  field;          /* 当前输入焦点：0 账号，1 密码，2 确认 */
    char message[128];   /* 状态栏提示，空串表示无错误，非空时以红底显示 */
    int  fxFrame;        /* 登录成功动画的帧号：0 空闲，从 1 开始逐帧播放 */
} Login;

/* 电子相册的运行状态。
   缩略图一次只保留 6 张，翻页时重新生成，以控制内存占用；
   大图单独占一份缓冲，与缩略图互不影响。 */
typedef struct {
    char   paths[ALBUM_LIMIT][PATH_CAPACITY]; /* 扫描到的图片完整路径表 */
    size_t count;                             /* 实际扫描到的图片数量 */
    size_t page;                              /* 缩略图当前页号，每页 6 张 */
    size_t selected;                          /* 正在放大查看的图片下标 */
    Image  thumbs[6];                         /* 当前页的缩略图缓冲 */
    Image  photo;                             /* 放大后的整幅图像缓冲 */
    bool   limited;                           /* 图片数超过上限时为真，用于提示 */
} Album;

/* 应用的全部运行时状态。整个程序只有一个实例，由 main 在栈上静态分配。 */
typedef struct {
    Image  screen;        /* 屏幕帧缓冲，800x480，直接提交给 /dev/fb0 */
    Image  drawing;       /* 画板画布，752x286，保存与重绘都以它为准 */
    Image  tiles[12];     /* 2048 的 12 张数字牌，启动时从 res/game 载入 */
    Album  album;         /* 相册状态 */
    Game   game;          /* 2048 的对局状态 */
    /* 三款棋盘游戏各自保存局面：中途返回列表再进来，棋盘不会被清空，
       想重新开局只能按右上角的「重新开始」 */
    Gomoku gomoku;        /* 五子棋 */
    Toe    toe;           /* 井字棋 */
    Chess  chess;         /* 中国象棋 */
    Login  login;         /* 登录页与注册页共用的输入状态 */
    Page   page;          /* 当前显示的页面 */
    bool   running;       /* 主循环是否继续，置假后程序退出 */
    bool   dirty;         /* 有变化需要整帧重绘 */
    bool   pressed;       /* 手指是否处于按下状态 */
    bool   painting;      /* 画板是否正在落笔，此时只推局部矩形 */
    int    downX;         /* 按下时的横坐标，命中判定会参考它 */
    int    downY;         /* 按下时的纵坐标 */
    int    lastX;         /* 上一个采样点的横坐标，画笔画线用 */
    int    lastY;         /* 上一个采样点的纵坐标 */
    int    radius;        /* 画板当前笔刷半径：2 细、3 中、7 粗 */
    int    color;         /* 画板当前颜色在调色板中的下标 */
    /* 最近一次抬起的坐标与它对应的触摸屏原始值。
       屏幕上会把这几个数打印出来，把「按钮没反应」这类主观描述变成可对比的
       数字；rawX/rawY 是换算成像素之前的设备原始值，用于判断量程是否正确。 */
    int    tapX;          /* 最近一次抬起的横坐标 */
    int    tapY;          /* 最近一次抬起的纵坐标 */
    int    rawX;          /* 最近一次采样的触摸屏原始 X */
    int    rawY;          /* 最近一次采样的触摸屏原始 Y */
    /* 手指按下期间最后一个采样点。参考工程的做法是按下与抬起之间的任意采样
       都可触发，因此命中判定会依次尝试抬起点、按下点与这个中间采样点；它与
       lastX/lastY 分开保存，后者被 2048 用来记录滑动的远端。 */
    int    lastSampleX;   /* 按下期间的中间采样横坐标 */
    int    lastSampleY;   /* 按下期间的中间采样纵坐标 */
    int    lastDir;       /* 2048 最近一次有效的滑动方向，用于界面回显，-1 表示无 */
    /* 点击没有命中任何控件时置真，页面会把坐标显示出来，
       省得用户对着屏幕猜为什么没反应 */
    bool   missTap;
    time_t clockSecond;   /* 主界面时钟上次刷新到的秒数，用于每秒重绘一次 */
    /* 增量绘制后发生变化的屏幕区域。主循环只提交这块矩形，
       避免每采集一个笔画点就把整屏 800x480 推一遍 */
    Rect   dirtyArea;
    char   notice[128];   /* 页面级提示文字，例如相册目录不可读 */
    char   saveDir[PATH_CAPACITY]; /* 画板作品的保存目录 */
} App;

/* 图像缓冲全部由 App 持有，必须在主线程上初始化一次、关闭一次 */
bool app_open(App *app, const char *saveDir);
void app_close(App *app);

/* 把一次触摸事件分发到当前页面 */
void app_event(App *app, Event event);

/* 记录与读取最近一次采样的触摸屏原始值。设备层写入、需要显示的页面读取；
   放在核心层是为了让测试与截图工具不链接设备层也能完整驱动界面。 */
void app_note_raw(int rawX, int rawY);
void app_last_raw(int *rawX, int *rawY);

/* 绘制当前页面并处理每秒一次的定时刷新 */
void app_render(App *app);
void app_tick(App *app);

/* 重新扫描相册目录，失败时返回假 */
bool album_scan(App *app);
void album_close(Album *album);

/* 登录模块：首次启动时创建演示账号，之后负责两页的绘制、事件与成功动画 */
void login_init(void);
void login_render(App *app);
void login_event(App *app, Event event);
void login_fx_tick(App *app);

/* 相册模块的两个入口 */
void album_render(App *app);
void album_event(App *app, Event event);

/* 画板模块的两个入口 */
void board_render(App *app);
void board_event(App *app, Event event);

/* 2048 模块：对局逻辑在 game.c，界面在 GameSystem.c */
void game_render(App *app);
void game_event(App *app, Event event);

/* 小游戏列表：四款游戏的入口页 */
void games_render(App *app);
void games_event(App *app, Event event);

/* 五子棋、井字棋、象棋三个棋类页面。规则与电脑走法在 boardgames.c，
   这里只负责画面与输入 */
void gomoku_render(App *app);
void gomoku_event(App *app, Event event);
void toe_render(App *app);
void toe_event(App *app, Event event);
void chess_render(App *app);
void chess_event(App *app, Event event);

/* 主界面 */
void home_render(App *app);

/* 公共界面控件：页头带标题与返回按钮，按钮有常态与高亮两种外观 */
void ui_header(App *app, const char *title);
void ui_header_sized(App *app, const char *title, int size);
void ui_button(App *app, Rect rect, const char *label, bool active);
void ui_button_sized(App *app, Rect rect, const char *label, bool active, int size);

/* 判断一次点击是否落在矩形内。ui_clicked 允许按下点与抬起点任一命中，
   适配触摸屏两个采样点偶有偏差的情况；ui_clicked_strict 要求两点都落在矩形内，
   用于输入框这类误触代价较高的控件。 */
bool ui_clicked(const App *app, Event event, Rect rect);
bool ui_clicked_strict(const App *app, Event event, Rect rect);

/* 在一组矩形中找命中的那个，返回下标，未命中返回 -1。先做精确包含判定，
   再退化为找半径内最近的矩形中心——调色板这类小色块间距很窄，
   触摸屏的纵向误差会把点击丢进两个色块中间。 */
int ui_nearest(const App *app, Event event, const Rect *rects, int count, int radius);

/* 按钮样式：0 次要操作，1 主要操作（青色填充），2 数字键盘按键（白底） */
void ui_button_style(App *app, Rect rect, const char *label, int style, int size);

/* 页面跳转。从单个游戏返回时回到游戏列表而不是主界面，因此两者分开 */
void app_home(App *app);
void app_games(App *app);

/* 全局配色，全部界面统一取自这四个常量 */
extern const uint32_t UI_INK, UI_MUTED, UI_BG, UI_TEAL;

#endif
