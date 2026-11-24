/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   main.c
Author:      汪贤云        Version: 1.0.1       Date: 2026-11-19
Description: 程序入口。负责解析命令行参数、初始化图像与设备资源、加载触摸标定
             记录，然后进入单一主循环。
             主循环的节奏是：取一个事件 -> 把同一批里剩余的事件全部吃掉 -> 每秒
             一次的定时刷新 -> 按需提交整帧或局部矩形。把一批事件一次吃完，是为了
             避免画一笔就重绘一帧时队列越积越长，导致笔迹明显滞后于手指。
Others:      命令行参数按用途分为四类：设备节点覆盖、坐标变换、标定控制、
             面板诊断。诊断类参数（--probe / --sweep / --log-touch / --trace-abs）
             只读不写，不会改动标定记录与用户数据。
Function List:
  1. main           解析参数、初始化资源、驱动主循环
  2. stop_signal    信号处理，只置停止标志，不做任何其他动作
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 建立单循环结构，支持设备节点覆盖与坐标变换
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 增加 --probe / --sweep / --log-touch 三个面板诊断开关；
                   增加 --skip-login 演示入口；注释按规范改为中文
******************************************************************************/
#include "platform.h"
#include "touchprobe.h"
#include "touchsweep.h"
#include "logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define change_dir _chdir
#else
#include <unistd.h>
#define change_dir chdir
#endif

/* 收到退出信号后置真的标志。用 sig_atomic_t 是因为信号处理函数可能在任意指令
   边界打断主循环，普通变量在这个位置被写入的结果没有保证。 */
static volatile sig_atomic_t g_stopping;

/* 信号处理函数内绝不触碰图形、堆内存或设备描述符：printf 与 free 都不是
   可重入的，在主循环任意位置被打断时调用它们会死锁。这里只置一个标志，
   真正的收尾工作交给主循环之后的正常流程。 */
static void stop_signal(int signalNumber)
{
    (void)signalNumber;
    g_stopping = 1;
}

/* 程序入口。
   参数：argc/argv 为命令行参数，含义见 --help 输出。
   返回：0 正常退出；1 资源或设备初始化失败；2 参数错误。 */
