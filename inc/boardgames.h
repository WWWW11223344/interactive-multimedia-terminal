/* Board game rules, 2026-11-20: gomoku, tic-tac-toe and Chinese chess.
 *
 * These live apart from the drawing code on purpose. The page files only place stones
 * and paint them; every "is this move allowed" and "did somebody win" question is
 * answered here, so the rules can be exercised without a screen (tests/test_boards.c)
 * and a wrong answer is a failing assertion instead of a bug found on the panel.
 */
#ifndef TERMINAL_BOARDGAMES_H
#define TERMINAL_BOARDGAMES_H
#include <stdbool.h>
#include <stdint.h>

/* ---------------------------------------------------------------- 五子棋 */
#define GOMOKU_N 15
#define GOMOKU_CELLS (GOMOKU_N * GOMOKU_N)

typedef struct {
    uint8_t cell[GOMOKU_CELLS]; /* 0 empty, 1 black (player), 2 white (computer) */
    uint32_t seed;
    int turn;    /* 1 black to move, 2 white to move */
    int winner;  /* 0 none, 1 black, 2 white, 3 draw */
    int last;    /* index of the last stone, -1 before the first move */
} Gomoku;

void gomoku_start(Gomoku *game, uint32_t seed);
/* Place the side to move at index. False when occupied, out of range, or finished. */
bool gomoku_place(Gomoku *game, int index);
/* 0, or the side that has five in a row through index. */
int gomoku_winner_at(const Gomoku *game, int index);
/* Computer move for the side to move; false when the board is full or finished. */
bool gomoku_ai(Gomoku *game, int *index);

/* ---------------------------------------------------------------- 井字棋 */
#define TOE_CELLS 9

typedef struct {
    uint8_t cell[TOE_CELLS]; /* 0 empty, 1 X (player), 2 O (computer) */
    int turn;
    int result; /* 0 playing, 1 X wins, 2 O wins, 3 draw */
} Toe;

void toe_start(Toe *game);
bool toe_place(Toe *game, int index);
/* Perfect player: searches the whole tree, so it can never be beaten. */
bool toe_ai(Toe *game, int *index);
/* Recomputes and stores result; returns the same value. */
int toe_result(Toe *game);
/* The three squares that decided the game, for the highlight line. */
bool toe_win_line(const Toe *game, int *a, int *b, int *c);

/* ---------------------------------------------------------------- 中国象棋 */
#define CHESS_COLS 9
#define CHESS_ROWS 10
#define CHESS_CELLS (CHESS_COLS * CHESS_ROWS)

/* Positive is red (moves up, rows 9..0), negative is black. Types: 1 king, 2 advisor,
   3 elephant, 4 horse, 5 rook, 6 cannon, 7 pawn. */
typedef struct {
    int8_t cell[CHESS_CELLS];
    int side;   /* 1 red to move, -1 black to move */
    int sel;    /* selected square for the UI, -1 when nothing is selected */
    int result; /* 0 playing, 1 red wins, -1 black wins */
} Chess;

void chess_start(Chess *game);
/* Piece rules only: geometry, blocking, palace, river. Does not test for check. */
bool chess_piece_move_ok(const Chess *game, int from, int to);
/* Piece rules plus the flying-general test: what the UI should offer as a target. */
bool chess_target_ok(const Chess *game, int from, int to);
/* Full move: piece rules, plus the flying-general test, then switches the side. */
bool chess_move(Chess *game, int from, int to);
/* Some legal move exists for side (used to end the game on checkmate/stalemate). */
bool chess_has_move(const Chess *game, int side);
int chess_type(int piece);  /* 0 when empty */
int chess_side(int piece);  /* +1 red, -1 black, 0 empty */

#endif
