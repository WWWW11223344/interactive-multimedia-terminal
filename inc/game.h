/* Adapted from Xiao Linxiang's game.c, 2026-11-19: deterministic, bounded 2048 rules. */
#ifndef TERMINAL_GAME_H
#define TERMINAL_GAME_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { GAME_LEFT, GAME_RIGHT, GAME_UP, GAME_DOWN } Direction;
typedef struct {
    uint8_t cells[16]; /* Xiao's exponent representation: 0=blank, 1=2 ... 11=2048. */
    uint32_t score, seed;
    bool started, won, over;
} Game;
/* Caller owns Game; no global RNG or drawing side effects. */
void game_start(Game *game, uint32_t seed);
bool game_move(Game *game, Direction direction);
bool game_slide(Game *game, Direction direction); /* No spawn: rule/test boundary. */
bool game_can_move(const Game *game);
#endif
