/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   platform.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 平台抽象层头文件。统一声明帧缓冲显示、evdev 触摸读取以及 Windows 桌面
             预览三套后端共用的接口，使核心层与页面不依赖具体的设备实现。这里还
             声明标定记录的读写与应用、诊断用的 ABS 轴跟踪与原始事件日志接口。
Others:      同一时刻只有一个平台实例，由主循环持有。poll 在设备丢失时返回 -1。
             xBias/yBias 对每个上报的触摸做整像素平移，正值表示向右、向下移动，
             用于把面板与用户所见画面对齐。
Function List:
  1. platform_open          打开帧缓冲与触摸设备，或建立桌面预览窗口
  2. platform_poll          轮询一次触摸事件，带超时
  3. platform_present       把整幅图像提交到屏幕
  4. platform_present_area  只提交指定的屏幕矩形，用于画笔增量绘制
  5. platform_close         关闭设备并释放资源
  6. platform_is_preview    是否为桌面预览模式
  7. platform_cal_load      从文件载入标定记录
  8. platform_cal_save      把标定记录写入文件
  9. platform_cal_apply     使标定记录在下一次采样时生效
 10. touch_calibrate        九点标定向导
 11. platform_trace_abs     打开或关闭 ABS 轴诊断跟踪
 12. platform_touch_log     把每个原始事件写入日志文件
 13. platform_alt_raw       读取备用多点触控坐标通道
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#ifndef TERMINAL_PLATFORM_H
#define TERMINAL_PLATFORM_H
#include "terminal.h"
#include "touchcal.h"
/* 整个程序只有一个平台实例，由主循环持有。poll 在设备丢失时返回 -1。
   xBias/yBias 把每次上报的触摸平移若干像素，正值表示向右、向下移动，
   可用它把面板与用户实际看到的画面对齐。 */
bool platform_open(const char *fbPath, const char *touchPath, bool swapXY, bool invertX, bool invertY,
                   int xBias, int yBias);
/* 轮询一次触摸事件。返回 1 表示拿到事件，0 表示超时，负数表示设备丢失 */
int platform_poll(Event *event, int timeoutMs);
/* 把整幅图像提交到帧缓冲或预览窗口 */
void platform_present(const Image *image);
/* 只推送指定的屏幕矩形。画笔落笔期间用它，面板就不必每个采样点都收一整帧。 */
void platform_present_area(const Image *image, Rect area);
/* 关闭平台，释放帧缓冲映射、设备句柄与窗口 */
void platform_close(void);
/* 桌面预览返回真。此时指针本来就是屏幕像素，再走标定只会白白浪费五次点击。 */
bool platform_is_preview(void);

/* 载入本面板实测的原始量程，与账号列表存在一起。载入只是建议性的：文件缺失或
   不可用时，仍沿用驱动声明的量程。 */
bool platform_cal_load(TouchCal *cal);
/* 把标定记录保存到文件 */
bool platform_cal_save(const TouchCal *cal);
/* 从下一次采样起生效，并持续到本次运行结束。退化的记录会被忽略，
   这样被截断的文件也不会让面板彻底失灵。 */
void platform_cal_apply(const TouchCal *cal);
/* 九点标定向导。它会阻塞、在 app->screen 上绘图，并在标定完成、用户放弃或
   触摸设备消失时返回。 */
void touch_calibrate(App *app);
/* 诊断手段而非修复手段：列出设备公布的每一根 ABS 轴及其声明量程和当前值，
   随后逐条回显到来的采样。它不改动任何映射，因此打印的是面板本身的特性，
   而不是我们的换算结果。应在 platform_open 之后打开；桌面预览下为空操作。 */
void platform_trace_abs(bool on);

/* 一次诊断过程中把每个原始事件逐行写入文件。串口会滚动，截图也装不下上千个
   采样，而文件可以原样发回分析。传 NULL 表示停止；桌面预览下为空操作。 */
void platform_touch_log(const char *path);
/* 备用坐标通道 ABS_MT_POSITION_*，与映射实际使用的通道并列读取。两者不一致
   说明面板只通过其中一个通道上报，这正是量程错误与某一轴卡死的区别所在。 */
void platform_alt_raw(int *mtX, int *mtY, bool *have);
#endif
