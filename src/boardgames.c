/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   boardgames.c
Author:      汪贤云        Version: 1.0.0       Date: 2026-11-20
Description: 五子棋、井字棋、中国象棋的纯规则层，不含绘制与设备访问。五子棋为
             15×15 棋盘，按四个方向统计连子数与两端开口给空点打分，防守权重略
             高于进攻，电脑因此既能成五也能堵四；井字棋九格全搜索，电脑完美不输；
             中国象棋为 9×10 棋盘，逐棋子类型实现走法，并用试走再撤销的方式判断
             将帅照面与困毙。由于不依赖屏幕，可在几毫秒内链接进测试。
Others:      数组 DIR4 是五子棋的四个方向（横、竖、两条斜线），TOE_LINES 是井字棋
             的八条三连线。规则如果出错，能在任何人触摸面板之前就被测试发现。
Function List:
  1. in_board                 判断行列坐标是否落在棋盘内
  2. gomoku_start             五子棋开局：清空棋盘、设种子、玩家先行
  3. gomoku_winner_at         判断某点落子后是否成五连并返回获胜方
  4. gomoku_full              棋盘是否已满
  5. gomoku_place             在指定点落子并更新胜负与手数
  6. gomoku_line              评估某点在单一方向上的成线价值
  7. gomoku_point             汇总某点四个方向的价值分
  8. gomoku_near              该点附近是否已有棋子，用于收窄搜索
  9. gomoku_rand              线性同余伪随机数，供评分打破平局
 10. gomoku_ai                五子棋电脑走法：评分选点兼顾进攻与防守
 11. toe_start                井字棋开局
 12. toe_result               判断井字棋胜负或平局
 13. toe_win_line             找出获胜三连的三个格子下标
 14. toe_place                井字棋落子并切换手数
 15. toe_search               全搜索求某一方的估值
 16. toe_ai                   井字棋电脑走法：逐空点全搜索取最优
 17. chess_type               返回棋子类型，即去掉正负号后的绝对值
 18. chess_side               返回棋子所属方，零表示空子
 19. chess_start              中国象棋开局摆子
 20. chess_in_palace          判断坐标是否在该方的九宫内
 21. chess_between            统计同一行或同一列两格之间的棋子数
 22. chess_kings_face         将帅是否照面
 23. chess_piece_move_ok      判断某枚棋子按自身走法能否走到目标格
 24. chess_leaves_kings_facing 试走一步后是否造成将帅照面
 25. chess_target_ok          综合判断目标格是否可走或可吃
 26. chess_has_move           判断某方是否还有合法走法
 27. chess_move               执行一步走子并判定终局
History:
  1. Date: 2026-11-20   Author: 汪贤云
     Modification: 初版；注释按华为编程规范改为中文
******************************************************************************/
#include "boardgames.h"
#include <stdlib.h>
#include <string.h>

/* 五子棋的四个方向：横、竖、正斜、反斜 */
static const int DIR4[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

/* 判断行列坐标是否落在指定尺寸的棋盘内 */
static bool in_board(int row, int col, int rows, int cols)
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

/* ---------------------------------------------------------------- 五子棋 */
/* 五子棋开局：清空棋盘，种子为零时取一，玩家执黑先行 */
void gomoku_start(Gomoku *game, uint32_t seed)
{
    if (!game) { return; }
    memset(game->cell, 0, sizeof(game->cell));
    game->seed = seed ? seed : 1u;
    game->turn = 1;
    game->winner = 0;
    game->last = -1;
}

/* 判断在 index 处落子后是否形成五连，返回获胜方；未成五或参数越界返回零 */
int gomoku_winner_at(const Gomoku *game, int index)
{
    if (!game || index < 0 || index >= GOMOKU_CELLS) { return 0; }
    int side = game->cell[index];
    if (side == 0) { return 0; }
    /* 换算成行列，再沿四个方向双向统计连子 */
    int row = index / GOMOKU_N, col = index % GOMOKU_N;
    for (int d = 0; d < 4; ++d) {
        int count = 1;
        for (int sign = -1; sign <= 1; sign += 2) {
            int r = row + DIR4[d][0] * sign, c = col + DIR4[d][1] * sign;
            while (in_board(r, c, GOMOKU_N, GOMOKU_N) && game->cell[r * GOMOKU_N + c] == side) {
                ++count;
                r += DIR4[d][0] * sign;
                c += DIR4[d][1] * sign;
            }
        }
        if (count >= 5) { return side; }
    }
    return 0;
}

/* 棋盘是否已满，用于判定平局 */
static bool gomoku_full(const Gomoku *game)
{
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (!game->cell[i]) { return false; }
    }
    return true;
}

