/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   game.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 2048 对局规则头文件。改编自小林湘的 game.c，采用确定性的指数表示法与
             自带种子，不依赖全局随机数，也没有绘制副作用：格子值 0 表示空，1 表示
             2，依次到 11 表示 2048。调用方自行持有 Game，规则与界面分离，界面实现
             在 GameSystem.c。
Others:      随机数种子由调用方传入，同一局面可复现，便于回归测试；game_slide 只做
             滑动不生成新数字，划出规则与测试的边界。
Function List:
  1. game_start     用给定种子开局
  2. game_move      执行一次滑动并生成新数字
  3. game_slide     只滑动不生成新数字，供规则与测试使用
  4. game_can_move  判断是否还有合法走法
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#ifndef TERMINAL_GAME_H
#define TERMINAL_GAME_H
#include <stdbool.h>
#include <stdint.h>
/* 滑动方向：GAME_LEFT 左、GAME_RIGHT 右、GAME_UP 上、GAME_DOWN 下 */
typedef enum { GAME_LEFT, GAME_RIGHT, GAME_UP, GAME_DOWN } Direction;
typedef struct {
    uint8_t cells[16]; /* 指数表示法格子值：0 空，1 表示 2，依次到 11 表示 2048 */
    uint32_t score, seed;   /* 当前得分与随机数种子 */
    bool started, won, over; /* 是否已开局、是否已达到 2048、是否已结束 */
} Game;
/* 调用方持有 Game；没有全局随机数，也没有绘制副作用 */
void game_start(Game *game, uint32_t seed);
/* 执行一次滑动，若棋盘发生变化则生成新数字并返回真 */
bool game_move(Game *game, Direction direction);
bool game_slide(Game *game, Direction direction); /* 不生成新数字：规则与测试的边界 */
/* 判断是否还存在合法走法，用于结束对局 */
bool game_can_move(const Game *game);
#endif
