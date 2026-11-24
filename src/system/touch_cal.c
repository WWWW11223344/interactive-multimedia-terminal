/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   touch_cal.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 触摸标定算法实现。每根轴取三个锚点、两段直线：本板实测屏幕 y=70/250/430
             处原始值为 86/330/518，只连两端会让中央偏低约 23 像素，正好半个按键
             高度，登录键盘因此总点到下一行。三段锚点在中点相接，映射连续无跳变。
             全部为整数运算且不访问设备，拟合结果可在桌面构建上验证。
Others:      九个采样点按行优先排列，下标 = 行 * 3 + 列；锚点取在面板内部 60..740、
             70..430，胖手指也能点中，屏幕边缘由外侧线段各自外推得到。
Function List:
  1. cal_abs              整数求绝对值
  2. touch_cal_map        按量程做单段线性换算并裁剪
  3. touch_cal_edges      外推屏幕两端对应的原始值
  4. touch_cal_map_piece  经实测锚点做两段换算
  5. axis_usable          判断一根轴的锚点是否严格单调且量程够用
  6. touch_cal_valid      校验标定记录两轴是否可用
  7. touch_cal_solve      由九点采样解算标定记录并计算最大偏差
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
/* 触摸标定算法：每根轴三个锚点、两段直线。
 *
 * 为什么不用一条直线：本板在屏幕 y=70、250、430 处上报的原始 y 分别是 86、330、
 * 518。只穿过两端的一条直线会把中间算成原始值 302，于是屏幕中央附近的东西全部
 * 偏低约 23 像素 —— 这是半个按键的高度，也正是登录键盘总点到下一行的原因。面板
 * 的响应是弯的，那就允许映射一起弯：三个实测锚点、两段直线，在中点锚点处相接，
 * 结果没有台阶。
 *
 * 纯整数运算，不访问设备，因此拟合可以在桌面上验证。
 */
#include "touchcal.h"

/* 行优先的 3x3 网格：下标 = 行 * 3 + 列。锚点落在面板内部（横向 60..740、纵向
   70..430），胖手指也点得中；屏幕边缘由外侧线段各自按其斜率外推得到。 */
const int TOUCH_CAL_AXIS_X[TOUCH_CAL_AXES] = { 60, 400, 740};
const int TOUCH_CAL_AXIS_Y[TOUCH_CAL_AXES] = { 70, 250, 430};
const int TOUCH_CAL_TARGET[TOUCH_CAL_POINTS][2] = {
    { 60,  70}, {400,  70}, {740,  70},
    { 60, 250}, {400, 250}, {740, 250},
    { 60, 430}, {400, 430}, {740, 430}
};

/* 整数求绝对值 */
static int cal_abs(int value)
{
    return value < 0 ? -value : value;
}

/* 按声明量程做直线换算，并把结果裁剪到 0..size-1。 */
int touch_cal_map(int raw, int lo, int hi, int size)
{
    int64_t scaled = ((int64_t)raw - lo) * (size - 1) / ((int64_t)hi - lo);
    if (scaled < 0) { scaled = 0; }
    if (scaled >= size) { scaled = size - 1; }
    return (int)scaled;
}

/* 屏幕坐标 0 与 size-1 处的原始值。两端各由自己那一段外推：上边沿用上段斜率、
   下边沿用下段斜率；取平均只会重新引入第二段本来要消除的误差。 */
void touch_cal_edges(const int anchors[TOUCH_CAL_AXES], const int axis[TOUCH_CAL_AXES],
                     int size, int *atStart, int *atEnd)
{
    int64_t rise0 = (int64_t)anchors[1] - anchors[0];
    int64_t rise1 = (int64_t)anchors[2] - anchors[1];
    int64_t run0 = (int64_t)axis[1] - axis[0];
    int64_t run1 = (int64_t)axis[2] - axis[1];
    if (atStart) {
        *atStart = (int)(anchors[0] + (run0 ? rise0 * (0 - axis[0]) / run0 : 0));
    }
    if (atEnd) {
        *atEnd = (int)(anchors[1] + (run1 ? rise1 * ((size - 1) - axis[1]) / run1 : 0));
    }
}

