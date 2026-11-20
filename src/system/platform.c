/* Derived from Xie Donghong's lcd/touch modules. Codex, 2026-11-19.
 * One-thread platform backend: validated framebuffer layout, evdev ranges and Windows preview.
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

/* Paint the application's actual pixels; the preview contains no separate UI implementation. */
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

/* Translate mouse input to the same typed events emitted by Linux evdev. */
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
        g_pending = (Event){message == WM_LBUTTONDOWN ? EVENT_DOWN :
            message == WM_LBUTTONUP ? EVENT_UP : EVENT_MOVE,x,y};
        return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}

/* Fixed client size matches the target LCD; no hidden alternative rendering path. */
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

/* Bound idle waiting to the main loop tick; no busy polling or worker thread. */
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

/* WM_PAINT borrows this buffer only while App remains alive. */
void platform_present(const Image *image)
{
    g_frame = image;
    InvalidateRect(g_window,NULL,FALSE); UpdateWindow(g_window);
}

/* The preview repaints whole frames; partial presentation is a device-only path. */
void platform_present_area(const Image *image, Rect area)
{
    (void)area;
    platform_present(image);
}

/* Destroy the window before the main loop frees its image buffer. */
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
static struct input_absinfo g_axisX, g_axisY;
static unsigned int g_codeX, g_codeY;
static int g_rawX, g_rawY;
static bool g_down, g_reported, g_seenX, g_seenY, g_dropped, g_swap, g_invertX, g_invertY;
static bool g_blocked;
static int g_slot;
/* Signed axis corrections, set from --x-bias/--y-bias at startup. */
static int g_xBias, g_yBias;

/* Cleanup is safe after any partial initialization failure. */
void platform_close(void)
{
    if (g_map) { munmap(g_map,g_mapLength); g_map = NULL; }
    if (g_fb >= 0) { close(g_fb); g_fb = -1; }
    if (g_touch >= 0) { close(g_touch); g_touch = -1; }
}

/* Reject unknown colour layouts rather than writing a wrongly sized framebuffer. */
bool platform_open(const char *fbPath, const char *touchPath, bool swapXY, bool invertX, bool invertY,
                   int xBias, int yBias)
{
    g_swap = swapXY; g_invertX = invertX; g_invertY = invertY;
    g_xBias = xBias; g_yBias = yBias;
    g_fb = open(fbPath,O_RDWR);
    if (g_fb < 0 || ioctl(g_fb,FBIOGET_VSCREENINFO,&g_var) < 0 ||
        ioctl(g_fb,FBIOGET_FSCREENINFO,&g_fix) < 0) { goto failed; }
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
    if (ioctl(g_touch,EVIOCGABS(ABS_X),&g_axisX) < 0 || ioctl(g_touch,EVIOCGABS(ABS_Y),&g_axisY) < 0) {
        g_codeX = ABS_MT_POSITION_X; g_codeY = ABS_MT_POSITION_Y;
        if (ioctl(g_touch,EVIOCGABS(ABS_MT_POSITION_X),&g_axisX) < 0 ||
            ioctl(g_touch,EVIOCGABS(ABS_MT_POSITION_Y),&g_axisY) < 0) { goto failed; }
    }
    if (g_axisX.maximum <= g_axisX.minimum || g_axisY.maximum <= g_axisY.minimum) {
        fprintf(stderr,"Invalid touchscreen ABS ranges\n"); goto failed;
    }
    g_rawX = g_axisX.value; g_rawY = g_axisY.value;
    g_seenX = true; g_seenY = true;
    fprintf(stderr,"LCD 800x480/%u, stride=%u; touch X=%d..%d Y=%d..%d\n",
        g_var.bits_per_pixel,g_fix.line_length,g_axisX.minimum,g_axisX.maximum,g_axisY.minimum,g_axisY.maximum);
    return true;
failed:
    perror("Device initialization"); platform_close(); return false;
}

