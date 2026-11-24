/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   platform.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 平台后端实现，单线程，包含 Windows 桌面预览与 Linux 设备两套分支。
             校验帧缓冲布局后做内存映射；触摸侧读取 evdev 的 EV_ABS/ABS_X/ABS_Y
             坐标与 EV_KEY/BTN_TOUCH 接触标志，以 EV_SYN/SYN_REPORT 结束一次上报；
             换算采用驱动声明量程经整数线性映射到 800x480，而非参考工程硬编码的
             x*800/1024、y*480/600；可选标定路径只在 --calibrate 下启用。
Others:      曾叠加四层逻辑，每层都可能扣住面板已停止更新的坐标：ABS_MT_SLOT 过滤、
             双轴已见锁存、SYN_DROPPED 取消手势、标定记录压过面板。现已全部移除。
             SYN_DROPPED 只计数、保留上次位置继续走，不打断笔画；原始事件日志有
             40 万行上限，防止诊断把分区写满。
Function List:
  1. platform_is_preview    预览分支恒返回真
  2. platform_cal_load      预览分支：标定加载空实现
  3. platform_cal_save      预览分支：标定保存空实现
  4. platform_cal_apply     预览分支：标定应用空实现
  5. platform_trace_abs     预览分支：ABS 跟踪空实现
  6. platform_touch_log     预览分支：事件日志空实现
  7. platform_alt_raw       预览分支：备用通道恒为无
  8. paint_window           预览分支：把应用像素画到窗口客户区
  9. window_proc            预览分支：把鼠标消息翻译成同类型事件
 10. platform_open          预览分支：建立固定尺寸的预览窗口
 11. platform_poll          预览分支：带超时地轮询窗口消息
 12. platform_present       预览分支：提交整帧并触发重绘
 13. platform_present_area  预览分支：整帧重绘的简版局部提交
 14. platform_close         预览分支：销毁窗口
 15. platform_is_preview    设备分支恒返回假
 16. span_gap               计算两个计数之差的绝对值
 17. cal_close_to_declared  判断标定记录是否仍与驱动声明量程相符
 18. platform_cal_load      从文件载入并校验标定记录
 19. platform_cal_save      把标定记录写入文件
 20. platform_cal_apply     使标定记录在下一次采样时生效
 21. abs_name               把 ABS 轴码翻译成可读名称
 22. trace_axes             列出设备公布的 ABS 轴及按键能力
 23. platform_trace_abs     打开或关闭 ABS 轴诊断
 24. platform_close         设备分支：关闭日志、解映射并关闭设备
 25. platform_open          设备分支：校验帧缓冲并打开触摸设备
 26. scale_axis             按当前量程做单段线性换算并裁剪
 27. log_event              把一条原始事件写入日志
 28. platform_touch_log     打开或关闭原始事件日志
 29. platform_alt_raw       读取备用多点触控坐标通道
 30. platform_poll          设备分支：读取一次evdev上报并换算成事件
 31. pack_channel           把 8 位颜色通道打包进帧缓冲位域
 32. platform_present_area  按实际步幅与位域提交屏幕矩形
 33. platform_present       提交整帧
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 源自谢东宏的 lcd/touch 模块。Codex，2026-11-19。
 * 单线程平台后端：校验过的帧缓冲布局、evdev 量程以及 Windows 预览。
 */
#include "platform.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
static HWND g_window;
static const Image *g_frame;
static Event g_pending;

/* 预览没有面板可标定，也没有原始轴可上报：指针本来就是屏幕像素，原始值与映射值
   是同一个数。 */
bool platform_is_preview(void) { return true; }
/* 预览分支：标定文件不存在，加载恒失败 */
bool platform_cal_load(TouchCal *cal) { (void)cal; return false; }
/* 预览分支：标定不落盘 */
bool platform_cal_save(const TouchCal *cal) { (void)cal; return false; }
/* 预览分支：标定无需应用 */
void platform_cal_apply(const TouchCal *cal) { (void)cal; }
/* 预览没有 evdev：指针已是屏幕像素，没有可枚举的原始轴，也就没有可跟踪的内容。 */
void platform_trace_abs(bool on) { (void)on; }
/* 同理，事件日志与备用通道预览下都不存在；留空实现使调用点只需一份。 */
void platform_touch_log(const char *path) { (void)path; }
/* 预览分支：没有备用通道，恒返回无 */
void platform_alt_raw(int *mtX, int *mtY, bool *have)
{
    if (mtX) { *mtX = 0; }
    if (mtY) { *mtY = 0; }
    if (have) { *have = false; }
}

