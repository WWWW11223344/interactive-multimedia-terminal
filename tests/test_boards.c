/* Board game rules, 2026-11-20. Links boardgames.c only: no framebuffer, no font,
 * no touch device, no App. Runs in milliseconds, so a broken horse-leg or elephant-eye
 * rule is caught on the desktop instead of on the panel.
 *
 * 五子棋 / 井字棋 / 中国象棋共用一个测试程序：三者都在 boardgames.c 里，任何一处回归
 * 都能在宿主机上先暴露。棋盘用 row * 列数 + col 的扁平下标，at() 让测试看起来像棋谱。 */
#include "boardgames.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int at(int row, int col) { return row * GOMOKU_N + col; }

/* Alternating play: a five-in-a-row test only means something if the opponent moves
 * too, otherwise every stone placed would belong to the same side. */
static void pair(Gomoku *game, int mine, int theirs)
{
    assert(gomoku_place(game, mine));
    assert(gomoku_place(game, theirs));
}

static void test_gomoku_win(void)
{
    Gomoku game;
    gomoku_start(&game, 1u);
    assert(game.turn == 1 && game.winner == 0 && game.last == -1);

    /* 横向五连：黑 (7,0)-(7,4)，白在最后一行垫子 */
    for (int i = 0; i < 4; ++i) { pair(&game, at(7, i), at(14, i)); }
    assert(gomoku_winner_at(&game, at(7, 3)) == 0);      /* 四连不算赢 */
    assert(gomoku_place(&game, at(7, 4)) && game.winner == 1);

    /* 竖向五连 */
    gomoku_start(&game, 2u);
    for (int i = 0; i < 4; ++i) { pair(&game, at(3 + i, 3), at(14, i)); }
    assert(gomoku_place(&game, at(7, 3)) && game.winner == 1);

    /* 主对角线五连 */
    gomoku_start(&game, 3u);
    for (int i = 0; i < 4; ++i) { pair(&game, at(4 + i, 4 + i), at(14, i)); }
    assert(gomoku_place(&game, at(8, 8)) && game.winner == 1);

    /* 副对角线五连 */
    gomoku_start(&game, 4u);
    for (int i = 0; i < 4; ++i) { pair(&game, at(2 + i, 8 - i), at(14, i)); }
    assert(gomoku_place(&game, at(6, 4)) && game.winner == 1);

    /* 贴着右边界取胜：边界检查不能把连子截断 */
    gomoku_start(&game, 5u);
    for (int i = 0; i < 4; ++i) { pair(&game, at(0, 10 + i), at(14, i)); }
    assert(gomoku_place(&game, at(0, 14)) && game.winner == 1);

    /* 长连（六连以上）同样算赢：规则层不做禁手判断 */
    gomoku_start(&game, 6u);
    for (int i = 0; i < 4; ++i) { pair(&game, at(9, i), at(14, i)); }
    assert(gomoku_place(&game, at(9, 4)) && game.winner == 1);
    puts("PASS: 五子棋 横/竖/两斜/贴边/长连 判胜，四连不判胜");
}

static void test_gomoku_rules(void)
{
    Gomoku game;
    gomoku_start(&game, 7u);
    assert(gomoku_place(&game, at(7, 7)));
    assert(!gomoku_place(&game, at(7, 7)));              /* 占用格拒绝 */
    assert(!gomoku_place(&game, -1));
    assert(!gomoku_place(&game, GOMOKU_CELLS));

    /* 终局后不再接受落子 */
    gomoku_start(&game, 8u);
    for (int i = 0; i < 4; ++i) { pair(&game, at(5, i), at(14, i)); }
    assert(gomoku_place(&game, at(5, 4)) && game.winner == 1);
    assert(!gomoku_place(&game, at(0, 0)));
    puts("PASS: 五子棋 占用格/越界/终局后落子 一律拒绝");
}

