#pragma once
// AI（易/普/強）: 共通評価関数 + alpha-beta。探索は Match を複製して進めるため、
// 読み筋内の潮汐は実際に発火する（research R4）。

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <random>
#include <vector>

#include "core/board.hpp"
#include "core/match.hpp"
#include "core/rules.hpp"

namespace tidal {

enum class Level { Easy, Medium, Strong };

namespace ai_detail {

inline constexpr int kWeights[kSize][kSize] = {
    {100, -20, 10, 5, 5, 10, -20, 100},
    {-20, -50, -2, -2, -2, -2, -50, -20},
    {10, -2, 1, 1, 1, 1, -2, 10},
    {5, -2, 1, 0, 0, 1, -2, 5},
    {5, -2, 1, 0, 0, 1, -2, 5},
    {10, -2, 1, 1, 1, 1, -2, 10},
    {-20, -50, -2, -2, -2, -2, -50, -20},
    {100, -20, 10, 5, 5, 10, -20, 100},
};

inline constexpr int kWinScore = 1'000'000;
inline constexpr int kInf = INT_MAX / 2;

inline int weight_sum(const Board& b, Cell me) {
    const Cell opp = opponent(me);
    int s = 0;
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) {
            if (b[r][c] == me) s += kWeights[r][c];
            else if (b[r][c] == opp) s -= kWeights[r][c];
        }
    return s;
}

// 手番側 me から見た評価値
inline int evaluate(const Match& m, Cell me) {
    const Count n = count(m.board);
    const int mine = me == Cell::Black ? n.black : n.white;
    const int theirs = me == Cell::Black ? n.white : n.black;
    if (m.ended()) {
        if (mine > theirs) return kWinScore + (mine - theirs);
        if (mine < theirs) return -kWinScore - (theirs - mine);
        return 0;
    }
    int pos = weight_sum(m.board, me);
    // 潮汐近接ボーナス: 次回潮汐が近いなら、回転後の盤の位置価値も同等に見る
    if (m.tide.next_in() <= 4) pos = (pos + weight_sum(rotate_ring(m.board, m.tide.next_dir), me)) / 2;
    const int mob = mobility(m.board, me) - mobility(m.board, opponent(me));
    const int empties = kSize * kSize - n.black - n.white;
    const int disc_weight = empties <= 12 ? 6 : 0;
    return pos + 8 * mob + disc_weight * (mine - theirs);
}

struct Search {
    std::chrono::steady_clock::time_point deadline;
    bool has_deadline = false;
    const std::atomic<bool>* cancel = nullptr;
    std::uint64_t nodes = 0;
    bool aborted = false;

    bool should_stop() {
        if (aborted) return true;
        if ((++nodes & 1023) == 0) {
            if (cancel && cancel->load(std::memory_order_relaxed)) aborted = true;
            if (has_deadline && std::chrono::steady_clock::now() >= deadline) aborted = true;
        }
        return aborted;
    }
};

inline void order_moves(std::vector<Move>& moves) {
    std::stable_sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
        return kWeights[a.pos.row][a.pos.col] > kWeights[b.pos.row][b.pos.col];
    });
}

inline int negamax(const Match& m, int depth, int alpha, int beta, Search& s) {
    if (s.should_stop()) return 0;
    if (m.ended() || depth == 0) return evaluate(m, m.turn);
    if (m.must_pass()) {
        Match child = m;
        child.pass();
        return -negamax(child, depth - 1, -beta, -alpha, s);
    }
    auto moves = m.legal();
    order_moves(moves);
    int best = -kInf;
    for (const auto& mv : moves) {
        Match child = m;
        child.apply_move(mv.pos);
        const int score = -negamax(child, depth - 1, -beta, -alpha, s);
        if (s.aborted) return 0;
        best = std::max(best, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) break;
    }
    return best;
}

struct RootResult {
    Pos best;
    std::vector<std::pair<int, Pos>> scored;  // 降順
    bool complete = false;
};

inline RootResult search_root(const Match& m, int depth, Search& s, const std::vector<Move>& ordered) {
    RootResult res;
    int alpha = -kInf;
    for (const auto& mv : ordered) {
        Match child = m;
        child.apply_move(mv.pos);
        const int score = -negamax(child, depth - 1, -kInf, -alpha, s);
        if (s.aborted) return res;
        res.scored.push_back({score, mv.pos});
        alpha = std::max(alpha, score);
    }
    std::stable_sort(res.scored.begin(), res.scored.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    res.best = res.scored.front().second;
    res.complete = true;
    return res;
}

}  // namespace ai_detail

// 手番側の手を選ぶ。合法手が 1 つ以上あること（呼び出し側が保証）。
// 返す手は必ず legal_moves の要素。cancel が立つと途中までの最善手を返す。
inline Pos choose_move(const Match& m, Level level, std::uint64_t seed,
                       const std::atomic<bool>* cancel = nullptr) {
    using namespace ai_detail;
    auto moves = m.legal();
    order_moves(moves);
    if (moves.size() == 1) return moves.front().pos;

    std::mt19937_64 rng(seed);
    Search s;
    s.cancel = cancel;

    if (level == Level::Easy) {
        // 初級者が勝てる強さ: 一定確率で完全ランダム、それ以外は 1 手読みの上位 3 手から重み付き抽選
        if (std::uniform_int_distribution<int>(0, 99)(rng) < 35)
            return moves[std::uniform_int_distribution<size_t>(0, moves.size() - 1)(rng)].pos;
        const RootResult r = search_root(m, 1, s, moves);
        const size_t k = std::min<size_t>(3, r.scored.size());
        static constexpr int kPick[3] = {3, 2, 1};
        int total = 0;
        for (size_t i = 0; i < k; ++i) total += kPick[i];
        int roll = std::uniform_int_distribution<int>(0, total - 1)(rng);
        for (size_t i = 0; i < k; ++i) {
            if (roll < kPick[i]) return r.scored[i].second;
            roll -= kPick[i];
        }
        return r.scored.front().second;
    }

    if (level == Level::Medium) {
        const RootResult r = search_root(m, 4, s, moves);
        return r.complete ? r.best : moves.front().pos;
    }

    // Strong: 反復深化（最大深さ 7、思考時間上限 2.5 秒）
    s.has_deadline = true;
    s.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    Pos best = moves.front().pos;
    for (int depth = 1; depth <= 7; ++depth) {
        const RootResult r = search_root(m, depth, s, moves);
        if (!r.complete) break;
        best = r.best;
        // 前回の最善手を先頭に並べ替えて枝刈り効率を上げる
        std::stable_partition(moves.begin(), moves.end(), [&](const Move& mv) { return mv.pos == best; });
    }
    return best;
}

}  // namespace tidal
