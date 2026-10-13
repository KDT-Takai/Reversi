#include "game/anim.hpp"

#include <algorithm>
#include <cmath>

namespace tidal::ui {

float Tween::progress(int now) const {
    return std::clamp(static_cast<float>(now - start) / static_cast<float>(duration), 0.0f, 1.0f);
}

float ease_in_out(float t) { return t < 0.5f ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2.0f) / 2; }

void draw_flip(const BoardView& v, const Board& after_flip, Pos placed, const std::vector<Pos>& flips, Cell player,
               float t) {
    const float radius = v.cell * 0.42f;
    const float xscale = std::abs(std::cos(t * 3.14159265f));
    const Cell shown = t < 0.5f ? opponent(player) : player;
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) {
            const Pos p{r, c};
            if (std::find(flips.begin(), flips.end(), p) != flips.end()) {
                draw_disc(v.cx(c), v.cy(r), radius, shown, xscale);
            } else {
                draw_disc(v.cx(c), v.cy(r), radius, after_flip[r][c]);
            }
        }
    draw_last_move(v, placed);
}

void draw_tide_slide(const BoardView& v, const Board& before, Dir dir, float t) {
    const float radius = v.cell * 0.42f;
    const float e = ease_in_out(t);
    // 内側 6x6 は動かない
    for (int r = 1; r < kSize - 1; ++r)
        for (int c = 1; c < kSize - 1; ++c) draw_disc(v.cx(c), v.cy(r), radius, before[r][c]);
    const int step = dir == Dir::CW ? 1 : kRingSize - 1;
    for (int i = 0; i < kRingSize; ++i) {
        const Pos from = kRing[i];
        const Cell cell = before[from.row][from.col];
        if (cell == Cell::Empty) continue;
        const Pos to = kRing[(i + step) % kRingSize];
        const float x = v.cx(from.col) + (v.cx(to.col) - v.cx(from.col)) * e;
        const float y = v.cy(from.row) + (v.cy(to.row) - v.cy(from.row)) * e;
        draw_disc(x, y, radius, cell);
    }
}

}  // namespace tidal::ui