/* 直接绘制应用真正的像素；预览里没有另一套界面实现。 */
static void paint_window(HDC dc)
{
    if (!g_frame) { return; }
    BITMAPINFO info;
    memset(&info,0,sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 800; info.bmiHeader.biHeight = -480;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    RECT client;
    GetClientRect(g_window,&client);
    StretchDIBits(dc,0,0,client.right,client.bottom,0,0,800,480,g_frame->pixels,&info,DIB_RGB_COLORS,SRCCOPY);
}

/* 把鼠标输入翻译成与 Linux evdev 相同的带类型事件。 */
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window,&paint); paint_window(dc); EndPaint(window,&paint);
        return 0;
    }
    if (message == WM_CLOSE) { g_pending = (Event){EVENT_QUIT,0,0}; return 0; }
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
        (message == WM_MOUSEMOVE && (wParam & MK_LBUTTON))) {
        RECT client;
        GetClientRect(window,&client);
        int x = GET_X_LPARAM(lParam)*800/(client.right ? client.right : 800);
        int y = GET_Y_LPARAM(lParam)*480/(client.bottom ? client.bottom : 480);
        if (message == WM_LBUTTONDOWN) { SetCapture(window); }
        if (message == WM_LBUTTONUP) { ReleaseCapture(); }
        /* 指针本来就是屏幕像素，所以它同时也是原始值对。 */
        app_note_raw(x, y);
        g_pending = (Event){message == WM_LBUTTONDOWN ? EVENT_DOWN :
            message == WM_LBUTTONUP ? EVENT_UP : EVENT_MOVE,x,y};
        return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}

/* 固定客户区尺寸与目标 LCD 一致；没有隐藏的第二条渲染路径。 */
bool platform_open(const char *fbPath, const char *touchPath, bool swapXY, bool invertX, bool invertY,
                   int xBias, int yBias)
{
    (void)fbPath; (void)touchPath; (void)swapXY; (void)invertX; (void)invertY;
    (void)xBias; (void)yBias;
    SetProcessDPIAware();
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = window_proc; cls.hInstance = instance;
    cls.lpszClassName = L"GecTerminalPreview"; cls.hCursor = LoadCursor(NULL,IDC_ARROW);
    if (!RegisterClassW(&cls)) { return false; }
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT area = {0,0,800,480}; AdjustWindowRect(&area,style,FALSE);
    g_window = CreateWindowW(cls.lpszClassName,L"GEC6818 Terminal - desktop simulation",style,
        CW_USEDEFAULT,CW_USEDEFAULT,area.right-area.left,area.bottom-area.top,NULL,NULL,instance,NULL);
    if (!g_window) { return false; }
    ShowWindow(g_window,SW_SHOW);
    return true;
}

/* 把空闲等待限制在主循环 tick 上；不忙等，也不开工作线程。 */
int platform_poll(Event *event, int timeoutMs)
{
    MSG message;
    *event = (Event){0};
    MsgWaitForMultipleObjects(0,NULL,FALSE,(DWORD)timeoutMs,QS_ALLINPUT);
    while (PeekMessageW(&message,NULL,0,0,PM_REMOVE)) {
        if (message.message == WM_QUIT) { *event = (Event){EVENT_QUIT,0,0}; return 1; }
        TranslateMessage(&message); DispatchMessageW(&message);
        if (g_pending.type != EVENT_NONE) { *event = g_pending; g_pending = (Event){0}; return 1; }
    }
    return 0;
}

/* WM_PAINT 只在 App 存活期间借用这块缓冲。 */
void platform_present(const Image *image)
{
    g_frame = image;
    InvalidateRect(g_window,NULL,FALSE); UpdateWindow(g_window);
}

