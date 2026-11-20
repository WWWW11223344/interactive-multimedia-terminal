/* Board game rules, 2026-11-20. No drawing, no device access: this file can be linked
 * into a test that runs in a few milliseconds and tells you the horse's leg rule is
 * wrong long before anyone touches the panel. */
#include "boardgames.h"
#include <stdlib.h>
#include <string.h>

static const int DIR4[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

static bool in_board(int row, int col, int rows, int cols)
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

/* ---------------------------------------------------------------- 五子棋 */
void gomoku_start(Gomoku *game, uint32_t seed)
{
    if (!game) { return; }
    memset(game->cell, 0, sizeof(game->cell));
    game->seed = seed ? seed : 1u;
    game->turn = 1;
    game->winner = 0;
    game->last = -1;
}

int gomoku_winner_at(const Gomoku *game, int index)
{
    if (!game || index < 0 || index >= GOMOKU_CELLS) { return 0; }
    int side = game->cell[index];
    if (side == 0) { return 0; }
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

static bool gomoku_full(const Gomoku *game)
{
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (!game->cell[i]) { return false; }
    }
    return true;
}

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
        game->turn = 3 - game->turn;
    }
    return true;
}

/* Value of putting a stone here, looking one direction: the run it joins and how many
   of its two ends are still open. A blocked four is worth much less than an open one,
   which is what stops the computer from building a line it can never finish. */
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

    if (run >= 4) { return open >= 1 ? 1000000 : 300000; }
    if (run == 3) { return open == 2 ? 30000 : open == 1 ? 2500 : 0; }
    if (run == 2) { return open == 2 ? 900 : open == 1 ? 120 : 0; }
    return open == 2 ? 30 : 6;
}

static int gomoku_point(const Gomoku *game, int index, int side)
{
    int row = index / GOMOKU_N, col = index % GOMOKU_N, total = 0;
    for (int d = 0; d < 4; ++d) {
        total += gomoku_line(game, row, col, side, DIR4[d][0], DIR4[d][1]);
    }
    return total;
}

/* Only squares near an existing stone are considered: on an empty board every square
   scores the same, and the stone count is what keeps an opening move out in the corner. */
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

static uint32_t gomoku_rand(Gomoku *game)
{
    game->seed = game->seed * 1103515245u + 12345u;
    return (game->seed >> 16) & 0x7fffu;
}

bool gomoku_ai(Gomoku *game, int *index)
{
    if (!game || game->winner) { return false; }
    int side = game->turn, foe = 3 - side;
    int empty = -1, best = -1, bestScore = -1;
    for (int i = 0; i < GOMOKU_CELLS; ++i) {
        if (!game->cell[i]) { empty = i; continue; }
    }
    if (empty < 0) { return false; }

    bool first = true;
    for (int i = 0; i < GOMOKU_CELLS && first; ++i) {
        if (game->cell[i]) { first = false; }
    }
    if (first) {
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
        /* Defending is weighted just above attacking so a four in a row gets blocked,
           while a five of our own still wins the tie and ends the game. */
        int score = attack * 2 + defend * 3 / 2 + (int)(gomoku_rand(game) % 9);
        if (score > bestScore) { bestScore = score; best = i; }
    }
    if (best < 0) { best = empty; }
    if (!gomoku_place(game, best)) { return false; }
    if (index) { *index = best; }
    return true;
}

/* ---------------------------------------------------------------- 井字棋 */
static const int TOE_LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
    {0, 4, 8}, {2, 4, 6}
};

void toe_start(Toe *game)
{
    if (!game) { return; }
    memset(game->cell, 0, sizeof(game->cell));
    game->turn = 1;
    game->result = 0;
}

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

bool toe_place(Toe *game, int index)
{
    if (!game || game->result || index < 0 || index >= TOE_CELLS) { return false; }
    if (game->cell[index]) { return false; }
    game->cell[index] = (uint8_t)game->turn;
    if (!toe_result(game)) { game->turn = 3 - game->turn; }
    return true;
}

