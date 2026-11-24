/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   touchcal.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 触摸标定算法头文件。每根轴用三个锚点、两段直线拟合，把触摸屏上报的
             原始值换算成屏幕像素。算法为纯整数运算且不访问设备，因此可在桌面
             构建上先验证拟合结果，再拿到板子上使用。
Others:      三点标定而非两点：本板实测 raw y 在屏幕 y=70/250/430 处分别为
             86/330/518，只连两端会让屏幕中央偏下约 23 像素，恰好是半个按键高度，
             这正是登录数字键盘总点到下一行的原因。
Function List:
  1. touch_cal_map        按声明量程做单段线性换算并裁剪到屏幕范围
  2. touch_cal_map_piece  经实测锚点做两段换算
  3. touch_cal_edges      外推屏幕两端对应的原始值
  4. touch_cal_solve      由九点采样解算标定记录并给出最大偏差
  5. touch_cal_valid      校验标定记录两轴是否严格单调且量程够用
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 触摸标定算法：每根轴三个锚点、两段直线。纯整数运算且不访问设备，因此拟合
   可以在桌面构建（tests/test_touch_cal.c）上先得到验证，再用到面板上。 */
#ifndef TERMINAL_TOUCHCAL_H
#define TERMINAL_TOUCHCAL_H
#include "display.h"

/* 在 3x3 网格上按九个点。每个锚点取三次采样，一次手抖只会影响锚点的三分之一，
   而不是整个锚点，同时留下可供检验的残差。 */
#define TOUCH_CAL_POINTS 9
/* 每根轴的锚点个数 */
#define TOUCH_CAL_AXES 3
/* 九个采样点对应的屏幕目标坐标 */
extern const int TOUCH_CAL_TARGET[TOUCH_CAL_POINTS][2];
/* 每根轴上三个锚点的屏幕位置，由拟合与映射共用，保证两者对锚点位置的认定一致。 */
extern const int TOUCH_CAL_AXIS_X[TOUCH_CAL_AXES];
extern const int TOUCH_CAL_AXIS_Y[TOUCH_CAL_AXES];

/* 面板在各锚点上报的原始值。由拟合填入、由设备层保存。无论原始值来自哪个通道，
   rawX 始终代表水平轴。 */
typedef struct {
    int swap;                        /* 原始 Y 通道承载水平轴 */
    int rawX[TOUCH_CAL_AXES];        /* 水平轴三个锚点的原始值 */
    int rawY[TOUCH_CAL_AXES];        /* 竖直轴三个锚点的原始值 */
} TouchCal;

/* 按声明量程做直线换算；没有标定记录时走这条路径。 */
int touch_cal_map(int raw, int lo, int hi, int size);
/* 经实测锚点做两段换算。响应弯曲的面板无法用一条直线描述：残差会落在屏幕中央，
   而那正是按键所在的位置。本板实测单条直线在中央偏差 23 像素，两端却很准。 */
int touch_cal_map_piece(int raw, const int anchors[TOUCH_CAL_AXES],
                        const int axis[TOUCH_CAL_AXES], int size);
/* 屏幕坐标 0 与 size-1 处对应的原始值，按各段分别外推；也用于把保存的记录
   与驱动声明的量程做比对。 */
void touch_cal_edges(const int anchors[TOUCH_CAL_AXES], const int axis[TOUCH_CAL_AXES],
                     int size, int *atStart, int *atEnd);
/* 由九点原始值解算标定记录，errorPx 返回换算后相对目标的最大偏差（像素） */
bool touch_cal_solve(const int rawX[TOUCH_CAL_POINTS], const int rawY[TOUCH_CAL_POINTS],
                     TouchCal *cal, int *errorPx);
/* 两轴都必须严格单调且量程够用；处处上报同一数值的面板无法映射，
   拒绝它好过让指针卡死。 */
bool touch_cal_valid(const TouchCal *cal);
/* 标定允许的最大残差，超过即判定标定不合格 */
#define TOUCH_CAL_TOLERANCE 40
#endif