/* 预览提交整帧；局部提交是设备专用路径。 */
void platform_present_area(const Image *image, Rect area)
{
    (void)area;
    platform_present(image);
}

/* 在主循环释放图像缓冲之前先销毁窗口。 */
void platform_close(void)
{
    g_frame = NULL;
    if (g_window) { DestroyWindow(g_window); g_window = NULL; }
}
#else
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
static int g_fb = -1, g_touch = -1;
static uint8_t *g_map;
static size_t g_mapLength;
static struct fb_var_screeninfo g_var;
static struct fb_fix_screeninfo g_fix;
/* 当前选中的 ABS 轴码与其声明量程 */
static struct input_absinfo g_axisX, g_axisY;
static unsigned int g_codeX, g_codeY;
static int g_rawX, g_rawY;
static bool g_down, g_reported, g_swap, g_invertX, g_invertY;
/* SYN_DROPPED 上报计数。参考读取器忽略它们；这里计数，因为某一带里突然出现一串
   本身就是诊断结论。 */
static long g_droppedCount;
/* --log-touch：带缓冲、有上限，且在 platform_close 中关闭。写满闪存比日志短更糟，
   一个把分区写满的诊断看起来就像 bug。 */
#define TOUCH_LOG_LIMIT 400000L
static FILE *g_log;
static long g_logLines;
/* 有符号轴向修正，启动时由 --x-bias/--y-bias 设定。 */
static int g_xBias, g_yBias;
/* 实际生效的原始量程。初值为驱动声明量程；存在 data/touch.cal 时由实测锚点取代。 */
static int g_minX, g_maxX, g_minY, g_maxY;
/* 实测锚点及其是否生效 */
static TouchCal g_cal;
static bool g_calActive;
/* 由标定向导写出；删掉这个文件即要求重新标定。 */
#define CAL_FILE "data/touch.cal"
/* --trace-abs。计数器防止话痨面板把串口日志淹没。 */
static bool g_traceAbs;
static int g_traceLines;
#define TRACE_LINES 900
/* 备用多点触控位置通道的上报值，仅用于与真正使用的通道对照跟踪。 */
static int g_mtX, g_mtY;
static bool g_hasMT;

bool platform_is_preview(void) { return false; }


/* 计算两个计数之间的距离，不为一次调用而引入 stdlib。 */
static int span_gap(int a, int b)
{
    int gap = a - b;
    return gap < 0 ? -gap : gap;
}

/* 只有仍描述这块面板的标定记录才可信。量程必须吻合：被压缩的量程会把整排按键
   挤到够不着的地方。偏移允许差四分之一，因为数字化板的有源区相对 LCD 偏几毫米
   是正常的 —— 这种偏移正是标定向导要测的东西。 */
static bool cal_close_to_declared(const TouchCal *cal)
{
    int spanX = g_maxX - g_minX, spanY = g_maxY - g_minY;
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    if (spanX <= 0 || spanY <= 0) { return true; }
    touch_cal_edges(cal->rawX, TOUCH_CAL_AXIS_X, SCREEN_WIDTH, &x0, &x1);
    touch_cal_edges(cal->rawY, TOUCH_CAL_AXIS_Y, SCREEN_HEIGHT, &y0, &y1);
    if (span_gap(x1 - x0, spanX) * 7 > spanX) { return false; }
    if (span_gap(y1 - y0, spanY) * 7 > spanY) { return false; }
    if (span_gap(x0, g_minX) * 4 > spanX) { return false; }
    if (span_gap(y0, g_minY) * 4 > spanY) { return false; }
    return true;
}

/* 只有两轴各有三个不同锚点的记录才可信；写了一半的文件或处处同一读数的面板
   否则会把指针冻在角落里。 */
