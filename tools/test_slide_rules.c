/* 滑动规则的穷举检查：对每一个方向上每一种列组合，
 * 都与用另一种独立方式编写的参考实现进行比较。
 * Codex, 2026-11-19. */
#include "game.h"
#include <stdio.h>
#include <string.h>

static int failures;

/* 参考实现：从目标边缘开始反复单步移动来实现重力。
 * 刻意采用与 game_slide 不同的遍历方式，以便暴露映射错误。 */
static void reference(int col[4], int dir, int out[4])
{
    int v[4];
    for (int i = 0; i < 4; ++i) { v[i] = col[i]; }
    /* dir：0 向下，1 向上 —— 只涉及纵向，问题正出在这里。 */
    for (int pass = 0; pass < 3; ++pass) {
        if (dir == 0) {
            for (int i = 3; i > 0; --i) { if (v[i] == 0) { v[i] = v[i-1]; v[i-1] = 0; } }
        } else {
            for (int i = 0; i < 3; ++i) { if (v[i] == 0) { v[i] = v[i+1]; v[i+1] = 0; } }
        }
    }
    /* 每个方块只合并一次，从目标边缘开始。 */
    if (dir == 0) {
        for (int i = 3; i > 0; --i) {
            if (v[i] && v[i] == v[i-1]) { ++v[i]; v[i-1] = 0; }
        }
        for (int pass = 0; pass < 3; ++pass) {
            for (int i = 3; i > 0; --i) { if (v[i] == 0) { v[i] = v[i-1]; v[i-1] = 0; } }
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            if (v[i] && v[i] == v[i+1]) { ++v[i]; v[i+1] = 0; }
        }
        for (int pass = 0; pass < 3; ++pass) {
            for (int i = 0; i < 3; ++i) { if (v[i] == 0) { v[i] = v[i+1]; v[i+1] = 0; } }
        }
    }
    for (int i = 0; i < 4; ++i) { out[i] = v[i]; }
}

int main(void)
{
    int pattern = 0;
    for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b)
    for (int c = 0; c < 4; ++c) for (int d = 0; d < 4; ++d) {
        int col[4] = {a, b, c, d};
        for (int dir = 0; dir < 2; ++dir) {
            Game game;
            memset(&game, 0, sizeof(game));
            game.started = true;
            game.seed = 99u;
            for (int i = 0; i < 4; ++i) { game.cells[i*4] = (uint8_t)col[i]; }
            game_slide(&game, dir == 0 ? GAME_DOWN : GAME_UP);
            int want[4];
            reference(col, dir, want);
            for (int i = 0; i < 4; ++i) {
                if (game.cells[i*4] != want[i]) {
                    fprintf(stderr, "MISMATCH dir=%s col=[%d %d %d %d] got=[%d %d %d %d] want=[%d %d %d %d]\n",
                        dir == 0 ? "down" : "up", col[0], col[1], col[2], col[3],
                        game.cells[0], game.cells[4], game.cells[8], game.cells[12],
                        want[0], want[1], want[2], want[3]);
                    ++failures;
                    break;
                }
            }
        }
        ++pattern;
    }
    fprintf(stderr, "%d column patterns checked, %d mismatch(es)\n", pattern, failures);
    return failures ? 1 : 0;
}
