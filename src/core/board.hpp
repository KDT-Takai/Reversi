#pragma once
// 盤面・最外周リング・潮汐回転（純 C++20、DxLib 非依存 — 憲法 I）

#include <array>
#include <cstdint>

namespace tidal {

inline constexpr int kSize = 8;
inline constexpr int kRingSize = 28;

enum class Cell : std::uint8_t { Empty, Black, White };

constexpr Cell opponent(Cell c) {
    return c == Cell::Black ? Cell::White : (c == Cell::White ? Cell::Black : Cell::Empty);
}

struct Pos {
    int row = 0;
    int col = 0;
    friend constexpr bool operator==(Pos, Pos) = default;
};

constexpr bool in_bounds(int row, int col) {
    return row >= 0 && row < kSize && col >= 0 && col < kSize;
}

using Board = std::array<std::array<Cell, kSize>, kSize>;

// 潮汐の回転方向
enum class Dir : std::uint8_t { CW, CCW };

constexpr Dir toggled(Dir d) { return d == Dir::CW ? Dir::CCW : Dir::CW; }

// 標準初期配置: (3,3)/(4,4)=白, (3,4)/(4,3)=黒
constexpr Board initial_board() {
    Board b{};
    for (auto& row : b) row.fill(Cell::Empty);
    b[3][3] = Cell::White;
    b[4][4] = Cell::White;
    b[3][4] = Cell::Black;
    b[4][3] = Cell::Black;
    return b;
}

// 最外周リングを時計回りに列挙:
// 上辺 (0,0)→(0,7)、右辺 (1,7)→(7,7)、下辺 (7,6)→(7,0)、左辺 (6,0)→(1,0)
constexpr std::array<Pos, kRingSize> make_ring() {
    std::array<Pos, kRingSize> ring{};
    int i = 0;
    for (int c = 0; c < kSize; ++c) ring[i++] = {0, c};
    for (int r = 1; r < kSize; ++r) ring[i++] = {r, kSize - 1};
    for (int c = kSize - 2; c >= 0; --c) ring[i++] = {kSize - 1, c};
    for (int r = kSize - 2; r >= 1; --r) ring[i++] = {r, 0};
    return ring;
}

inline constexpr std::array<Pos, kRingSize> kRing = make_ring();

constexpr bool on_ring(Pos p) {
    return p.row == 0 || p.row == kSize - 1 || p.col == 0 || p.col == kSize - 1;
}

// リング上のインデックス（リング外は -1）
constexpr int ring_index(Pos p) {
    for (int i = 0; i < kRingSize; ++i)
        if (kRing[i] == p) return i;
    return -1;
}

// リング 28 マスの内容（石・空）を 1 マス回転する。内側 6x6 は不変。
constexpr Board rotate_ring(const Board& src, Dir dir) {
    Board dst = src;
    const int step = dir == Dir::CW ? 1 : kRingSize - 1;
    for (int i = 0; i < kRingSize; ++i) {
        const Pos from = kRing[i];
        const Pos to = kRing[(i + step) % kRingSize];
        dst[to.row][to.col] = src[from.row][from.col];
    }
    return dst;
}

struct Count {
    int black = 0;
    int white = 0;
};

constexpr Count count(const Board& b) {
    Count n;
    for (const auto& row : b)
        for (Cell c : row) {
            if (c == Cell::Black) ++n.black;
            else if (c == Cell::White) ++n.white;
        }
    return n;
}

constexpr bool is_full(const Board& b) {
    for (const auto& row : b)
        for (Cell c : row)
            if (c == Cell::Empty) return false;
    return true;
}

}  // namespace tidal