bool platform_cal_load(TouchCal *cal)
{
    FILE *file;
    int swap = 0;
    int x0 = 0, x1 = 0, x2 = 0, y0 = 0, y1 = 0, y2 = 0;
    if (!cal) { return false; }
    file = fopen(CAL_FILE, "r");
    if (!file) { return false; }
    int read = fscanf(file, "%d %d %d %d %d %d %d", &swap, &x0, &x1, &x2, &y0, &y1, &y2);
    fclose(file);
    if (read != 7) { return false; }
    cal->swap = swap != 0;
    cal->rawX[0] = x0; cal->rawX[1] = x1; cal->rawX[2] = x2;
    cal->rawY[0] = y0; cal->rawY[1] = y1; cal->rawY[2] = y2;
    if (!touch_cal_valid(cal)) { return false; }
    if (!cal_close_to_declared(cal)) {
        fprintf(stderr,"touch calibration ignored: X=%d/%d/%d Y=%d/%d/%d is not close to "
                       "the declared X=%d..%d Y=%d..%d (rerun with --calibrate)\n",
            cal->rawX[0],cal->rawX[1],cal->rawX[2],cal->rawY[0],cal->rawY[1],cal->rawY[2],
            g_minX,g_maxX,g_minY,g_maxY);
        return false;
    }
    return true;
}

/* 把标定记录写入文件 */
bool platform_cal_save(const TouchCal *cal)
{
    FILE *file;
    if (!touch_cal_valid(cal)) { return false; }
    file = fopen(CAL_FILE, "w");
    if (!file) { return false; }
    /* 每根轴三个锚点，位置固定在 touchcal.h 中。 */
    fprintf(file, "# 触摸屏实测锚点，删除本文件即可改回驱动声明范围\n"
                  "# swap  x(%d,%d,%d)  y(%d,%d,%d)\n"
                  "%d %d %d %d %d %d %d\n",
        TOUCH_CAL_AXIS_X[0], TOUCH_CAL_AXIS_X[1], TOUCH_CAL_AXIS_X[2],
        TOUCH_CAL_AXIS_Y[0], TOUCH_CAL_AXIS_Y[1], TOUCH_CAL_AXIS_Y[2],
        cal->swap == 0 ? 0 : 1,
        cal->rawX[0], cal->rawX[1], cal->rawX[2],
        cal->rawY[0], cal->rawY[1], cal->rawY[2]);
    fclose(file);
    return true;
}

/* 使标定记录生效 */
void platform_cal_apply(const TouchCal *cal)
{
    if (!touch_cal_valid(cal)) { return; }
    g_cal = *cal;
    g_calActive = true;
    g_swap = cal->swap != 0;
}

/* 为触摸屏可能公布的轴取个名字，让转储读起来像驱动自己的术语而不是裸数字。 */
static const char *abs_name(unsigned int code)
{
    switch (code) {
    case ABS_X: return "ABS_X";
    case ABS_Y: return "ABS_Y";
    case ABS_PRESSURE: return "ABS_PRESSURE";
    case ABS_DISTANCE: return "ABS_DISTANCE";
    case ABS_TILT_X: return "ABS_TILT_X";
    case ABS_TILT_Y: return "ABS_TILT_Y";
    case ABS_MT_SLOT: return "ABS_MT_SLOT";
    case ABS_MT_TOUCH_MAJOR: return "ABS_MT_TOUCH_MAJOR";
    case ABS_MT_TOUCH_MINOR: return "ABS_MT_TOUCH_MINOR";
    case ABS_MT_WIDTH_MAJOR: return "ABS_MT_WIDTH_MAJOR";
    case ABS_MT_WIDTH_MINOR: return "ABS_MT_WIDTH_MINOR";
    case ABS_MT_ORIENTATION: return "ABS_MT_ORIENTATION";
    case ABS_MT_POSITION_X: return "ABS_MT_POSITION_X";
    case ABS_MT_POSITION_Y: return "ABS_MT_POSITION_Y";
    case ABS_MT_TOOL_TYPE: return "ABS_MT_TOOL_TYPE";
    case ABS_MT_BLOB_ID: return "ABS_MT_BLOB_ID";
    case ABS_MT_TRACKING_ID: return "ABS_MT_TRACKING_ID";
    case ABS_MT_PRESSURE: return "ABS_MT_PRESSURE";
    case ABS_MT_DISTANCE: return "ABS_MT_DISTANCE";
    default: return "ABS_?";
    }
}

/* 向设备查询它能上报什么。若第二条位置通道的量程比正在使用的那条更宽，
   那正是拯救读数那条死带的希望所在。 */