/* 在 index 处落子：更新最后落子点与胜负，越界、占用或对局已结束时拒绝 */
bool gomoku_place(Gomoku *game, int index)
{
    if (!game || game->winner || index < 0 || index >= GOMOKU_CELLS) { return false; }
    if (game->cell[index]) { return false; }
    game->cell[index] = (uint8_t)game->turn;
    game->last = index;
    int winner = gomoku_winner_at(game, index);
    if (winner) {
        game->winner = winner;
    } else if (gomoku_full(game)) {
        game->winner = 3;
    } else {
        game->turn = 3 - game->turn; /* 换手：1 与 2 互换 */
    }
    return true;
}

/* 评估在 (row, col) 处落一 side 子、沿 (dr, dc) 方向的价值：统计连子数以及两端
   是否仍开放。被堵住的四连价值远低于活四，这能避免电脑去做永远成不了的线。 */
static int gomoku_line(const Gomoku *game, int row, int col, int side, int dr, int dc)
{
    int run = 0, open = 0;
    int r = row + dr, c = col + dc;
    while (in_board(r, c, GOMOKU_N, GOMOKU_N) && game->cell[r * GOMOKU_N + c] == side) {
        ++run;
        r += dr;
        c += dc;
    }
    if (in_board(r, c, GOMOKU_N, GOMOKU_N) && !game->cell[r * GOMOKU_N + c]) { ++open; }
    r = row - dr;
    c = col - dc;
    while (in_board(r, c, GOMOKU_N, GOMOKU_N) && game->cell[r * GOMOKU_N + c] == side) {
        ++run;
        r -= dr;
        c -= dc;
    }
    if (in_board(r, c, GOMOKU_N, GOMOKU_N) && !game->cell[r * GOMOKU_N + c]) { ++open; }

    /* 成五或活四、冲四、活三、活二按档位给分，封死的线价值为零 */
    if (run >= 4) { return open >= 1 ? 1000000 : 300000; }
    if (run == 3) { return open == 2 ? 30000 : open == 1 ? 2500 : 0; }
    if (run == 2) { return open == 2 ? 900 : open == 1 ? 120 : 0; }
    return open == 2 ? 30 : 6;
}

/* 汇总某点四个方向的价值，作为该点进攻或防守的得分 */
static int gomoku_point(const Gomoku *game, int index, int side)
{
    int row = index / GOMOKU_N, col = index % GOMOKU_N, total = 0;
    for (int d = 0; d < 4; ++d) {
        total += gomoku_line(game, row, col, side, DIR4[d][0], DIR4[d][1]);
    }
    return total;
}

/* 该点周围两格内是否已有棋子：空盘上所有点得分相同，靠已有棋子把电脑的首手
   收在中心附近，而不是走到角上。 */
static bool gomoku_near(const Gomoku *game, int index)
{
    int row = index / GOMOKU_N, col = index % GOMOKU_N;
    for (int dr = -2; dr <= 2; ++dr) {
        for (int dc = -2; dc <= 2; ++dc) {
            int r = row + dr, c = col + dc;
            if (in_board(r, c, GOMOKU_N, GOMOKU_N) && game->cell[r * GOMOKU_N + c]) { return true; }
        }
    }
    return false;
}

/* 线性同余伪随机数，供评分打破平局，避免电脑每局走法完全一样 */
static uint32_t gomoku_rand(Gomoku *game)
{
    game->seed = game->seed * 1103515245u + 12345u;
    return (game->seed >> 16) & 0x7fffu;
}

/* 五子棋电脑走法：只对已有棋子附近的空点评分，空盘直接落天元。
   防守权重略高于进攻，因此会先堵对手的四连；自己成五的价值更高，仍能优先取胜。 */
