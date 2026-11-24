/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   touchprobe.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 触摸自检页面头文件。该页面只显示指针背后的面板原始值：读取与应用
             相同的状态，且只写 app->screen，因此自检过程不会改动面板、标定文件
             或作品保存目录。
Others:      「屏幕上方没反应」这类描述单看像素坐标无法判断，因为量程错误与面板
             停止上报在换算后表现一致；把每个采样的原始值显示出来即可一次区分。
Function List:
  1. touch_probe_event  自检期间替代 app_event 接收事件，只记录不导航
  2. touch_probe_draw   重绘整帧：分段标尺、原始值与映射值大字读数、量程范围
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 触摸自检页面：只显示指针背后的面板原始值，别无其他。它读取应用所用的同一份
   状态，且只写 app->screen，因此自检不会改变面板、标定文件或保存目录。 */
#ifndef TERMINAL_TOUCHPROBE_H
#define TERMINAL_TOUCHPROBE_H
#include "terminal.h"
/* 自检期间把所有事件送到这里而不是 app_event：自检页面不得发生页面跳转，
   而且它下面的页面本来也到不了。 */
void touch_probe_event(App *app, Event event);
/* 重绘整帧：分段标尺、放大的原始值与映射值读数，以及启动以来见到的原始量程。
   量程是区分「一条死带」与「量程错误」的关键。 */
void touch_probe_draw(App *app);
#endif