static void trace_axes(void)
{
    uint8_t axes[ABS_CNT/8 + 1];
    uint8_t keys[KEY_CNT/8 + 1];
    unsigned int code;
    memset(axes,0,sizeof(axes));
    memset(keys,0,sizeof(keys));
    if (ioctl(g_touch,EVIOCGBIT(EV_ABS,sizeof(axes)),axes) < 0) {
        fprintf(stderr,"trace: EVIOCGBIT(EV_ABS) failed: %s\n",strerror(errno));
        return;
    }
    fprintf(stderr,"trace: ABS axes published by the touch device\n");
    for (code = 0; code <= ABS_MAX; ++code) {
        struct input_absinfo info;
        if (!(axes[code/8] & (1u << (code%8)))) { continue; }
        if (ioctl(g_touch,EVIOCGABS(code),&info) < 0) { continue; }
        fprintf(stderr,"  %-20s value=%6d min=%6d max=%6d fuzz=%d flat=%d res=%d\n",
            abs_name(code),info.value,info.minimum,info.maximum,
            info.fuzz,info.flat,info.resolution);
    }
    if (ioctl(g_touch,EVIOCGBIT(EV_KEY,sizeof(keys)),keys) >= 0 &&
        (keys[BTN_TOUCH/8] & (1u << (BTN_TOUCH%8)))) {
        fprintf(stderr,"  press arrives as BTN_TOUCH\n");
    }
    fprintf(stderr,"trace: samples follow; touch the dead strip and compare the channels\n");
}

/* 打开或关闭 ABS 轴诊断跟踪 */
void platform_trace_abs(bool on)
{
    g_traceAbs = on;
    if (on && g_touch >= 0) { g_traceLines = 0; trace_axes(); }
}

/* 任意部分的初始化失败后，清理都是安全的。 */
void platform_close(void)
{
    if (g_log) {
        fprintf(g_log,"# end: %ld lines, %ld SYN_DROPPED\n",g_logLines,g_droppedCount);
        fclose(g_log); g_log = NULL;
        fprintf(stderr,"touch log: %ld events, %ld SYN_DROPPED\n",g_logLines,g_droppedCount);
    }
    if (g_map) { munmap(g_map,g_mapLength); g_map = NULL; }
    if (g_fb >= 0) { close(g_fb); g_fb = -1; }
    if (g_touch >= 0) { close(g_touch); g_touch = -1; }
}

