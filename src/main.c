/* Xie Donghong entry point adapted by Codex, 2026-11-19: checked startup and clean single-loop shutdown. */
#include "platform.h"
#include <signal.h>
#include <stdio.h>
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
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i],"--help") == 0) {
            puts("terminal [--root DIR] [--fb DEVICE] [--touch DEVICE] [--data DIR] [--swap-xy] [--invert-x] [--invert-y]");
            return 0;
        }
        if (strcmp(argv[i],"--swap-xy") == 0) { swapXY = true; continue; }
        if (strcmp(argv[i],"--invert-x") == 0) { invertX = true; continue; }
        if (strcmp(argv[i],"--invert-y") == 0) { invertY = true; continue; }
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
    if (!platform_open(fbPath,touchPath,swapXY,invertX,invertY)) { app_close(&app); return 1; }
    signal(SIGINT,stop_signal); signal(SIGTERM,stop_signal);
    int status = 0;
    while (app.running && !g_stopping) {
        app_tick(&app);
        if (app.dirty) { app_render(&app); platform_present(&app.screen); }
        Event event;
        if (platform_poll(&event,16) < 0) { fprintf(stderr,"Touch device disconnected/read failed\n"); status = 1; break; }
        app_event(&app,event);
    }
    platform_close(); app_close(&app);
    return status;
}
