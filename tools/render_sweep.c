/* 扫屏诊断布局的离屏检查。
 * 向屏幕输入一段看似合理的扫屏数据，包含一条失效带、一个卡住的原始值，以及两侧
 * 的正常区域，以便在上板之前先审阅绘图。 */
#include "terminal.h"
#include "touchsweep.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static App app;
    memset(&app,0,sizeof(app));
    if (!app_open(&app,"data/drawings")) { fprintf(stderr,"app_open failed\n"); return 1; }
    /* 面板的正常区域：rawY 随 y 变化，大约每像素 1.25 个原始值。 */
    for (int y = 0; y < 480; y += 4) {
        if (y >= 100 && y <= 259) { continue; }          /* 无响应的条带 */
        app_note_raw(300 + y / 4, 20 + (int)((long)y * 125 / 100));
        touch_sweep_event(&app,(Event){y >= 476 ? EVENT_UP : EVENT_MOVE,200,y});
    }
    /* 卡住的条带：大量采样，同一个原始值。 */
    for (int i = 0; i < 120; ++i) {
        app_note_raw(420,330);
        touch_sweep_event(&app,(Event){i == 119 ? EVENT_UP : EVENT_MOVE,200,120 + i});
    }
    app_note_raw(430,340);
    touch_sweep_event(&app,(Event){EVENT_MOVE,200,264});
    touch_sweep_draw(&app);
    if (!image_save("shots/sweep.bmp",&app.screen)) { fprintf(stderr,"save failed\n"); return 1; }
    fprintf(stderr,"wrote shots/sweep.bmp\n");
    return 0;
}