/* 拒绝未知的颜色布局，而不是写错尺寸的帧缓冲。 */
bool platform_open(const char *fbPath, const char *touchPath, bool swapXY, bool invertX, bool invertY,
                   int xBias, int yBias)
{
    g_swap = swapXY; g_invertX = invertX; g_invertY = invertY;
    g_xBias = xBias; g_yBias = yBias;
    g_fb = open(fbPath,O_RDWR);
    if (g_fb < 0 || ioctl(g_fb,FBIOGET_VSCREENINFO,&g_var) < 0 ||
        ioctl(g_fb,FBIOGET_FSCREENINFO,&g_fix) < 0) { goto failed; }
    /* 只接受 800x480、16/32 位、真彩、打包像素的帧缓冲，逐项核对位域与步长。 */
    if (g_var.xres != 800 || g_var.yres != 480 ||
        (g_var.bits_per_pixel != 16 && g_var.bits_per_pixel != 32) ||
        g_var.red.length > 8 || !g_var.red.length || g_var.red.offset + g_var.red.length > g_var.bits_per_pixel ||
        g_var.green.length > 8 || !g_var.green.length || g_var.green.offset + g_var.green.length > g_var.bits_per_pixel ||
        g_var.blue.length > 8 || !g_var.blue.length || g_var.blue.offset + g_var.blue.length > g_var.bits_per_pixel ||
        g_var.transp.length > 8 || g_var.transp.offset + g_var.transp.length > g_var.bits_per_pixel ||
        g_var.red.msb_right || g_var.green.msb_right || g_var.blue.msb_right || g_var.transp.msb_right ||
        g_fix.type != FB_TYPE_PACKED_PIXELS || g_fix.visual != FB_VISUAL_TRUECOLOR ||
        g_var.nonstd || g_var.grayscale ||
        ((uint64_t)g_var.xoffset + 800) * (g_var.bits_per_pixel/8) > g_fix.line_length ||
        ((uint64_t)g_var.yoffset + 479) * g_fix.line_length +
        ((uint64_t)g_var.xoffset + 800) * (g_var.bits_per_pixel/8) > g_fix.smem_len) {
        fprintf(stderr,"Unsupported framebuffer: require 800x480 true-colour 16/32 bpp\n"); goto failed;
    }
    g_mapLength = g_fix.smem_len;
    void *mapped = mmap(NULL,g_mapLength,PROT_READ|PROT_WRITE,MAP_SHARED,g_fb,0);
    if (mapped == MAP_FAILED) { goto failed; }
    g_map = mapped;
    g_touch = open(touchPath,O_RDONLY|O_NONBLOCK);
    if (g_touch < 0) { goto failed; }
    g_codeX = ABS_X; g_codeY = ABS_Y;
    /* 优先用传统 ABS_X/ABS_Y；取不到时退回 ABS_MT_POSITION_X/Y。 */
    if (ioctl(g_touch,EVIOCGABS(ABS_X),&g_axisX) < 0 || ioctl(g_touch,EVIOCGABS(ABS_Y),&g_axisY) < 0) {
        g_codeX = ABS_MT_POSITION_X; g_codeY = ABS_MT_POSITION_Y;
        if (ioctl(g_touch,EVIOCGABS(ABS_MT_POSITION_X),&g_axisX) < 0 ||
            ioctl(g_touch,EVIOCGABS(ABS_MT_POSITION_Y),&g_axisY) < 0) { goto failed; }
    }
    if (g_axisX.maximum <= g_axisX.minimum || g_axisY.maximum <= g_axisY.minimum) {
        fprintf(stderr,"Invalid touchscreen ABS ranges\n"); goto failed;
    }
    g_rawX = g_axisX.value; g_rawY = g_axisY.value;
    g_minX = g_axisX.minimum; g_maxX = g_axisX.maximum;
    g_minY = g_axisY.minimum; g_maxY = g_axisY.maximum;
    fprintf(stderr,"LCD 800x480/%u, stride=%u; touch X=%d..%d Y=%d..%d via %s/%s\n",
        g_var.bits_per_pixel,g_fix.line_length,g_axisX.minimum,g_axisX.maximum,
        g_axisY.minimum,g_axisY.maximum,abs_name(g_codeX),abs_name(g_codeY));
    return true;
failed:
    perror("Device initialization"); platform_close(); return false;
}

/* 用当前生效的量程做归一化，并裁剪到可见坐标。量程要么是驱动声明的，要么是
   实测锚点的。 */
static int scale_axis(int raw, int lo, int hi, int size)
{
    return touch_cal_map(raw, lo, hi, size);
}

/* 把一条原始事件写入日志。 */
static void log_event(const struct input_event *input)
{
    if (!g_log || g_logLines >= TOUCH_LOG_LIMIT) { return; }
    /* 不记时间戳：musl 在 -std=c99 下隐藏了 input_event.time，而日志真正需要的是
       顺序 —— 哪根轴停了、又以什么顺序恢复。 */
    fprintf(g_log,"%ld %u %u %d\n",g_logLines,
        (unsigned)input->type,(unsigned)input->code,input->value);
    ++g_logLines;
}

/* 打开或关闭原始事件日志；传 NULL 表示关闭。 */
void platform_touch_log(const char *path)
{
    if (g_log) { fclose(g_log); g_log = NULL; }
    if (!path) { return; }
    g_log = fopen(path,"w");
    if (!g_log) { fprintf(stderr,"touch log: cannot write %s\n",path); return; }
    fprintf(g_log,"# build %s; axis %s/%s declared X=%d..%d Y=%d..%d\n",APP_BUILD,
        abs_name(g_codeX),abs_name(g_codeY),g_minX,g_maxX,g_minY,g_maxY);
    fprintf(g_log,"# columns: seq type code value\n");
    g_logLines = 0;
    fprintf(stderr,"touch log: writing every raw event to %s\n",path);
}