/* Normalize from the kernel-reported axis range, then clamp to visible coordinates. */
static int scale_axis(int raw, struct input_absinfo axis, int size)
{
    int64_t scaled = ((int64_t)raw-axis.minimum)*(size-1)/((int64_t)axis.maximum-axis.minimum);
    if (scaled < 0) { scaled = 0; }
    if (scaled >= size) { scaled = size-1; }
    return (int)scaled;
}

/* Emit coherent SYN_REPORT samples. A dropped report cancels the current gesture. */
int platform_poll(Event *event, int timeoutMs)
{
    *event = (Event){0};
    struct pollfd descriptor = {g_touch,POLLIN,0};
    int ready = poll(&descriptor,1,timeoutMs);
    if (ready < 0) { return errno == EINTR ? 0 : -1; }
    if (!ready) { return 0; }
    if (descriptor.revents & (POLLERR|POLLHUP|POLLNVAL)) { return -1; }
    for (int count = 0; count < 128; ++count) {
        struct input_event input;
        ssize_t length = read(g_touch,&input,sizeof(input));
        if (length < 0 && (errno == EAGAIN || errno == EINTR)) { return 0; }
        if (length != sizeof(input)) { return -1; }
        if (input.type == EV_SYN && input.code == SYN_DROPPED) {
            g_dropped = true; g_blocked = true; g_down = false; g_reported = false;
            *event = (Event){EVENT_CANCEL,0,0}; return 1;
        }
        if (g_dropped) {
            if (input.type == EV_SYN && input.code == SYN_REPORT) { g_dropped = false; }
            continue;
        }
        if (input.type == EV_ABS && input.code == ABS_MT_SLOT) { g_slot = input.value; }
        if (input.type == EV_ABS && input.code >= ABS_MT_SLOT && g_slot != 0) { continue; }
        if (input.type == EV_ABS && input.code == g_codeX) { g_rawX = input.value; g_seenX = true; }
        if (input.type == EV_ABS && input.code == g_codeY) { g_rawY = input.value; g_seenY = true; }
        if (input.type == EV_KEY && input.code == BTN_TOUCH) {
            if (!input.value) { g_blocked = false; }
            g_down = !g_blocked && input.value != 0;
        }
        if (input.type == EV_ABS && input.code == ABS_MT_TRACKING_ID) {
            if (input.value < 0) { g_blocked = false; }
            g_down = !g_blocked && input.value >= 0;
        }
        if (input.type == EV_SYN && input.code == SYN_REPORT && g_seenX && g_seenY) {
            int x = scale_axis(g_swap ? g_rawY : g_rawX,g_swap ? g_axisY : g_axisX,800);
            int y = scale_axis(g_swap ? g_rawX : g_rawY,g_swap ? g_axisX : g_axisY,480);
            if (g_invertX) { x = 799-x; } if (g_invertY) { y = 479-y; }
            /* Signed panel correction from --x-bias/--y-bias, applied after any mirroring. */
            x += g_xBias; y += g_yBias;
            if (x < 0) { x = 0; } if (x > 799) { x = 799; }
            if (y < 0) { y = 0; } if (y > 479) { y = 479; }
            event->x = x; event->y = y;
            event->type = g_down ? (g_reported ? EVENT_MOVE : EVENT_DOWN) :
                g_reported ? EVENT_UP : EVENT_NONE;
            g_reported = g_down;
            return event->type != EVENT_NONE;
        }
    }
    return 0;
}

/* Scale an 8-bit channel into the framebuffer's actual bitfield. */
static uint32_t pack_channel(uint32_t rgb, unsigned int shift, struct fb_bitfield field)
{
    if (!field.length) { return 0; }
    return ((((rgb >> shift)&255) * ((1u << field.length)-1u) + 127u)/255u) << field.offset;
}

/* Respect stride and visible offsets. When the panel is 32bpp with 8-bit channels in
   the usual 16/8/0 positions the source already matches, so a frame is a row copy;
   the generic path stays for 16bpp and any odd bitfield. */
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

void platform_present(const Image *image)
{
    platform_present_area(image, (Rect){0,0,SCREEN_WIDTH,SCREEN_HEIGHT});
}
#endif
