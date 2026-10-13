#pragma once
// 合法手生成・反転適用（標準オセロ規則。盤面は常に現在の直線として評価する）

#include <vector>

#include "core/board.hpp"

namespace tidal {

struct Move {
    Pos pos;
    std::vector<Pos> flips;  // 反転される石（方向ごと、置いた石に近い順）
};

inline constexpr int kDirs[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                    {0, 1},   {1, -1}, {1, 0},  {1, 1}};

// pos に player が置いた場合に反転する石。空なら非法手。
inline std::vector<Pos> flips_for(const Board& b, Pos pos, Cell player) {
    std::vector<Pos> flips;
    if (!in_bounds(pos.row, pos.col) || b[pos.row][pos.col] != Cell::Empty) return flips;
    const Cell opp = opponent(player);
    for (const auto& d : kDirs) {
        int r = pos.row + d[0];
        int c = pos.col + d[1];
        int run = 0;
        while (in_bounds(r, c) && b[r][c] == opp) {
            r += d[0];
            c += d[1];
            ++run;
        }
        if (run > 0 && in_bounds(r, c) && b[r][c] == player) {
            for (int k = 1; k <= run; ++k) flips.push_back({pos.row + d[0] * k, pos.col + d[1] * k});
        }
    }
    return flips;
}

inline bool is_legal(const Board& b, Pos pos, Cell player) {
    if (!in_bounds(pos.row, pos.col) || b[pos.row][pos.col] != Cell::Empty) return false;
    const Cell opp = opponent(player);
    for (const auto& d : kDirs) {
        int r = pos.row + d[0];
        int c = pos.col + d[1];
        int run = 0;
        while (in_bounds(r, c) && b[r][c] == opp) {
            r += d[0];
            c += d[1];
            ++run;
        }
        if (run > 0 && in_bounds(r, c) && b[r][c] == player) return true;
    }
    return false;
}

// 行優先順（row→col）で合法手を列挙
inline std::vector<Move> legal_moves(const Board& b, Cell player) {
    std::vector<Move> moves;
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) {
            if (b[r][c] != Cell::Empty) continue;
            auto f = flips_for(b, {r, c}, player);
            if (!f.empty()) moves.push_back({{r, c}, std::move(f)});
        }
    return moves;
}

inline int mobility(const Board& b, Cell player) {
    int n = 0;
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            if (is_legal(b, {r, c}, player)) ++n;
    return n;
}

inline bool has_legal_move(const Board& b, Cell player) {
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c)
            if (is_legal(b, {r, c}, player)) return true;
    return false;
}

// 指定手（合法であること）を適用した新しい盤を返す。元の盤は不変。
inline Board apply_flip(const Board& b, const Move& m, Cell player) {
    Board out = b;
    out[m.pos.row][m.pos.col] = player;
    for (Pos p : m.flips) out[p.row][p.col] = player;
    return out;
}

}  // namespace tidal
