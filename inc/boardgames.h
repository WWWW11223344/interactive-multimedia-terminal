/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   boardgames.h
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 棋类规则头文件。五子棋、井字棋、中国象棋的合法性判断与胜负判定集中
             于此，刻意与绘制代码分离：页面只负责落子与画棋子，凡「这步是否合法」
             「谁赢了」的问题都由本文件回答，因此规则可以脱离屏幕单独测试
             （tests/test_boards.c），错误答案会表现为断言失败，而不是等到在面板上
             才发现异常。棋盘坐标统一用一维下标表示，等于行号乘列数加列号。
Others:      棋子与棋盘的编码见各结构体字段注释；电脑走法使用传入的种子，保证
             同一局面可复现，便于回归测试。
Function List:
  1. gomoku_start          五子棋开局，清空棋盘并用给定种子初始化
  2. gomoku_place          当前方在指定下标落子
  3. gomoku_winner_at      返回经过指定下标形成五连的一方
  4. gomoku_ai             为当前方计算一步电脑走法
  5. toe_start             井字棋开局
  6. toe_place             当前方在指定格落子
  7. toe_ai                全搜索电脑走法，不会被击败
  8. toe_result            重算并保存胜负结果
  9. toe_win_line          返回决定胜负的三格，供高亮连线使用
 10. chess_start           中国象棋开局，按标准阵型摆子
 11. chess_piece_move_ok   只判断棋子自身的走法规则
 12. chess_target_ok       棋子规则加白脸将判断，供界面提示可落点
 13. chess_move            完整走子：规则加白脸将判断后换手
 14. chess_has_move        判断某方是否还有合法走法
 15. chess_type            返回棋子类型，空格返回 0
 16. chess_side            返回棋子阵营，+1 红、-1 黑、0 空
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#ifndef TERMINAL_BOARDGAMES_H
#define TERMINAL_BOARDGAMES_H
#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------- 五子棋 */
/* 棋盘边长，15 路标准棋盘 */
#define GOMOKU_N 15
/* 棋盘格总数，等于边长自乘 */
#define GOMOKU_CELLS (GOMOKU_N * GOMOKU_N)

typedef struct
{
    uint8_t cell[GOMOKU_CELLS]; /* 棋盘：0 空，1 黑（玩家），2 白（电脑） */
    uint32_t seed;              /* 随机数种子，使电脑走法可复现 */
    int turn;                   /* 轮到谁：1 黑走，2 白走 */
    int winner;                 /* 胜负：0 未定，1 黑胜，2 白胜，3 平局 */
    int last;                   /* 最后一手棋的下标，首手之前为 -1 */
} Gomoku;

/* 五子棋开局：清空棋盘并用给定种子初始化 */
void gomoku_start(Gomoku *game, uint32_t seed);
/* 让当前方在指定下标落子；已占、越界或对局已结束时返回假 */
bool gomoku_place(Gomoku *game, int index);
/* 返回经过该下标形成五连的一方，没有则返回 0 */
int gomoku_winner_at(const Gomoku *game, int index);
/* 为当前方计算一步电脑走法；棋盘已满或对局已结束时返回假 */
bool gomoku_ai(Gomoku *game, int *index);

/* ---------------------------------------------------------------- 井字棋 */
/* 井字棋盘格总数，3 行 3 列 */
#define TOE_CELLS 9

typedef struct
{
    uint8_t cell[TOE_CELLS]; /* 棋盘：0 空，1 X（玩家），2 O（电脑） */
    int turn;                /* 轮到谁：1 X 走，2 O 走 */
    int result;              /* 结果：0 对局中，1 X 胜，2 O 胜，3 平局 */
} Toe;

/* 井字棋开局：清空棋盘并置 X 先行 */
void toe_start(Toe *game);
/* 当前方在指定格落子；已占、越界或对局已结束时返回假 */
bool toe_place(Toe *game, int index);
/* 完美玩家：搜索整棵博弈树，因此不会被击败 */
bool toe_ai(Toe *game, int *index);
/* 重新计算并保存胜负结果，返回值与保存值相同 */
int toe_result(Toe *game);
/* 返回决定胜负的三格，供界面画高亮连线 */
bool toe_win_line(const Toe *game, int *a, int *b, int *c);

/* ---------------------------------------------------------------- 中国象棋 */
/* 棋盘列数 */
#define CHESS_COLS 9
/* 棋盘行数 */
#define CHESS_ROWS 10
/* 棋盘格总数，等于列数乘行数 */
#define CHESS_CELLS (CHESS_COLS * CHESS_ROWS)

/* 棋盘值正数为红（向上走，行号 9 到 0），负数为黑。类型：1 将，2 士，3 象，
   4 马，5 车，6 炮，7 兵。 */
typedef struct
{
    int8_t cell[CHESS_CELLS]; /* 棋盘，正数红、负数黑，0 空 */
    int side;                 /* 轮到谁：1 红走，-1 黑走 */
    int sel;                  /* 界面当前选中的格子下标，-1 表示未选中 */
    int result;               /* 结果：0 对局中，1 红胜，-1 黑胜 */
} Chess;

/* 中国象棋开局：按标准阵型摆子并置红方先行 */
void chess_start(Chess *game);
/* 只判断棋子自身走法：几何、蹩马腿、九宫与过河，不判断将军 */
bool chess_piece_move_ok(const Chess *game, int from, int to);
/* 棋子规则加白脸将判断，即界面应当提示给玩家的目标点 */
bool chess_target_ok(const Chess *game, int from, int to);
/* 完整走子：先做棋子规则与白脸将判断，再交换行棋方 */
bool chess_move(Chess *game, int from, int to);
/* 判断指定方是否还有合法走法，用于将死或困毙时结束对局 */
bool chess_has_move(const Chess *game, int side);
int chess_type(int piece); /* 返回棋子类型，空格返回 0 */
int chess_side(int piece); /* 返回阵营：+1 红，-1 黑，0 空 */

#endif
