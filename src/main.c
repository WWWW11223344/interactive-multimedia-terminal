/* Xie Donghong entry point adapted by Codex, 2026-11-19: checked startup and clean single-loop shutdown. */
#include "platform.h"
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
static volatile sig_atomic_t g_stopping;

/* Signal handlers never touch graphics, heap memory or device descriptors. */
static void stop_signal(int signalNumber)
{
    (void)signalNumber; g_stopping = 1;
}

/* CLI overrides board-specific device nodes; resources are resolved after --root/chdir. */
int main(int argc, char **argv)
{
    const char *fbPath = "/dev/fb0", *touchPath = "/dev/input/event0", *saveDir = "data/drawings";
    bool swapXY = false, invertX = false, invertY = false;
    /* No global bias by default: the panel reads true except for one strip, and the
       keypad layout simply avoids that strip rather than shifting every screen. Use
       the calibration pass below to measure that strip properly. */
    int xBias = 0, yBias = 0;
    /* The five-tap calibration runs on a panel that has no record yet. --calibrate
       repeats it on demand, --no-calibrate skips it on a panel that is already good. */
    bool forceCal = false, skipCal = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i],"--help") == 0) {
            puts("terminal [--root DIR] [--fb DEVICE] [--touch DEVICE] [--data DIR]\n"
                 "         [--swap-xy] [--invert-x] [--invert-y] [--x-bias N] [--y-bias N]\n"
                 "         [--calibrate] [--no-calibrate]");
            return 0;
        }
        if (strcmp(argv[i],"--swap-xy") == 0) { swapXY = true; continue; }
        if (strcmp(argv[i],"--invert-x") == 0) { invertX = true; continue; }
        if (strcmp(argv[i],"--invert-y") == 0) { invertY = true; continue; }
        if (strcmp(argv[i],"--calibrate") == 0) { forceCal = true; continue; }
        if (strcmp(argv[i],"--no-calibrate") == 0) { skipCal = true; continue; }
        if (strcmp(argv[i],"--x-bias") == 0) {
            if (i+1 >= argc) { fprintf(stderr,"Missing value: --x-bias\n"); return 2; }
            xBias = atoi(argv[++i]);
            continue;
        }
        if (strcmp(argv[i],"--y-bias") == 0) {
            if (i+1 >= argc) { fprintf(stderr,"Missing value: --y-bias\n"); return 2; }
            yBias = atoi(argv[++i]);
            continue;
        }
        if (i+1 >= argc) { fprintf(stderr,"Missing argument: %s\n",argv[i]); return 2; }
        const char *option = argv[i], *value = argv[++i];
        if (strcmp(option,"--fb") == 0) { fbPath = value; }
        else if (strcmp(option,"--touch") == 0) { touchPath = value; }
        else if (strcmp(option,"--data") == 0) { saveDir = value; }
        else if (strcmp(option,"--root") == 0) {
            if (change_dir(value) != 0) { perror("--root"); return 2; }
        } else { fprintf(stderr,"Unknown option: %s\n",option); return 2; }
    }
    static App app;
    if (!app_open(&app,saveDir)) { fprintf(stderr,"Resource initialization failed\n"); return 1; }
    if (!platform_open(fbPath,touchPath,swapXY,invertX,invertY,xBias,yBias)) { app_close(&app); return 1; }
    /* A calibrated panel wins over the range the driver declares, because that
       declaration is what put half the screen out of reach in the first place. */
    TouchCal cal;
    bool haveCal = platform_cal_load(&cal);
    if (haveCal) {
        platform_cal_apply(&cal);
        fprintf(stderr,"touch calibration loaded: swap=%d X=%d..%d Y=%d..%d\n",
            cal.swap,cal.x0,cal.x1,cal.y0,cal.y1);
    }
    if (forceCal || (!skipCal && !haveCal && !platform_is_preview())) {
        touch_calibrate(&app);
    }
    signal(SIGINT,stop_signal); signal(SIGTERM,stop_signal);
    int status = 0;
    while (app.running && !g_stopping) {
        Event event;
        int ready = platform_poll(&event,16);
        if (ready < 0) { fprintf(stderr,"Touch device disconnected/read failed\n"); status = 1; break; }
        if (ready > 0) { app_event(&app,event); }
        /* Absorb the rest of the burst before painting. A stroke or a swipe produces
           dozens of samples per frame; handling one per repaint let the queue grow
           without bound, which is what made the canvas feel seconds behind the pen. */
        while (app.running && !g_stopping && platform_poll(&event,0) > 0) { app_event(&app,event); }
        app_tick(&app);
        if (app.dirty) {
            app_render(&app);
            platform_present(&app.screen);
            app.dirtyArea = (Rect){0,0,0,0};
        } else if (app.dirtyArea.w > 0) {
            /* A stroke sample already painted itself into the frame: push that
               rectangle only. */
            platform_present_area(&app.screen, app.dirtyArea);
            app.dirtyArea = (Rect){0,0,0,0};
        }
    }
    platform_close(); app_close(&app);
    return status;
}
