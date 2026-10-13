// CoreTests: ルールコアの決定論的テスト（グラフィクスなし — 憲法 I）
// 依存フレームワークなし。失敗が 1 件でもあれば終了コード 1。

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "core/ai.hpp"
#include "core/board.hpp"
#include "core/match.hpp"
#include "core/rules.hpp"

using namespace tidal;

namespace {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

int g_failures_in_test = 0;

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

#define TEST(name)                                   \
    static void name();                              \
    static const Registrar reg_##name(#name, name);  \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures_in_test;                                                \
        }                                                                        \
    } while (0)

Board empty_board() {
    Board b{};
    for (auto& row : b) row.fill(Cell::Empty);
    return b;
}

bool contains(const std::vector<Pos>& v, Pos p) { return std::find(v.begin(), v.end(), p) != v.end(); }

std::vector<Pos> positions(const std::vector<Move>& ms) {
    std::vector<Pos> out;
    for (const auto& m : ms) out.push_back(m.pos);
    return out;
}

}  // namespace

// ---------------------------------------------------------------- 標準ルール (T010)

TEST(initial_board_setup) {
    const Board b = initial_board();
    CHECK(b[3][3] == Cell::White && b[4][4] == Cell::White);
    CHECK(b[3][4] == Cell::Black && b[4][3] == Cell::Black);
    const Count n = count(b);
    CHECK(n.black == 2 && n.white == 2);
}

TEST(initial_legal_moves_black) {
    const auto ps = positions(legal_moves(initial_board(), Cell::Black));
    CHECK(ps.size() == 4);
    CHECK(contains(ps, {2, 3}) && contains(ps, {3, 2}) && contains(ps, {4, 5}) && contains(ps, {5, 4}));
}

TEST(flip_exact_single) {
    const auto f = flips_for(initial_board(), {2, 3}, Cell::Black);
    CHECK(f.size() == 1 && f[0] == (Pos{3, 3}));
}

TEST(flip_multi_direction) {
    Board b = empty_board();
    // 中央 (3,3) に黒を置くと、右方向 2 枚と下方向 1 枚を挟む
    b[3][4] = Cell::White;
    b[3][5] = Cell::White;
    b[3][6] = Cell::Black;
    b[4][3] = Cell::White;
    b[5][3] = Cell::Black;
    b[2][2] = Cell::White;  // 左上方向は終端なし → 反転しない
    const auto f = flips_for(b, {3, 3}, Cell::Black);
    CHECK(f.size() == 3);
    CHECK(contains(f, {3, 4}) && contains(f, {3, 5}) && contains(f, {4, 3}));
    CHECK(!contains(f, {2, 2}));
}

TEST(corner_and_edge_moves) {
    Board b = empty_board();
    b[0][1] = Cell::White;
    b[0][2] = Cell::Black;
    b[1][1] = Cell::White;
    b[2][2] = Cell::Black;
    const auto f = flips_for(b, {0, 0}, Cell::Black);  // 角: 右と右下の 2 方向
    CHECK(f.size() == 2 && contains(f, {0, 1}) && contains(f, {1, 1}));
    CHECK(flips_for(b, {0, 0}, Cell::White).empty());
}

TEST(illegal_moves_rejected) {
    const Board b = initial_board();
    CHECK(flips_for(b, {3, 3}, Cell::Black).empty());  // 埋まっているマス
    CHECK(flips_for(b, {0, 0}, Cell::Black).empty());  // 挟めない
    CHECK(flips_for(b, {-1, 2}, Cell::Black).empty());  // 盤外
    CHECK(flips_for(b, {8, 0}, Cell::Black).empty());
    Match m;
    CHECK(!m.apply_move({0, 0}).has_value());
    CHECK(m.tide.hands_played == 0 && m.turn == Cell::Black);
}

TEST(apply_flip_keeps_source_immutable) {
    const Board b = initial_board();
    const Board before = b;
    const auto ms = legal_moves(b, Cell::Black);
    const Board after = apply_flip(b, ms[0], Cell::Black);
    CHECK(b == before);
    CHECK(after != before);
    const Count n = count(after);
    CHECK(n.black == 4 && n.white == 1);
}