static void test_gomoku_ai(void)
{
    Gomoku game;
    int move = -1;

    /* 空盘首手落在天元 */
    gomoku_start(&game, 9u);
    assert(gomoku_ai(&game, &move));
    assert(move == at(7, 7) && game.cell[move] == 1 && game.turn == 2);

    /* 玩家活四，电脑必须堵其中一端 */
    gomoku_start(&game, 10u);
    for (int c = 3; c <= 6; ++c) { game.cell[at(7, c)] = 1; }
    game.turn = 2;
    assert(gomoku_ai(&game, &move));
    assert(move == at(7, 2) || move == at(7, 7));
    assert(game.cell[move] == 2 && game.turn == 1);

    /* 自己有活四时优先自己成五，而不是去堵对手 */
    gomoku_start(&game, 11u);
    for (int c = 3; c <= 6; ++c) { game.cell[at(4, c)] = 2; }
    for (int c = 3; c <= 6; ++c) { game.cell[at(11, c)] = 1; }
    game.turn = 2;
    assert(gomoku_ai(&game, &move));
    assert(game.winner == 2);

    /* 连续应手：电脑每走一手都把回合交回玩家，不会连走或空手 */
    gomoku_start(&game, 12u);
    assert(gomoku_place(&game, at(7, 7)));
    for (int i = 0; i < 20 && !game.winner; ++i) {
        int reply = -1;
        assert(gomoku_ai(&game, &reply));
        assert(reply >= 0 && reply < GOMOKU_CELLS && game.cell[reply] == 2);
        if (game.winner) { break; }
        assert(game.turn == 1);
        for (int k = 0; k < GOMOKU_CELLS; ++k) {
            if (!game.cell[k]) { assert(gomoku_place(&game, k)); break; }
        }
    }
    puts("PASS: 五子棋 AI 首手天元、堵活四、优先成五、回合交替正确");
}

static void test_toe(void)
{
    Toe game;
    int a = -1, b = -1, c = -1;
    toe_start(&game);
    assert(game.turn == 1 && game.result == 0);

    assert(toe_place(&game, 0) && !toe_place(&game, 0));  /* 占用格拒绝 */
    assert(toe_place(&game, 4) && toe_place(&game, 1) && toe_place(&game, 8));
    assert(!game.result && game.turn == 1);
    assert(toe_place(&game, 2) && game.result == 1);      /* 顶行三连 */
    assert(toe_win_line(&game, &a, &b, &c) && a == 0 && b == 1 && c == 2);
    assert(!toe_place(&game, 3));                          /* 终局后拒绝 */

    /* 斜线取胜 */
    toe_start(&game);
    assert(toe_place(&game, 0) && toe_place(&game, 1) && toe_place(&game, 4));
    assert(toe_place(&game, 2) && toe_place(&game, 8) && game.result == 1);
    assert(toe_win_line(&game, &a, &b, &c) && a == 0 && b == 4 && c == 8);

    /* 平局：整盘填满且无人三连 */
    toe_start(&game);
    {
        static const int draw[9] = {1, 2, 1,
                                    1, 2, 2,
                                    2, 1, 1};
        for (int i = 0; i < 9; ++i) { game.cell[i] = (uint8_t)draw[i]; }
    }
    assert(toe_result(&game) == 3);
    assert(!toe_win_line(&game, NULL, NULL, NULL));
    puts("PASS: 井字棋 三连/占用格/平局/终局锁定/胜利线坐标");
}

/* 玩家（X = 1）先手穷举所有走法，AI 用正式实现对局：返回玩家是否还能赢。
 * 全树搜索的 AI 理论上必平或必胜，所以这一项必须为假。 */
static bool player_can_win(Toe *game)
{
    int result = toe_result(game);
    if (result == 1) { return true; }
    if (result) { return false; }
    if (game->turn == 1) {
        for (int i = 0; i < TOE_CELLS; ++i) {
            if (game->cell[i]) { continue; }
            Toe next = *game;
            assert(toe_place(&next, i));
            if (player_can_win(&next)) { return true; }
        }
        return false;
    }
    Toe next = *game;
    int move = -1;
    if (!toe_ai(&next, &move)) { return false; }
    return player_can_win(&next);
}

static void test_toe_ai(void)
{
    Toe game;
    int move = -1;
    toe_start(&game);
    assert(!player_can_win(&game));

    /* 玩家 0、1 已成两子，电脑必须占 2 */
    toe_start(&game);
    assert(toe_place(&game, 0) && toe_place(&game, 3) && toe_place(&game, 1));
    assert(toe_ai(&game, &move) && move == 2);

    /* 电脑自己有 3、4 成两子时先成三，而不是去堵 */
    toe_start(&game);
    assert(toe_place(&game, 0) && toe_place(&game, 3) && toe_place(&game, 8) &&
           toe_place(&game, 4) && toe_place(&game, 2));
    assert(toe_ai(&game, &move) && move == 5 && game.result == 2);
    puts("PASS: 井字棋 AI 全树搜索，玩家先手无法取胜；能堵也能抢先成三");
}

/* 中国象棋的空盘：只留明确的走子方，方便逐个规则单独验证。 */
static void chess_blank(Chess *game, int side)
{
    memset(game, 0, sizeof(*game));
    game->side = side;
    game->sel = -1;
    game->result = 0;
}

