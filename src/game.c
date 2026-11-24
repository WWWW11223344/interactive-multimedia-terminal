/******************************************************************************
Copyright (c), 2026, 汪贤云
File name:   game.c
Author:      汪贤云        Version: 1.0.1       Date: 2026-11-19
Description: 2048 的纯规则层，不接触界面与触摸。棋盘为 4x4，格子里存的是 2 的指数：
             0 表示空格，1 表示 2，2 表示 4，依此类推。这样合并只需把指数加一，
             得分则是 2 的指数次方，避免每次都做浮点运算。
             每次滑动分三步：压缩空格 -> 相邻同值合并一次 -> 补零。无效滑动不生成
             新牌，也不会误判为走了一步。
Others:      使用本文件内的确定性伪随机数，而不是标准库的 rand()，原因有两个：
             一是测试可复现（同一种子得到同一棋盘），二是不与其他模块共享全局随机
             状态。伪随机只用异或与移位，成本低且没有死等空位的循环。
Function List:
  1. game_start    开局或重开：清空棋盘并生成两张初始牌
  2. game_slide    按四个方向之一压缩并合并，返回棋盘是否变化
  3. game_move     走一步：先滑动，棋盘变了才补新牌，再判断是否无路可走
  4. game_can_move 判断是否还有空格或相邻同值
  5. spawn_tile    在空格中随机位置生成一张牌
  6. next_random   确定性伪随机数
History:
  1. Date: 2026-11-19   Author: 汪贤云
     Modification: 由肖林祥的 game.c 改写：每张牌每步只合并一次、计分改用真实值、
                   无效滑动不补牌、随机数有界
  2. Date: 2026-11-20   Author: 汪贤云
     Modification: 注释改为中文规范格式
******************************************************************************/
#include "game.h"
#include <string.h>

/* 确定性伪随机数：先用种子做异或移位，再把结果存回种子供下一次调用。
   这样同一种子必然得到同一序列，回归测试可以精确复现一整局。 */
static uint32_t next_random(Game *game)
{
    uint32_t value = game->seed;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    game->seed = value;
    return value;
}

/* 在空格中生成一张新牌：先把所有空格的下标收进数组，再按取模随机挑一个。
   这种做法不会死等空位——棋盘已满时 count 为 0，函数直接返回。
   新牌约九成是 2（指数 1），一成是 4（指数 2）。 */
static void spawn_tile(Game *game)
{
    uint8_t empty[16];
    unsigned int count = 0;

    for (unsigned int i = 0; i < 16; ++i) {
        if (!game->cells[i]) { empty[count++] = (uint8_t)i; }
    }
    if (count) {
        unsigned int index = next_random(game) % count;
        game->cells[empty[index]] = next_random(game) % 10 == 0 ? 2 : 1;
    }
}

/* 开局或重新开局：清空全部规则状态，再用传入的种子生成两张初始牌。
   种子为 0 时改用 1，避免伪随机序列退化为全零。 */
void game_start(Game *game, uint32_t seed)
{
    if (!game) { return; }

    *game = (Game){0};
    game->seed = seed ? seed : 1;
    game->started = true;
    spawn_tile(game);
    spawn_tile(game);
}

/* 把四个方向的遍历统一映射成「向左压缩」的同一套算法。
   参数：direction 为滑动方向；line 为行号或列号（0 到 3）；pos 为该线上的位置（0 到 3）。
   返回：这条线上 pos 位置对应的棋盘下标。
   说明：向左时直接取，向右时倒序，向上按列走，向下按列倒序。正因为有了这层映射，
         合并逻辑只需要写一遍，四个方向不会各自出一套 bug。 */
static unsigned int cell_index(Direction direction, unsigned int line, unsigned int pos)
{
    switch (direction) {
        case GAME_LEFT:
            return line * 4 + pos;
        case GAME_RIGHT:
            return line * 4 + 3 - pos;
        case GAME_UP:
            return pos * 4 + line;
        case GAME_DOWN:
            return (3 - pos) * 4 + line;
        default:
            return 0;
    }
}

/* 压缩一行、合并相邻同值、再补零。
   参数：direction 为滑动方向。
   返回：棋盘是否发生变化；只有变化才算有效的一步。
   关键规则：每张牌每步最多合并一次。合并后用 pos 额外跳一格，正是为了防止刚
   合并出来的牌在同一步里再次被合并——这是 2048 最容易写错的地方。
   指数达到 11（即 2048）时置 won，但不再继续合并，避免溢出。 */
bool game_slide(Game *game, Direction direction)
{
    if (!game || direction < GAME_LEFT || direction > GAME_DOWN ||
        !game->started || game->won || game->over) {
        return false;
    }

    /* 先备份，最后用 memcmp 判断这一步是否真的改变了棋盘 */
    uint8_t before[16];
    memcpy(before, game->cells, sizeof(before));

    /* 逐线处理：一条线就是一行或一列，取决于滑动方向 */
    for (unsigned int line = 0; line < 4; ++line) {
        uint8_t packed[4] = {0};
        uint8_t merged[4] = {0};
        unsigned int count = 0;
        unsigned int dest = 0;

        /* 第一步：把非零值挤到线的前端 */
        for (unsigned int pos = 0; pos < 4; ++pos) {
            uint8_t cell = game->cells[cell_index(direction, line, pos)];
            if (cell) { packed[count++] = cell; }
        }

        /* 第二步：相邻同值合并一次，并累计真实得分 */
        for (unsigned int pos = 0; pos < count; ++pos) {
            uint8_t value = packed[pos];

            if (pos + 1 < count && value == packed[pos + 1] && value < 11) {
                ++value;
                ++pos;
                game->score += 1u << value;
                if (value == 11) { game->won = true; }
            }
            merged[dest++] = value;
        }

        /* 第三步：写回棋盘，尾部自然是零 */
        for (unsigned int pos = 0; pos < 4; ++pos) {
            game->cells[cell_index(direction, line, pos)] = merged[pos];
        }
    }
    return memcmp(before, game->cells, sizeof(before)) != 0;
}

/* 判断是否还有下一步：存在空格，或存在水平/垂直相邻的同值牌。
   指数相同即数值相同，所以直接比较格子里的指数即可。 */
bool game_can_move(const Game *game)
{
    if (!game) { return false; }

    for (unsigned int i = 0; i < 16; ++i) {
        if (!game->cells[i] ||
            (i % 4 < 3 && game->cells[i] == game->cells[i + 1]) ||
            (i < 12 && game->cells[i] == game->cells[i + 4])) {
            return true;
        }
    }
    return false;
}

/* 走一步：先滑动；只有棋盘真的变了才补一张新牌。
   棋盘满了且这一步无效时也能正确判负——不能把「没动」当成「还能走」。 */
bool game_move(Game *game, Direction direction)
{
    if (!game || !game->started || game->over || game->won) { return false; }

    bool changed = game_slide(game, direction);
    if (changed && !game->won) { spawn_tile(game); }
    game->over = !game_can_move(game);
    return changed;
}