/* 经实测锚点做两段换算 */
int touch_cal_map_piece(int raw, const int anchors[TOUCH_CAL_AXES],
                        const int axis[TOUCH_CAL_AXES], int size)
{
    int atStart = anchors[0], atEnd = anchors[2];
    /* 镜像面板上报的是递减量程：按方向选段，而不是假定原始值一定随屏幕向下增大。 */
    bool ascending = anchors[2] >= anchors[0];
    bool lower = ascending ? (raw <= anchors[1]) : (raw >= anchors[1]);
    touch_cal_edges(anchors, axis, size, &atStart, &atEnd);
    if (lower) {
        return touch_cal_map(raw, atStart, anchors[1], axis[1] + 1);
    }
    return axis[1] + touch_cal_map(raw, anchors[1], atEnd, size - axis[1]);
}

/* 严格单调，且三段锚点之间有足够间隔，确实是三个不同位置而不是同一处的三次读数。 */
static bool axis_usable(const int value[TOUCH_CAL_AXES])
{
    int first = value[1] - value[0];
    int second = value[2] - value[1];
    if (first > 0 && second > 0) {
        return first >= 8 && second >= 8 && cal_abs(value[2] - value[0]) >= 64;
    }
    if (first < 0 && second < 0) {
        return first <= -8 && second <= -8 && cal_abs(value[2] - value[0]) >= 64;
    }
    return false;
}

/* 两轴都必须可用；任何一个轴退化就拒绝整条记录。 */
bool touch_cal_valid(const TouchCal *cal)
{
    if (!cal) { return false; }
    return axis_usable(cal->rawX) && axis_usable(cal->rawY);
}

/* 由九点采样解算标定记录。 */
bool touch_cal_solve(const int rawX[TOUCH_CAL_POINTS], const int rawY[TOUCH_CAL_POINTS],
                     TouchCal *cal, int *errorPx)
{
    int column[TOUCH_CAL_AXES][2] = {{0}};
    int row[TOUCH_CAL_AXES][2] = {{0}};
    int i, worst = 0, swap;
    if (!cal) { return false; }
    /* 把同列或同行的三次点按取平均：一次手抖只占锚点的三分之一，而不是整个锚点。 */
    for (i = 0; i < TOUCH_CAL_POINTS; ++i) {
        int whichRow = i / TOUCH_CAL_AXES, whichCol = i % TOUCH_CAL_AXES;
        column[whichCol][0] += rawX[i];
        column[whichCol][1] += rawY[i];
        row[whichRow][0] += rawX[i];
        row[whichRow][1] += rawY[i];
    }
    for (i = 0; i < TOUCH_CAL_AXES; ++i) {
        column[i][0] /= TOUCH_CAL_AXES;
        column[i][1] /= TOUCH_CAL_AXES;
        row[i][0] /= TOUCH_CAL_AXES;
        row[i][1] /= TOUCH_CAL_AXES;
    }
    /* 手指从左列移到右列时哪个原始通道变化最大？据此自动判断轴向交换，
       所以不需要额外的 --swap-xy 参数。 */
    swap = cal_abs(column[2][1] - column[0][1]) > cal_abs(column[2][0] - column[0][0]) ? 1 : 0;
    cal->swap = swap;
    for (i = 0; i < TOUCH_CAL_AXES; ++i) {
        cal->rawX[i] = swap ? column[i][1] : column[i][0];
        cal->rawY[i] = swap ? row[i][0] : row[i][1];
    }
    if (!touch_cal_valid(cal)) {
        if (errorPx) { *errorPx = -1; }
        return false;
    }
    /* 用参与拟合的九个采样回代检验。锚点按构造必然吻合，这里真正要抓的是
       某个点按偏了十字。 */
    for (i = 0; i < TOUCH_CAL_POINTS; ++i) {
        int horizontal = touch_cal_map_piece(swap ? rawY[i] : rawX[i],
            cal->rawX, TOUCH_CAL_AXIS_X, SCREEN_WIDTH);
        int vertical = touch_cal_map_piece(swap ? rawX[i] : rawY[i],
            cal->rawY, TOUCH_CAL_AXIS_Y, SCREEN_HEIGHT);
        int dx = cal_abs(horizontal - TOUCH_CAL_TARGET[i][0]);
        int dy = cal_abs(vertical - TOUCH_CAL_TARGET[i][1]);
        if (dx > worst) { worst = dx; }
        if (dy > worst) { worst = dy; }
    }
    if (errorPx) { *errorPx = worst; }
    return worst <= TOUCH_CAL_TOLERANCE;
}