TEST(pass_does_not_advance_tide_clock) {
    Match m;
    m.board = empty_board();
    m.board[0][0] = Cell::Black;
    m.board[0][1] = Cell::White;
    m.turn = Cell::White;  // 白は置けない、黒は (0,2) に置ける
    m.tide.hands_played = 5;
    CHECK(m.must_pass());
    CHECK(m.pass());
    CHECK(m.turn == Cell::Black);
    CHECK(m.tide.hands_played == 5);
    CHECK(m.tide.next_in() == 3);
    CHECK(!m.ended());
    CHECK(!m.pass());  // 黒は合法手があるのでパス不可
}

TEST(consecutive_passes_end_game) {
    Match m;
    m.board = empty_board();
    m.board[0][0] = Cell::Black;
    CHECK(m.must_pass());
    CHECK(m.pass());
    CHECK(m.must_pass());
    CHECK(m.pass());
    CHECK(m.ended());
    CHECK(m.result() == Result::BlackWin);
}

TEST(game_ends_when_no_moves_for_both) {
    Match m;
    m.board = empty_board();
    m.board[0][0] = Cell::Black;
    m.board[0][1] = Cell::White;
    auto ev = m.apply_move({0, 2});
    CHECK(ev.has_value() && ev->ended);
    CHECK(m.ended());
    CHECK(m.result() == Result::BlackWin);
}

TEST(draw_and_win_results) {
    Match m;
    for (int r = 0; r < kSize; ++r) m.board[r].fill(r < 4 ? Cell::Black : Cell::White);
    CHECK(m.result() == Result::Draw);
    m.board[4][0] = Cell::Black;
    CHECK(m.result() == Result::BlackWin);
    m.board[0][0] = Cell::White;
    m.board[0][1] = Cell::White;
    CHECK(m.result() == Result::WhiteWin);
}

// ---------------------------------------------------------------- 潮汐 (T011)

TEST(ring_has_28_distinct_cells_clockwise) {
    for (int i = 0; i < kRingSize; ++i) {
        CHECK(on_ring(kRing[i]));
        for (int j = i + 1; j < kRingSize; ++j) CHECK(!(kRing[i] == kRing[j]));
        // 隣接する列挙要素は盤上で隣り合う（環状）
        const Pos a = kRing[i];
        const Pos b = kRing[(i + 1) % kRingSize];
        CHECK(std::abs(a.row - b.row) + std::abs(a.col - b.col) == 1);
    }
    CHECK(kRing[0] == (Pos{0, 0}) && kRing[7] == (Pos{0, 7}) && kRing[14] == (Pos{7, 7}) &&
          kRing[21] == (Pos{7, 0}) && kRing[27] == (Pos{1, 0}));
}

TEST(rotate_cw_moves_corners_along_edges) {
    Board b = empty_board();
    b[0][0] = Cell::Black;  // 左上角 → 右へ
    b[0][7] = Cell::White;  // 右上角 → 下へ
    b[7][7] = Cell::Black;  // 右下角 → 左へ
    b[7][0] = Cell::White;  // 左下角 → 上へ
    b[1][0] = Cell::Black;  // 左辺 → 左上角へ（辺→角）
    const Board r = rotate_ring(b, Dir::CW);
    CHECK(r[0][1] == Cell::Black);
    CHECK(r[1][7] == Cell::White);
    CHECK(r[7][6] == Cell::Black);
    CHECK(r[6][0] == Cell::White);
    CHECK(r[0][0] == Cell::Black);
    CHECK(r[0][7] == Cell::Empty && r[7][7] == Cell::Empty && r[7][0] == Cell::Empty);
}

TEST(rotate_ccw_is_inverse_of_cw) {
    Board b = initial_board();
    b[0][3] = Cell::Black;
    b[5][7] = Cell::White;
    b[7][0] = Cell::Black;
    const Board ccw = rotate_ring(b, Dir::CCW);
    CHECK(ccw[0][2] == Cell::Black);  // 上辺は左へ
    CHECK(ccw[4][7] == Cell::White);  // 右辺は上へ
    CHECK(ccw[7][1] == Cell::Black);  // 左下角 → 下辺右へ
    CHECK(rotate_ring(ccw, Dir::CW) == b);
    CHECK(rotate_ring(rotate_ring(b, Dir::CW), Dir::CCW) == b);
}