/* 读取备用多点触控通道 */
void platform_alt_raw(int *mtX, int *mtY, bool *have)
{
    if (mtX) { *mtX = g_mtX; }
    if (mtY) { *mtY = g_mtY; }
    if (have) { *have = g_hasMT; }
}

/* 每次调用读一条上报，方式与 GZ2418 谢东宏的 src/system/touch.c 一致：
   EV_ABS 设置坐标，EV_KEY 设置接触标志，EV_SYN 结束一次上报，然后由调用方绘制。
   他们的读取器就这么点代码，而且在这块板子上跑得好好的。

   本项目曾在其上叠了四层，每一层都可能扣住面板已经停止更新的坐标：

     - ABS_MT_SLOT 过滤：槽位非零时跳过所有 MT 轴。一个杂散槽位 —— 噪声带恰恰
       会产生幽灵第二触点 —— 就会让两轴一直冻结到收到槽位 0 的上报；
     - 双轴已见锁存：某根轴安静下来后继续重发它的旧值，而不是让采样如实反映
       刚到达的数据；
     - SYN_DROPPED 处理器取消手势，把一次缓冲小故障变成笔画里可见的断口；
     - 标定记录压过面板，使拟合曲线挡在手指与像素之间。

   这些都已移除。下面的映射用的是参考工程那套算术，只是量程取自驱动声明值，
   而不是硬编码的 1024x600（即 x*800/1024、y*480/600）；可选的标定路径留在
   --calibrate 后面，普通运行永远不会进入。 */
int platform_poll(Event *event, int timeoutMs)
{
    *event = (Event){0};
    struct pollfd descriptor = {g_touch,POLLIN,0};
    int ready = poll(&descriptor,1,timeoutMs);
    if (ready < 0) { return errno == EINTR ? 0 : -1; }
    if (!ready) { return 0; }
    if (descriptor.revents & (POLLERR|POLLHUP|POLLNVAL)) { return -1; }
    for (int count = 0; count < 256; ++count) {
        struct input_event input;
        ssize_t length = read(g_touch,&input,sizeof(input));
        if (length < 0 && (errno == EAGAIN || errno == EINTR)) { return 0; }
        if (length != sizeof(input)) { return -1; }
        log_event(&input);
        if (input.type == EV_SYN && input.code == SYN_DROPPED) {
            /* 参考读取器忽略它。这里只计数、保留上次位置并继续往后走：
               丢弃的上报属于日志，不该变成一条断线。 */
            ++g_droppedCount;
            continue;
        }
        if (input.type == EV_ABS) {
            /* 两个通道都记录。只有选中的通道驱动指针，但日志里两者不一致能说明
               哪根轴卡住了；若第二通道正常而第一通道发死，那就是一行代码的修复。 */
            if (input.code == g_codeX) { g_rawX = input.value; }
            if (input.code == g_codeY) { g_rawY = input.value; }
            if (input.code == ABS_MT_POSITION_X) { g_mtX = input.value; g_hasMT = true; }
            if (input.code == ABS_MT_POSITION_Y) { g_mtY = input.value; g_hasMT = true; }
            if (input.code == ABS_MT_TRACKING_ID) { g_down = input.value >= 0; }
        }
        if (input.type == EV_KEY && input.code == BTN_TOUCH) { g_down = input.value != 0; }
        if (g_traceAbs && input.type == EV_ABS && g_traceLines < TRACE_LINES) {
            ++g_traceLines;
            fprintf(stderr,"abs %s=%d\n",abs_name(input.code),input.value);
        }
        if (input.type == EV_SYN && input.code == SYN_REPORT) {
            /* 交换轴向的面板把水平轴放在原始 Y 通道上，因此在任何换算之前
               先对通道做交换。 */
            int horizontal = g_swap ? g_rawY : g_rawX;
            int vertical = g_swap ? g_rawX : g_rawY;
            int x, y;
            if (g_calActive) {
                x = touch_cal_map_piece(horizontal,g_cal.rawX,TOUCH_CAL_AXIS_X,SCREEN_WIDTH);
                y = touch_cal_map_piece(vertical,g_cal.rawY,TOUCH_CAL_AXIS_Y,SCREEN_HEIGHT);
            } else {
                int hLow = g_swap ? g_minY : g_minX, hHigh = g_swap ? g_maxY : g_maxX;
                int vLow = g_swap ? g_minX : g_minY, vHigh = g_swap ? g_maxX : g_maxY;
                x = scale_axis(horizontal,hLow,hHigh,SCREEN_WIDTH);
                y = scale_axis(vertical,vLow,vHigh,SCREEN_HEIGHT);
            }
            if (g_invertX) { x = 799-x; } if (g_invertY) { y = 479-y; }
            /* --x-bias/--y-bias 带来的有符号面板修正，在镜像之后再应用。 */
            x += g_xBias; y += g_yBias;
            if (x < 0) { x = 0; } if (x > 799) { x = 799; }
            if (y < 0) { y = 0; } if (y > 479) { y = 479; }
            event->x = x; event->y = y;
            app_note_raw(g_rawX, g_rawY);
            event->type = g_down ? (g_reported ? EVENT_MOVE : EVENT_DOWN) :
                g_reported ? EVENT_UP : EVENT_NONE;
            g_reported = g_down;
            /* 每次上报写一行：映射消费的原始值对、被它忽略的备用通道，以及
               最终产生的像素值，三者并列。 */
            if (g_traceAbs && event->type != EVENT_NONE) {
                fprintf(stderr,"  -> %d,%d from raw %d,%d",x,y,g_rawX,g_rawY);
                if (g_hasMT && g_codeY != ABS_MT_POSITION_Y) {
                    fprintf(stderr," (mt %d,%d)",g_mtX,g_mtY);
                }
                fprintf(stderr," %s\n",event->type == EVENT_DOWN ? "down" :
                    event->type == EVENT_MOVE ? "move" : "up");
            }
            return event->type != EVENT_NONE;
        }
    }
    return 0;
}