/* Full search: nine squares are few enough to look at every continuation, so this
   player is perfect rather than merely decent. Losses are pushed as deep as possible
   and wins as shallow as possible, which is also what makes it feel decisive. */
static int toe_search(Toe *game, int turn, int me, int depth)
{
    int result = toe_result(game);
    if (result == me) { return 10 - depth; }
    if (result == 3) { return 0; }
    if (result) { return depth - 10; }
    int best = (turn == me) ? -1000 : 1000;
    for (int i = 0; i < TOE_CELLS; ++i) {
        if (game->cell[i]) { continue; }
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
    toe_result(game); /* the search left game->result at a leaf value */
    if (best < 0) { return false; }
    if (!toe_place(game, best)) { return false; }
    if (index) { *index = best; }
    return true;
}

/* ---------------------------------------------------------------- 中国象棋 */
int chess_type(int piece)
{
    return piece < 0 ? -piece : piece;
}

int chess_side(int piece)
{
    if (!piece) { return 0; }
    return piece > 0 ? 1 : -1;
}

void chess_start(Chess *game)
{
    static const int8_t back[CHESS_COLS] = {5, 4, 3, 2, 1, 2, 3, 4, 5};
    if (!game) { return; }
    memset(game->cell, 0, sizeof(game->cell));
    for (int col = 0; col < CHESS_COLS; ++col) {
        game->cell[0 * CHESS_COLS + col] = (int8_t)-back[col];
        game->cell[9 * CHESS_COLS + col] = back[col];
    }
    game->cell[2 * CHESS_COLS + 1] = -6;
    game->cell[2 * CHESS_COLS + 7] = -6;
    game->cell[7 * CHESS_COLS + 1] = 6;
    game->cell[7 * CHESS_COLS + 7] = 6;
    for (int col = 0; col < CHESS_COLS; col += 2) {
        game->cell[3 * CHESS_COLS + col] = -7;
        game->cell[6 * CHESS_COLS + col] = 7;
    }
    game->side = 1;
    game->sel = -1;
    game->result = 0;
}

static bool chess_in_palace(int row, int col, int side)
{
    if (col < 3 || col > 5) { return false; }
    return side > 0 ? (row >= 7 && row <= 9) : (row >= 0 && row <= 2);
}

/* Pieces strictly between two squares on the same rank or file. */
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

bool chess_piece_move_ok(const Chess *game, int from, int to)
{
    if (!game || from < 0 || from >= CHESS_CELLS || to < 0 || to >= CHESS_CELLS) { return false; }
    if (from == to) { return false; }
    int piece = game->cell[from];
    if (!piece || chess_side(piece) != game->side) { return false; }
    int target = game->cell[to];
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

/* Apply, test, revert: cheaper and less error-prone than predicting the result. */
static bool chess_leaves_kings_facing(Chess *game, int from, int to)
{
    int8_t captured = game->cell[to];
    game->cell[to] = game->cell[from];
    game->cell[from] = 0;
    bool facing = chess_kings_face(game);
    game->cell[from] = game->cell[to];
    game->cell[to] = captured;
    return facing;
}

bool chess_target_ok(const Chess *game, int from, int to)
{
    if (!game || game->result) { return false; }
    if (!chess_piece_move_ok(game, from, to)) { return false; }
    if (chess_type(game->cell[to]) == 1) { return true; } /* 吃将不受将帅照面限制 */
    Chess copy = *game;
    return !chess_leaves_kings_facing(&copy, from, to);
}

bool chess_has_move(const Chess *game, int side)
{
    Chess copy;
    if (!game) { return false; }
    for (int from = 0; from < CHESS_CELLS; ++from) {
        int piece = game->cell[from];
        if (!piece || chess_side(piece) != side) { continue; }
        for (int to = 0; to < CHESS_CELLS; ++to) {
            copy = *game;
            copy.side = side;
            if (!chess_piece_move_ok(&copy, from, to)) { continue; }
            if (chess_leaves_kings_facing(&copy, from, to)) { continue; }
            return true;
        }
    }
    return false;
}

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
    if (!chess_has_move(game, game->side)) { game->result = mover; }
    return true;
}