bool gomoku_ai(Gomoku *game, int *index)
{
    if (!game || game->winner) { return false; }
    int side = game->turn, foe = 3 - side;
    int empty = -1, best = -1, bestScore = -1;
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (!game->cell[i]) { empty = i; continue; }
    }
    if (empty < 0) { return false; }

    /* first 为真表示盘上还没有任何棋子 */
    bool first = true;
    for (int i = 0; i < GOMOKU_CELLS && first; ++i) {
        if (game->cell[i]) { first = false; }
    }
    if (first) {
        /* 空盘首手落在正中的天元 */
        int centre = (GOMOKU_N / 2) * GOMOKU_N + GOMOKU_N / 2;
        game->cell[centre] = (uint8_t)side;
        game->last = centre;
        game->turn = foe;
        if (index) { *index = centre; }
        return true;
    }

    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (game->cell[i] || !gomoku_near(game, i)) { continue; }
        int attack = gomoku_point(game, i, side);
        int defend = gomoku_point(game, i, foe);
        /* 防守权重略高于进攻，因此会先堵四连；但自己的五连价值更高，仍能优先成五。 */
        int score = attack * 2 + defend * 3 / 2 + (int)(gomoku_rand(game) % 9);
        if (score > bestScore) { bestScore = score; best = i; }
    }
    /* 附近没有可落点时退回任意一个空位 */
    if (best < 0) { best = empty; }
    if (!gomoku_place(game, best)) { return false; }
    if (index) { *index = best; }
    return true;
}

/* ---------------------------------------------------------------- 井字棋 */
/* 井字棋的八条三连线：三行、三列与两条对角线 */
static const int TOE_LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
    {0, 4, 8}, {2, 4, 6}
};

/* 井字棋开局：清空棋盘，玩家执 X 先行 */
void toe_start(Toe *game)
{
    if (!game) { return; }
    memset(game->cell, 0, sizeof(game->cell));
    game->turn = 1;
    game->result = 0;
}

/* 判断井字棋结果：某方三连返回该方，棋盘填满返回平局三，否则返回零 */
int toe_result(Toe *game)
{
    if (!game) { return 3; }
    for (int i = 0; i < 8; ++i) {
        int a = game->cell[TOE_LINES[i][0]];
        if (a && a == game->cell[TOE_LINES[i][1]] && a == game->cell[TOE_LINES[i][2]]) {
            game->result = a;
            return a;
        }
    }
    for (int i = 0; i < TOE_CELLS; ++i) {
        if (!game->cell[i]) { game->result = 0; return 0; }
    }
    game->result = 3;
    return 3;
}

/* 找出第一条三连的三个格子下标，供界面画胜负线；无三连返回假 */
bool toe_win_line(const Toe *game, int *a, int *b, int *c)
{
    if (!game) { return false; }
    for (int i = 0; i < 8; ++i) {
        int p = TOE_LINES[i][0], q = TOE_LINES[i][1], r = TOE_LINES[i][2];
        int value = game->cell[p];
        if (value && value == game->cell[q] && value == game->cell[r]) {
            if (a) { *a = p; }
            if (b) { *b = q; }
            if (c) { *c = r; }
            return true;
        }
    }
    return false;
}

/* 井字棋落子并切换手数；对局已结束、越界或格子被占用时拒绝 */
bool toe_place(Toe *game, int index)
{
    if (!game || game->result || index < 0 || index >= TOE_CELLS) { return false; }
    if (game->cell[index]) { return false; }
    game->cell[index] = (uint8_t)game->turn;
    if (!toe_result(game)) { game->turn = 3 - game->turn; }
    return true;
}

/* 全搜索估值：九格足够少，可以枚举所有后续，因此这个电脑是完美而非仅仅是像样。
   败局尽量往后拖、胜局尽量提前，走起来也因此显得果断。 */
static int toe_search(Toe *game, int turn, int me, int depth)
{
    int result = toe_result(game);
    if (result == me) { return 10 - depth; }
    if (result == 3) { return 0; }
    if (result) { return depth - 10; }
    /* 轮到自己取最大值，轮到对手取最小值 */
    int best = (turn == me) ? -1000 : 1000;
    for (int i = 0; i < TOE_CELLS; ++i) {
        if (game->cell[i]) { continue; }
        /* 试落子、递归、撤销，保持棋盘不变 */
        game->cell[i] = (uint8_t)turn;
        int value = toe_search(game, 3 - turn, me, depth + 1);
        game->cell[i] = 0;
        if (turn == me) {
            if (value > best) { best = value; }
        } else if (value < best) {
            best = value;
        }
    }
    return best;
}

/* 井字棋电脑走法：对每个空点试落子后全搜索，取估值最高的一手 */
bool toe_ai(Toe *game, int *index)
{
    if (!game || game->result) { return false; }
    int me = game->turn, best = -1, bestScore = -1000;
    for (int i = 0; i < TOE_CELLS; ++i) {
        if (game->cell[i]) { continue; }
        game->cell[i] = (uint8_t)me;
        int value = toe_search(game, 3 - me, me, 1);
        game->cell[i] = 0;
        if (value > bestScore) { bestScore = value; best = i; }
    }
    toe_result(game); /* 搜索把 result 留在了某个叶子节点，这里重算回来 */
    if (best < 0) { return false; }
    if (!toe_place(game, best)) { return false; }
    if (index) { *index = best; }
    return true;
}