TEST(rotate_preserves_counts_and_inner) {
    Board b = empty_board();
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) b[r][c] = ((r * 7 + c * 3) % 3 == 0) ? Cell::Black : ((r + c) % 2 ? Cell::White : Cell::Empty);
    for (Dir d : {Dir::CW, Dir::CCW}) {
        const Board r = rotate_ring(b, d);
        CHECK(count(r).black == count(b).black && count(r).white == count(b).white);
        for (int row = 1; row < kSize - 1; ++row)
            for (int col = 1; col < kSize - 1; ++col) CHECK(r[row][col] == b[row][col]);
    }
    Board full = b;
    for (int i = 0; i < kRingSize; ++i) full = rotate_ring(full, Dir::CW);
    CHECK(full == b);  // 28 回で一周
}

TEST(tide_fires_on_eighth_move_with_ring_stone) {
    Match m;
    m.board[0][0] = Cell::White;  // リング上の石
    m.tide.hands_played = 7;
    CHECK(m.tide.next_in() == 1);
    auto ev = m.apply_move({2, 3});
    CHECK(ev.has_value());
    CHECK(ev->tide_fired && ev->tide_dir == Dir::CW);
    CHECK(ev->board_after_flip[0][0] == Cell::White);  // 潮汐前の盤
    CHECK(m.board[0][0] == Cell::Empty && m.board[0][1] == Cell::White);  // 潮汐後
    CHECK(m.tide.next_dir == Dir::CCW);
    CHECK(m.tide.next_in() == 8);
}

TEST(tide_fires_exactly_every_8_moves_alternating) {
    Match m;
    std::vector<int> fired_at;
    std::vector<Dir> dirs;
    int guard = 0;
    while (!m.ended() && guard++ < 200) {
        if (m.must_pass()) {
            const int before = m.tide.hands_played;
            m.pass();
            CHECK(m.tide.hands_played == before);
            continue;
        }
        const int next_in_before = m.tide.next_in();
        const auto moves = m.legal();
        // 決定論的だが偏りのある手順: 手数に応じて選ぶ
        const auto& mv = moves[static_cast<size_t>(m.tide.hands_played * 5) % moves.size()];
        auto ev = m.apply_move(mv.pos);
        CHECK(ev.has_value());
        if (ev->tide_fired) {
            fired_at.push_back(m.tide.hands_played);
            dirs.push_back(ev->tide_dir);
            CHECK(next_in_before == 1);  // 表示値と実際の発生が一致（SC-004）
        } else if (!ev->ended) {
            CHECK(next_in_before != 1);
        }
        CHECK(m.tide.next_in() == TideClock::kInterval - m.tide.hands_played % TideClock::kInterval);
    }
    CHECK(m.ended());
    CHECK(fired_at.size() >= 5);
    for (size_t i = 0; i < fired_at.size(); ++i) {
        CHECK(fired_at[i] == static_cast<int>(i + 1) * 8);
        CHECK(dirs[i] == (i % 2 == 0 ? Dir::CW : Dir::CCW));
    }
}

TEST(end_takes_priority_over_tide) {
    Match m;
    m.board = empty_board();
    m.board[0][0] = Cell::Black;
    m.board[0][1] = Cell::White;
    m.tide.hands_played = 7;
    auto ev = m.apply_move({0, 2});
    CHECK(ev.has_value() && ev->ended);
    CHECK(!ev->tide_fired);
    CHECK(m.board[0][0] == Cell::Black && m.board[0][1] == Cell::Black && m.board[0][2] == Cell::Black);
    CHECK(m.tide.next_dir == Dir::CW);  // 発生していないので方向は据え置き
}

TEST(legality_uses_post_tide_board) {
    // 潮汐で石が動いた後の盤で合法手が判定されること
    Match m;
    m.board = initial_board();
    m.board[0][6] = Cell::White;
    m.board[1][7] = Cell::Black;
    m.tide.hands_played = 7;
    auto ev = m.apply_move({2, 3});  // 黒の着手 → 潮汐 CW: (0,6)→(0,7), (1,7)→(2,7)
    CHECK(ev.has_value() && ev->tide_fired);
    CHECK(m.board[0][7] == Cell::White && m.board[2][7] == Cell::Black && m.board[1][7] == Cell::Empty);
    for (const auto& mv : m.legal()) CHECK(!flips_for(m.board, mv.pos, m.turn).empty());
}

// ---------------------------------------------------------------- AI (T023)

