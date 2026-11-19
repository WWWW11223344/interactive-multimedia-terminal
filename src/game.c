/* Adapted from Xiao Linxiang game.c: 4x4 exponent board and slide/merge/spawn flow.
 * Codex, 2026-11-19: one merge per tile, true score, no spawn on invalid moves, bounded RNG.
 */
#include "game.h"
#include <string.h>

/* Local deterministic PRNG makes tests repeatable and avoids shared rand() state. */
static uint32_t next_random(Game *game)
{
    uint32_t value = game->seed;
    value ^= value << 13; value ^= value >> 17; value ^= value << 5;
    game->seed = value;
    return value;
}

/* Select from the finite list of empty cells; never spin waiting for a blank. */
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

/* Starting/restarting clears all rules state and produces two initial tiles. */
void game_start(Game *game, uint32_t seed)
{
    if (!game) { return; }
    *game = (Game){0};
    game->seed = seed ? seed : 1;
    game->started = true;
    spawn_tile(game); spawn_tile(game);
}

/* Map a traversal line into the same leftward merge algorithm for all four directions. */
static unsigned int cell_index(Direction direction, unsigned int line, unsigned int pos)
{
    switch (direction) {
        case GAME_LEFT: return line * 4 + pos;
        case GAME_RIGHT: return line * 4 + 3 - pos;
        case GAME_UP: return pos * 4 + line;
        case GAME_DOWN: return (3 - pos) * 4 + line;
        default: return 0;
    }
}

/* Compact once, merge adjacent equal exponents once, then pad with zeros. */
bool game_slide(Game *game, Direction direction)
{
    if (!game || direction < GAME_LEFT || direction > GAME_DOWN || !game->started || game->won || game->over) {
        return false;
    }
    uint8_t before[16];
    memcpy(before, game->cells, sizeof(before));
    for (unsigned int line = 0; line < 4; ++line) {
        uint8_t packed[4] = {0}, merged[4] = {0};
        unsigned int count = 0, dest = 0;
        for (unsigned int pos = 0; pos < 4; ++pos) {
            uint8_t cell = game->cells[cell_index(direction, line, pos)];
            if (cell) { packed[count++] = cell; }
        }
        for (unsigned int pos = 0; pos < count; ++pos) {
            uint8_t value = packed[pos];
            if (pos + 1 < count && value == packed[pos + 1] && value < 11) {
                ++value; ++pos;
                game->score += 1u << value;
                if (value == 11) { game->won = true; }
            }
            merged[dest++] = value;
        }
        for (unsigned int pos = 0; pos < 4; ++pos) {
            game->cells[cell_index(direction, line, pos)] = merged[pos];
        }
    }
    return memcmp(before, game->cells, sizeof(before)) != 0;
}

/* Empty cells or equal horizontal/vertical neighbours mean another move exists. */
bool game_can_move(const Game *game)
{
    if (!game) { return false; }
    for (unsigned int i = 0; i < 16; ++i) {
        if (!game->cells[i] || (i % 4 < 3 && game->cells[i] == game->cells[i+1]) ||
            (i < 12 && game->cells[i] == game->cells[i+4])) { return true; }
    }
    return false;
}

/* Only a changed board earns a new tile; loss also works on a full invalid-move board. */
bool game_move(Game *game, Direction direction)
{
    if (!game || !game->started || game->over || game->won) { return false; }
    bool changed = game_slide(game, direction);
    if (changed && !game->won) { spawn_tile(game); }
    game->over = !game_can_move(game);
    return changed;
}
