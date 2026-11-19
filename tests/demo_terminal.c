/* Codex, 2026-11-19: replay real application events for an explicitly labelled software demo. */
#include "terminal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#ifdef _WIN32
#include <process.h>
#include <direct.h>
#define process_id _getpid
#define remove_dir _rmdir
#else
#include <unistd.h>
#define process_id getpid
#define remove_dir rmdir
#endif
static App g_app;
static FILE *g_index;
static unsigned int g_frame;

/* Key frames are C-rendered; the video tool only adds explanatory captions. */
static void save_frame(unsigned int durationMs, const char *caption)
{
    char path[128]; snprintf(path,sizeof(path),"build-demo/frame_%03u.bmp",g_frame);
    app_render(&g_app); assert(image_save(path,&g_app.screen));
    fprintf(g_index,"%03u\t%u\t%s\n",g_frame++,durationMs,caption);
}

/* Activate a control through the production input dispatcher. */
static void click(int x, int y)
{
    app_event(&g_app,(Event){EVENT_DOWN,x,y}); app_event(&g_app,(Event){EVENT_UP,x,y});
}

/* Capture progressive strokes so the demonstration shows the real interpolation. */
static void stroke(int x0, int y0, int x1, int y1)
{
    app_event(&g_app,(Event){EVENT_DOWN,x0,y0});
    for (int i = 1; i <= 12; ++i) {
        app_event(&g_app,(Event){EVENT_MOVE,x0+(x1-x0)*i/12,y0+(y1-y0)*i/12});
        save_frame(90,"画板：颜色、粗细与连续笔迹");
    }
    app_event(&g_app,(Event){EVENT_UP,x1,y1});
}

/* All generated artwork is isolated from the user's persistent data directory. */
int main(void)
{
    char directory[128]; snprintf(directory,sizeof(directory),"data/test-demo-%d",(int)process_id());
    g_index = fopen("build-demo/frames.tsv","wb"); assert(g_index);
    assert(app_open(&g_app,directory)); save_frame(4000,"主界面：时间显示与三个功能入口");
    click(600,260); click(90,110); click(575,110); save_frame(2000,"进入创意画板，选择颜色和笔触");
    stroke(110,350,220,220); stroke(220,220,330,350); stroke(330,350,110,350);
    click(180,110); stroke(410,350,410,225); stroke(410,225,590,225);
    stroke(590,225,590,350); stroke(590,350,410,350);
    click(700,40); save_frame(3500,"保存作品：写入 BMP 文件，显示保存结果");
    click(50,40); click(110,250); save_frame(4000,"电子相册：作品与原项目照片一起显示");
    click(100,140); save_frame(3000,"点击缩略图，放大查看刚保存的画板作品");
    click(520,445); save_frame(2500,"上一张 / 下一张：浏览原项目内置照片");
    click(50,40); click(350,250); save_frame(2000,"进入 2048：开始、重新开始和返回");
    click(680,40);
    for (int i = 0; i < 12; ++i) {
        click(i%2 ? 680 : 490,300); click(590,i%3 ? 235 : 300);
        save_frame(450,"2048：方向操作、数字合并与得分更新");
    }
    click(680,40); save_frame(2500,"重新开始：清空得分并生成新的初始棋盘");
    click(50,40); save_frame(2500,"返回主界面；音乐与视频功能已移除");
    app_close(&g_app); assert(fclose(g_index) == 0);
    DIR *dir = opendir(directory); assert(dir); struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') { continue; }
        char path[PATH_CAPACITY]; snprintf(path,sizeof(path),"%s/%s",directory,entry->d_name);
        assert(remove(path) == 0);
    }
    closedir(dir); assert(remove_dir(directory) == 0);
    puts("Saved real C-rendered software demonstration frames (not hardware video).");
    return 0;
}