/* ---------------------------------------------------------------- 中国象棋 */
/* 返回棋子类型，即去掉正负号后的绝对值 */
int chess_type(int piece)
{
    return piece < 0 ? -piece : piece;
}

/* 返回棋子所属方：正为一、负为负一，空子为零 */
int chess_side(int piece)
{
    if (!piece) { return 0; }
    return piece > 0 ? 1 : -1;
}

/* 中国象棋开局摆子：黑方在上、红方在下，红方先走，车马象士将与炮兵各就各位 */
void chess_start(Chess *game)
{
    /* 底线子力依次为车、马、象、士、将、士、象、马、车 */
    static const int8_t back[CHESS_COLS] = {5, 4, 3, 2, 1, 2, 3, 4, 5};
    if (!game) { return; }
    memset(game->cell, 0, sizeof(game->cell));
    /* 黑方底线在上、红方底线在下，符号相反 */
    for (int col = 0; col < CHESS_COLS; ++col) {
        game->cell[0 * CHESS_COLS + col] = (int8_t)-back[col];
        game->cell[9 * CHESS_COLS + col] = back[col];
    }
    /* 黑炮 */
    game->cell[2 * CHESS_COLS + 1] = -6;
    game->cell[2 * CHESS_COLS + 7] = -6;
    /* 红炮 */
    game->cell[7 * CHESS_COLS + 1] = 6;
    game->cell[7 * CHESS_COLS + 7] = 6;
    /* 双方五个兵／卒隔列摆放 */
    for (int col = 0; col < CHESS_COLS; col += 2) {
        game->cell[3 * CHESS_COLS + col] = -7;
        game->cell[6 * CHESS_COLS + col] = 7;
    }
    game->side = 1;
    game->sel = -1;
    game->result = 0;
}

/* 判断坐标是否落在该方的九宫内：红方在下三行，黑方在上三行，列限三到五 */
static bool chess_in_palace(int row, int col, int side)
{
    if (col < 3 || col > 5) { return false; }
    return side > 0 ? (row >= 7 && row <= 9) : (row >= 0 && row <= 2);
}

/* 统计同一行或同一列上、两格之间严格夹着的棋子数 */
static int chess_between(const Chess *game, int from, int to)
{
    int rf = from / CHESS_COLS, cf = from % CHESS_COLS;
    int rt = to / CHESS_COLS, ct = to % CHESS_COLS;
    int count = 0;
    if (rf == rt) {
        int step = ct > cf ? 1 : -1;
        for (int c = cf + step; c != ct; c += step) {
            if (game->cell[rf * CHESS_COLS + c]) { ++count; }
        }
    } else if (cf == ct) {
        int step = rt > rf ? 1 : -1;
        for (int r = rf + step; r != rt; r += step) {
            if (game->cell[r * CHESS_COLS + cf]) { ++count; }
        }
    }
    return count;
}

/* 将帅是否照面：同一列且中间没有任何棋子 */
static bool chess_kings_face(const Chess *game)
{
    int red = -1, black = -1;
    for (int i = 0; i < CHESS_CELLS; ++i) {
        if (game->cell[i] == 1) { red = i; }
        else if (game->cell[i] == -1) { black = i; }
    }
    if (red < 0 || black < 0) { return false; }
    if (red % CHESS_COLS != black % CHESS_COLS) { return false; }
    return chess_between(game, red, black) == 0;
}