/* 把 8 位颜色通道按帧缓冲实际的位域放大并移位打包。 */
static uint32_t pack_channel(uint32_t rgb, unsigned int shift, struct fb_bitfield field)
{
    if (!field.length) { return 0; }
    return ((((rgb >> shift)&255) * ((1u << field.length)-1u) + 127u)/255u) << field.offset;
}

/* 尊重步长与可见偏移。当面板为 32bpp 且通道为常见的 16/8/0 位时，源数据已经匹配，
   一帧就是整行拷贝；通用路径留给 16bpp 与任何奇特的位域。 */
void platform_present_area(const Image *image, Rect area)
{
    unsigned int bytes = g_var.bits_per_pixel/8;
    int x0 = area.x < 0 ? 0 : area.x, y0 = area.y < 0 ? 0 : area.y;
    int x1 = area.x + area.w, y1 = area.y + area.h;
    if (x1 > (int)g_var.xres) { x1 = (int)g_var.xres; }
    if (y1 > (int)g_var.yres) { y1 = (int)g_var.yres; }
    if (x1 <= x0 || y1 <= y0) { return; }
    bool direct = bytes == 4 && g_var.red.offset == 16 && g_var.red.length == 8 &&
        g_var.green.offset == 8 && g_var.green.length == 8 &&
        g_var.blue.offset == 0 && g_var.blue.length == 8 && !g_var.transp.length;
    for (int y = y0; y < y1; ++y) {
        uint8_t *row = g_map + ((unsigned int)y+g_var.yoffset)*g_fix.line_length +
                       ((unsigned int)x0+g_var.xoffset)*bytes;
        if (direct) {
            memcpy(row, image->pixels + y*800 + x0, (size_t)(x1-x0)*4u);
        } else {
            for (int x = x0; x < x1; ++x) {
                uint32_t rgb = image->pixels[y*800+x];
                uint32_t pixel = pack_channel(rgb,16,g_var.red) | pack_channel(rgb,8,g_var.green) |
                    pack_channel(rgb,0,g_var.blue) | pack_channel(0xFF,0,g_var.transp);
                memcpy(row+(x-x0)*bytes,&pixel,bytes);
            }
        }
    }
}

/* 提交整帧：把整屏矩形交给区域提交函数。 */
void platform_present(const Image *image)
{
    platform_present_area(image, (Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT});
}
#endif