static void test_chess_moves(void)
{
    Chess game;
    chess_start(&game);
    assert(game.cell[9 * CHESS_COLS + 4] == 1 && game.cell[0 * CHESS_COLS + 4] == -1);
    assert(game.cell[6 * CHESS_COLS + 0] == 7 && game.cell[3 * CHESS_COLS + 0] == -7);
    assert(chess_side(1) == 1 && chess_side(-7) == -1 && chess_type(-7) == 7);
    assert(chess_type(0) == 0 && chess_side(0) == 0);

    /* 开局炮二平五（路径无子），随后轮换到黑方并清除选中 */
    assert(chess_move(&game, 7 * CHESS_COLS + 1, 7 * CHESS_COLS + 4));
    assert(game.side == -1 && game.sel == -1);

    /* 马走日；蹩腿只影响被蹩的那两个方向 */
    chess_blank(&game, 1);
    game.cell[4 * CHESS_COLS + 4] = 4;
    assert(chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 2 * CHESS_COLS + 3));
    game.cell[3 * CHESS_COLS + 4] = 7;                       /* 蹩住马腿 */
    assert(!chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 2 * CHESS_COLS + 3));
    assert(!chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 2 * CHESS_COLS + 5));
    assert(chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 3 * CHESS_COLS + 2));
    game.cell[3 * CHESS_COLS + 4] = 0;
    assert(chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 2 * CHESS_COLS + 5));

    /* 象走田，塞象眼不合法，且不能过河 */
    chess_blank(&game, 1);
    game.cell[9 * CHESS_COLS + 2] = 3;
    assert(chess_piece_move_ok(&game, 9 * CHESS_COLS + 2, 7 * CHESS_COLS + 4));
    game.cell[8 * CHESS_COLS + 3] = 1;                       /* 塞象眼 */
    assert(!chess_piece_move_ok(&game, 9 * CHESS_COLS + 2, 7 * CHESS_COLS + 4));
    game.cell[8 * CHESS_COLS + 3] = 0;
    game.cell[7 * CHESS_COLS + 4] = 3;
    assert(chess_piece_move_ok(&game, 7 * CHESS_COLS + 4, 5 * CHESS_COLS + 2));
    game.cell[5 * CHESS_COLS + 2] = 3;
    assert(!chess_piece_move_ok(&game, 5 * CHESS_COLS + 2, 3 * CHESS_COLS + 4));

    /* 车走直线，中间有子走不过去 */
    chess_blank(&game, 1);
    game.cell[5 * CHESS_COLS + 0] = 5;
    game.cell[5 * CHESS_COLS + 3] = 7;                       /* 己方兵挡路 */
    assert(!chess_piece_move_ok(&game, 5 * CHESS_COLS + 0, 5 * CHESS_COLS + 4));
    assert(chess_piece_move_ok(&game, 5 * CHESS_COLS + 0, 5 * CHESS_COLS + 2));

    /* 炮：不吃子同车；吃子必须隔且只隔一个 */
    chess_blank(&game, 1);
    game.cell[7 * CHESS_COLS + 0] = 6;
    game.cell[7 * CHESS_COLS + 6] = -7;                      /* 目标：黑卒 */
    assert(!chess_piece_move_ok(&game, 7 * CHESS_COLS + 0, 7 * CHESS_COLS + 6));
    game.cell[7 * CHESS_COLS + 2] = 7;                       /* 一个炮架 */
    assert(chess_piece_move_ok(&game, 7 * CHESS_COLS + 0, 7 * CHESS_COLS + 6));
    game.cell[7 * CHESS_COLS + 4] = 2;                       /* 第二个炮架 */
    assert(!chess_piece_move_ok(&game, 7 * CHESS_COLS + 0, 7 * CHESS_COLS + 6));

    /* 兵：未过河只能直走，过河后可横走，永远不能后退 */
    chess_blank(&game, 1);
    game.cell[6 * CHESS_COLS + 4] = 7;
    assert(chess_piece_move_ok(&game, 6 * CHESS_COLS + 4, 5 * CHESS_COLS + 4));
    assert(!chess_piece_move_ok(&game, 6 * CHESS_COLS + 4, 6 * CHESS_COLS + 5));
    assert(!chess_piece_move_ok(&game, 6 * CHESS_COLS + 4, 7 * CHESS_COLS + 4));
    game.cell[4 * CHESS_COLS + 4] = 7;
    assert(chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 4 * CHESS_COLS + 5));

    /* 将一步一格不出九宫，士斜走一格不出九宫 */
    chess_blank(&game, 1);
    game.cell[9 * CHESS_COLS + 4] = 1;
    assert(chess_piece_move_ok(&game, 9 * CHESS_COLS + 4, 8 * CHESS_COLS + 4));
    assert(!chess_piece_move_ok(&game, 9 * CHESS_COLS + 4, 9 * CHESS_COLS + 4));
    assert(!chess_piece_move_ok(&game, 9 * CHESS_COLS + 4, 9 * CHESS_COLS + 6));
    game.cell[9 * CHESS_COLS + 3] = 2;
    assert(chess_piece_move_ok(&game, 9 * CHESS_COLS + 3, 8 * CHESS_COLS + 4));
    assert(!chess_piece_move_ok(&game, 9 * CHESS_COLS + 3, 8 * CHESS_COLS + 2));
    assert(!chess_piece_move_ok(&game, 9 * CHESS_COLS + 3, 9 * CHESS_COLS + 5));

    /* 不能吃自己的子，也不能替对方走棋 */
    chess_blank(&game, 1);
    game.cell[5 * CHESS_COLS + 5] = 5;
    game.cell[5 * CHESS_COLS + 8] = 7;
    assert(!chess_piece_move_ok(&game, 5 * CHESS_COLS + 5, 5 * CHESS_COLS + 8));
    game.cell[1 * CHESS_COLS + 0] = -5;
    assert(!chess_piece_move_ok(&game, 1 * CHESS_COLS + 0, 1 * CHESS_COLS + 8));
    puts("PASS: 象棋 马腿/象眼/象不过河/车挡/炮架/兵过河/九宫/越权 全部正确");
}