/* 判断当前方的这枚棋子按自身走法能否走到目标格，不检查将帅照面 */
bool chess_piece_move_ok(const Chess *game, int from, int to)
{
    if (!game || from < 0 || from >= CHESS_CELLS || to < 0 || to >= CHESS_CELLS) { return false; }
    if (from == to) { return false; }
    int piece = game->cell[from];
    if (!piece || chess_side(piece) != game->side) { return false; }
    int target = game->cell[to];
    /* 目标格是自己的棋子则不能走 */
    if (target && chess_side(target) == chess_side(piece)) { return false; }

    int rf = from / CHESS_COLS, cf = from % CHESS_COLS;
    int rt = to / CHESS_COLS, ct = to % CHESS_COLS;
    int dr = rt - rf, dc = ct - cf;
    int side = chess_side(piece);

    switch (chess_type(piece)) {
    case 1: /* 将／帅：九宫内走一步直线 */
        return abs(dr) + abs(dc) == 1 && chess_in_palace(rt, ct, side);
    case 2: /* 士／仕：九宫内斜走一步 */
        return abs(dr) == 1 && abs(dc) == 1 && chess_in_palace(rt, ct, side);
    case 3: /* 象／相：田字，塞象眼，不过河 */
        if (abs(dr) != 2 || abs(dc) != 2) { return false; }
        if (game->cell[(rf + dr / 2) * CHESS_COLS + (cf + dc / 2)]) { return false; }
        return side > 0 ? rt >= 5 : rt <= 4;
    case 4: { /* 马：日字，蹩马腿 */
        if (!((abs(dr) == 2 && abs(dc) == 1) || (abs(dr) == 1 && abs(dc) == 2))) { return false; }
        int lr = rf + (abs(dr) == 2 ? dr / 2 : 0);
        int lc = cf + (abs(dc) == 2 ? dc / 2 : 0);
        return game->cell[lr * CHESS_COLS + lc] == 0;
    }
    case 5: /* 车：直线，中间不得有子 */
        if (rf != rt && cf != ct) { return false; }
        return chess_between(game, from, to) == 0;
    case 6: /* 炮：不吃子同车；吃子必须隔且只隔一个 */
        if (rf != rt && cf != ct) { return false; }
        if (!target) { return chess_between(game, from, to) == 0; }
        return chess_between(game, from, to) == 1;
    case 7: { /* 兵／卒：向前一步，过河后可横走 */
        int forward = side > 0 ? -1 : 1;
        if (dc == 0 && dr == forward) { return true; }
        bool crossed = side > 0 ? rf <= 4 : rf >= 5;
        return crossed && dr == 0 && abs(dc) == 1;
    }
    default:
        return false;
    }
}

/* 试走一步后是否造成将帅照面：落子、检测、撤销，比预先推算更省事也不易错 */
static bool chess_leaves_kings_facing(Chess *game, int from, int to)
{
    int8_t captured = game->cell[to];
    game->cell[to] = game->cell[from];
    game->cell[from] = 0;
    /* 试走后立刻检测，随后恢复原状 */
    bool facing = chess_kings_face(game);
    game->cell[from] = game->cell[to];
    game->cell[to] = captured;
    return facing;
}

/* 目标格是否可走或可吃：走法合法，且不能让自己的将帅与对方照面；
   吃将不受照面限制，直接判胜 */
bool chess_target_ok(const Chess *game, int from, int to)
{
    if (!game || game->result) { return false; }
    if (!chess_piece_move_ok(game, from, to)) { return false; }
    if (chess_type(game->cell[to]) == 1) { return true; } /* 吃将不受将帅照面限制 */
    /* 在副本上试走并撤销，不改动真实棋盘 */
    Chess copy = *game;
    return !chess_leaves_kings_facing(&copy, from, to);
}

/* 判断指定方是否还有任何合法走法，用于判定困毙负 */
bool chess_has_move(const Chess *game, int side)
{
    Chess copy;
    if (!game) { return false; }
    for (int from = 0; from < CHESS_CELLS; ++from) {
        int piece = game->cell[from];
        if (!piece || chess_side(piece) != side) { continue; }
        for (int to = 0; to < CHESS_CELLS; ++to) {
            /* 每试一种走法都用副本，避免污染原棋盘 */
            copy = *game;
            copy.side = side;
            if (!chess_piece_move_ok(&copy, from, to)) { continue; }
            if (chess_leaves_kings_facing(&copy, from, to)) { continue; }
            return true;
        }
    }
    return false;
}

/* 执行一步走子：吃将即终局，否则换手并检查对方是否被困毙 */
bool chess_move(Chess *game, int from, int to)
{
    if (!game || game->result) { return false; }
    if (!chess_target_ok(game, from, to)) { return false; }
    int mover = game->side;
    if (chess_type(game->cell[to]) == 1) { /* 将死：吃将即终局 */
        game->cell[to] = game->cell[from];
        game->cell[from] = 0;
        game->result = mover;
        game->side = -mover;
        game->sel = -1;
        return true;
    }
    game->cell[to] = game->cell[from];
    game->cell[from] = 0;
    game->side = -mover;
    game->sel = -1;
    /* 对方无子可动即判困毙负 */
    if (!chess_has_move(game, game->side)) { game->result = mover; }
    return true;
}