int main(int argc, char **argv)
{
    /* 设备节点与保存目录的默认值。板端固定为 /dev/fb0 与 /dev/input/event0，
       换板或改用别的输入设备时由命令行覆盖，不必重新编译 */
    const char *fbPath = "/dev/fb0";
    const char *touchPath = "/dev/input/event0";
    const char *saveDir = "data/drawings";

    /* 坐标变换开关，用于屏幕方向装反或坐标轴对调的情况 */
    bool swapXY = false;
    bool invertX = false;
    bool invertY = false;

    /* 全局坐标偏移。默认为 0：这块面板除中间一条横带外读数是准的，
       版式只要避开那条带即可，不需要把每一屏整体平移 */
    int xBias = 0;
    int yBias = 0;

    /* 标定控制。五点标定向导只在面板首次使用时才有意义：
       --calibrate 手动重跑，--no-calibrate 在已知面板正常时跳过 */
    bool forceCal = false;
    bool skipCal = false;

    /* 只读诊断：打印驱动公布的每一条 ABS 轴及其量程，并逐条回显采样。
       不做任何换算，因此打印出来的数字直接反映硬件行为 */
    bool traceAbs = false;

    /* 单点探针：把当前原始值画在屏幕上，替代滚动的串口日志 */
    bool probe = false;

    /* 逐带扫描：探针只能显示有采样的位置，一个采样都收不到的横带在屏幕上
       什么都不会动，看不出是「没上报」还是「上报了同一个值」。逐带计数则能
       让空带表现为一根不存在的柱子 */
    bool sweep = false;

    /* 把每一个原始事件写进 data/touch.log。串口会滚屏、照片存不下上千条采样，
       而文件可以原样发回来做离线分析 */
    bool logTouch = false;

    /* 演示备用入口：直接进主界面，不画登录页。
       这块面板在画面中部有一条横带读不出坐标，键盘最上面一行 1/2/3 正好落在
       其中，账号里含这三个数字就无法输入。演示账号已改用 678678 避开该行，
       这个开关留作登录临时出问题时的兜底，不删除登录流程本身 */
    bool skipLogin = false;

    /* ---------------- 命令行解析 ---------------- */
    for (int i = 1; i < argc; ++i) {
        /* 不带值的开关 */
        if (strcmp(argv[i], "--help") == 0) {
            puts("terminal [--root DIR] [--fb DEVICE] [--touch DEVICE] [--data DIR]\n"
                 "         [--swap-xy] [--invert-x] [--invert-y] [--x-bias N] [--y-bias N]\n"
                 "         [--calibrate] [--no-calibrate] [--trace-abs] [--probe] [--sweep]\n"
                 "         [--log-touch] [--skip-login]");
            return 0;
        }
        if (strcmp(argv[i], "--swap-xy") == 0) { swapXY = true; continue; }
        if (strcmp(argv[i], "--invert-x") == 0) { invertX = true; continue; }
        if (strcmp(argv[i], "--invert-y") == 0) { invertY = true; continue; }
        if (strcmp(argv[i], "--calibrate") == 0) { forceCal = true; continue; }
        if (strcmp(argv[i], "--no-calibrate") == 0) { skipCal = true; continue; }
        if (strcmp(argv[i], "--trace-abs") == 0) { traceAbs = true; continue; }
        if (strcmp(argv[i], "--probe") == 0) { probe = true; continue; }
        if (strcmp(argv[i], "--sweep") == 0) { sweep = true; continue; }
        if (strcmp(argv[i], "--log-touch") == 0) { logTouch = true; continue; }
        if (strcmp(argv[i], "--skip-login") == 0) { skipLogin = true; continue; }

        /* 带一个整数参数的开头两项，缺值时明确报错而不是当成 0 处理 */
        if (strcmp(argv[i], "--x-bias") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Missing value: --x-bias\n"); return 2; }
            xBias = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--y-bias") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "Missing value: --y-bias\n"); return 2; }
            yBias = atoi(argv[++i]);
            continue;
        }

        /* 其余都是「选项 + 值」的形式，缺值即视为参数错误 */
        if (i + 1 >= argc) { fprintf(stderr, "Missing argument: %s\n", argv[i]); return 2; }
        const char *option = argv[i];
        const char *value = argv[++i];

        if (strcmp(option, "--fb") == 0) { fbPath = value; }
        else if (strcmp(option, "--touch") == 0) { touchPath = value; }
        else if (strcmp(option, "--data") == 0) { saveDir = value; }
        else if (strcmp(option, "--root") == 0) {
            /* 切换工作目录后，res/ 与 data/ 都相对它解析 */
            if (change_dir(value) != 0) { perror("--root"); return 2; }
        } else {
            fprintf(stderr, "Unknown option: %s\n", option);
            return 2;
        }
    }

    /* ---------------- 资源初始化 ---------------- */
    static App app;
    if (!app_open(&app, saveDir)) {
        fprintf(stderr, "Resource initialization failed\n");
        return 1;
    }

    /* 应用日志在资源与 data 目录就绪之后打开，这样日志文件有地方落盘。
       打开失败不影响程序运行，日志仍会写标准错误（板端即串口）。 */
    log_open("data/app.log");
    if (logTouch) { platform_touch_log("data/touch.log"); }
    log_write(LOG_INFO, "terminal build %s started, fb=%s touch=%s", APP_BUILD, fbPath, touchPath);

    if (!platform_open(fbPath, touchPath, swapXY, invertX, invertY, xBias, yBias)) {
        log_write(LOG_ERROR, "platform_open failed, fb=%s touch=%s", fbPath, touchPath);
        log_close();
        app_close(&app);
        return 1;
    }

    /* 跳过登录必须在第一帧之前生效，否则登录页会先被画出来一次。
       app_open 已经调用过 login_init，账号文件照常创建，因此这个开关不影响
       之后的正常登录流程，也不需要额外清理 */
    if (skipLogin) {
        app.page = PAGE_HOME;
        app.dirty = true;
        fprintf(stderr, "startup: --skip-login, entering the home screen directly\n");
    }

    if (traceAbs) {
        /* 在打开设备之后立即开启，保证轴信息在第一帧之前打印完；
           同时跳过标定向导，否则五次点击会盖掉要观察的采样 */
        platform_trace_abs(true);
    }

    /* ---------------- 标定记录 ---------------- */
    /* 有标定记录时以它为准：驱动声明的量程本身就是导致部分屏幕点不到的嫌疑对象。
       逐带扫描是例外——它的任务恰恰是定位面板上的失效带，若拿一份本身可疑的
       拟合记录当尺子，失效带的位置会被一起搬走。因此 --sweep 只按驱动声明的
       量程工作，那是一次有据可查的换算，而不是五点拟合出来的曲线 */
    TouchCal cal;
    bool haveCal = !sweep && platform_cal_load(&cal);
    if (haveCal) {
        platform_cal_apply(&cal);
        fprintf(stderr, "touch calibration loaded: swap=%d X=%d/%d/%d Y=%d/%d/%d\n",
            cal.swap, cal.rawX[0], cal.rawX[1], cal.rawX[2],
            cal.rawY[0], cal.rawY[1], cal.rawY[2]);
    }

    /* 标定向导默认不跑。逐带扫描实测这块面板在驱动声明的量程下是线性的，
       误差约一个像素，标定没有可改进的余地；反过来，用屏幕下半部两点拟合出来的
       记录会把上半屏拖出可点范围。工具仍然保留，供确实上报异常量程的面板使用 */
    if (!traceAbs && !probe && !sweep && forceCal && !skipCal) {
        touch_calibrate(&app);
    }

    /* 只接管这两个信号：一个来自终端的 Ctrl+C，一个来自 kill 或系统关机 */
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);

    int status = 0;

    /* 诊断画面第一帧先画一次，之后只在新采样到来时重画。
       否则手指按住不动也会每秒重写六十次整屏 */
    bool probeStale = true;

    /* ---------------- 主循环 ---------------- */
    while (app.running && !g_stopping) {
        Event event;

        /* 最多等 16 毫秒，约合 60 帧每秒；没有事件时返回 0 继续下一轮 */
        int ready = platform_poll(&event, 16);
        if (ready < 0) {
            /* 读设备出错通常意味着触摸屏被拔掉或驱动异常，直接结束而不是死循环重试 */
            fprintf(stderr, "Touch device disconnected/read failed\n");
            log_write(LOG_ERROR, "touch device disconnected or read failed");
            status = 1;
            break;
        }

        if (ready > 0) {
            if (probe) { touch_probe_event(&app, event); probeStale = true; }
            else if (sweep) { touch_sweep_event(&app, event); probeStale = true; }
            else { app_event(&app, event); }
        }

        /* 把同一批里剩余的事件全部处理完再重绘。画一笔或滑一次会产生几十个采样，
           若每个采样都重绘一帧，队列会越积越长，笔迹就会明显滞后于手指 */
        while (app.running && !g_stopping && platform_poll(&event, 0) > 0) {
            if (probe) { touch_probe_event(&app, event); probeStale = true; }
            else if (sweep) { touch_sweep_event(&app, event); probeStale = true; }
            else { app_event(&app, event); }
        }

        /* 定时刷新，目前用于主界面的时钟每秒走一次 */
        app_tick(&app);

        if (probe || sweep) {
            /* 诊断画面必须整帧提交：每个采样都会移动标记，只推局部矩形会在屏幕上
               留下上一帧的残影 */
            if (probeStale) {
                if (probe) { touch_probe_draw(&app); } else { touch_sweep_draw(&app); }
                platform_present(&app.screen);
                probeStale = false;
            }
        } else if (app.dirty) {
            /* 页面状态变了，整帧重绘并提交 */
            app_render(&app);
            platform_present(&app.screen);
            app.dirtyArea = (Rect){0, 0, 0, 0};
        } else if (app.dirtyArea.w > 0) {
            /* 笔画已经把新内容画进了帧缓冲，这里只需要把变化的那块矩形推给屏幕 */
            platform_present_area(&app.screen, app.dirtyArea);
            app.dirtyArea = (Rect){0, 0, 0, 0};
        }
    }

    /* 正常退出与信号退出走同一条收尾路径：先记退出原因，再还设备与图像缓冲 */
    log_write(LOG_INFO, "terminal exiting, status=%d signal=%d", status, (int)g_stopping);
    log_close();
    platform_close();
    app_close(&app);
    return status;
}