static void test_chess_face_and_end(void)
{
    Chess game;

    /* 将帅照面：中间那个兵一旦横走，两将相对，因此不允许；沿中间线走则可以 */
    chess_blank(&game, 1);
    game.cell[9 * CHESS_COLS + 4] = 1;
    game.cell[0 * CHESS_COLS + 4] = -1;
    game.cell[4 * CHESS_COLS + 4] = 7;
    assert(chess_piece_move_ok(&game, 4 * CHESS_COLS + 4, 4 * CHESS_COLS + 5));
    assert(!chess_target_ok(&game, 4 * CHESS_COLS + 4, 4 * CHESS_COLS + 5));
    assert(chess_target_ok(&game, 4 * CHESS_COLS + 4, 3 * CHESS_COLS + 4));
    assert(chess_move(&game, 4 * CHESS_COLS + 4, 3 * CHESS_COLS + 4));
    assert(game.cell[3 * CHESS_COLS + 4] == 7 && game.side == -1);

    /* 不在将帅之间的子不受照面限制（中间另有一子长期挡着） */
    chess_blank(&game, 1);
    game.cell[9 * CHESS_COLS + 4] = 1;
    game.cell[0 * CHESS_COLS + 4] = -1;
    game.cell[5 * CHESS_COLS + 4] = 7;
    game.cell[4 * CHESS_COLS + 0] = 7;
    assert(chess_target_ok(&game, 4 * CHESS_COLS + 0, 4 * CHESS_COLS + 1));

    /* 吃将即终局：此后任何落子都被拒绝 */
    chess_blank(&game, 1);
    game.cell[9 * CHESS_COLS + 4] = 1;
    game.cell[0 * CHESS_COLS + 4] = -1;
    game.cell[1 * CHESS_COLS + 4] = 5;
    assert(chess_target_ok(&game, 1 * CHESS_COLS + 4, 0 * CHESS_COLS + 4));
    assert(chess_move(&game, 1 * CHESS_COLS + 4, 0 * CHESS_COLS + 4));
    assert(game.result == 1 && game.side == -1 && game.sel == -1);
    assert(!chess_target_ok(&game, 1 * CHESS_COLS + 4, 1 * CHESS_COLS + 0));
    assert(!chess_move(&game, 1 * CHESS_COLS + 4, 1 * CHESS_COLS + 0));

    /* chess_has_move 供"无子可动判负"使用 */
    chess_start(&game);
    assert(chess_has_move(&game, 1) && chess_has_move(&game, -1));
    chess_blank(&game, -1);
    assert(!chess_has_move(&game, -1) && !chess_has_move(&game, 1));
    puts("PASS: 象棋 将帅照面限制移动、吃将终局、终局锁定、has_move 判定");
}

int main(void)
{
    test_gomoku_win();
    test_gomoku_rules();
    test_gomoku_ai();
    test_toe();
    test_toe_ai();
    test_chess_moves();
    test_chess_face_and_end();
    puts("ALL BOARD RULE TESTS PASSED (rules only; drawing and touch not exercised)");
    return 0;
}