namespace {

// 固定シードのランダム手順で局面を作る（終局したらその手前で止める）
Match random_position(std::uint64_t seed, int plies) {
    std::mt19937_64 rng(seed);
    Match m;
    for (int i = 0; i < plies && !m.ended(); ++i) {
        if (m.must_pass()) {
            m.pass();
            continue;
        }
        const auto moves = m.legal();
        Match next = m;
        next.apply_move(moves[std::uniform_int_distribution<size_t>(0, moves.size() - 1)(rng)].pos);
        if (next.ended()) break;
        m = next;
    }
    return m;
}

bool is_legal_for(const Match& m, Pos p) { return !flips_for(m.board, p, m.turn).empty(); }

}  // namespace

TEST(ai_returns_only_legal_moves) {
    int checked = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        Match m = random_position(seed, static_cast<int>(seed % 50));
        if (m.ended() || m.must_pass()) continue;
        CHECK(is_legal_for(m, choose_move(m, Level::Easy, seed)));
        if (seed % 4 == 0) CHECK(is_legal_for(m, choose_move(m, Level::Medium, seed)));
        ++checked;
    }
    CHECK(checked >= 150);
}

TEST(ai_strong_legal_and_cancellable) {
    for (std::uint64_t seed : {7u, 21u}) {
        Match m = random_position(seed, 20);
        if (m.ended() || m.must_pass()) continue;
        CHECK(is_legal_for(m, choose_move(m, Level::Strong, seed)));
    }
    // キャンセル済みなら即座に（途中までの）合法手を返す
    std::atomic<bool> cancel{true};
    Match m = random_position(3, 10);
    const auto t0 = std::chrono::steady_clock::now();
    CHECK(is_legal_for(m, choose_move(m, Level::Strong, 3, &cancel)));
    CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(500));
}

TEST(ai_deterministic_given_seed) {
    for (std::uint64_t seed = 11; seed <= 30; ++seed) {
        Match m = random_position(seed, 12);
        if (m.ended() || m.must_pass()) continue;
        CHECK(choose_move(m, Level::Easy, seed) == choose_move(m, Level::Easy, seed));
        CHECK(choose_move(m, Level::Medium, seed) == choose_move(m, Level::Medium, seed));
    }
}

TEST(ai_eval_sees_imminent_tide) {
    // 黒の石 (0,1) は反時計回りの潮汐で角 (0,0) へ流れ込む
    Match far;
    far.board = initial_board();
    far.board[0][1] = Cell::Black;
    far.tide.hands_played = 3;  // 次回まで 5 手
    far.tide.next_dir = Dir::CCW;
    Match near = far;
    near.tide.hands_played = 7;  // 次回まで 1 手
    CHECK(ai_detail::evaluate(near, Cell::Black) > ai_detail::evaluate(far, Cell::Black));
    // 時計回りなら (0,1)→(0,2) で角には入らない
    Match near_cw = near;
    near_cw.tide.next_dir = Dir::CW;
    CHECK(ai_detail::evaluate(near, Cell::Black) > ai_detail::evaluate(near_cw, Cell::Black));
}

TEST(ai_medium_clearly_beats_easy) {
    int medium_wins = 0;
    constexpr int kGames = 20;
    for (int g = 0; g < kGames; ++g) {
        Match m;
        const Cell medium_color = g % 2 == 0 ? Cell::Black : Cell::White;
        std::uint64_t seed = 1000 + static_cast<std::uint64_t>(g) * 97;
        while (!m.ended()) {
            if (m.must_pass()) {
                m.pass();
                continue;
            }
            const Level lv = m.turn == medium_color ? Level::Medium : Level::Easy;
            m.apply_move(choose_move(m, lv, seed++));
        }
        const Result r = m.result();
        if ((r == Result::BlackWin && medium_color == Cell::Black) ||
            (r == Result::WhiteWin && medium_color == Cell::White))
            ++medium_wins;
    }
    std::printf("    medium won %d / %d vs easy\n", medium_wins, kGames);
    CHECK(medium_wins >= 15);
}

int main() {
    int failed = 0;
    for (const auto& t : registry()) {
        g_failures_in_test = 0;
        t.fn();
        std::printf("[%s] %s\n", g_failures_in_test == 0 ? "PASS" : "FAIL", t.name);
        if (g_failures_in_test != 0) ++failed;
    }
    std::printf("\n%zu tests, %d failed\n", registry().size(), failed);
    return failed == 0 ? 0 : 1;
}
