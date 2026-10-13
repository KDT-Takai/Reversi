#pragma once
// 反転アニメ・潮汐スライドアニメ（ui-contract.md: Animation contract）

#include <vector>

#include "core/board.hpp"
#include "game/render.hpp"

namespace tidal::ui {

inline constexpr int kFlipMs = 300;
inline constexpr int kTideMs = 500;

struct Tween {
    int start = 0;
    int duration = 1;

    float progress(int now) const;
    bool done(int now) const { return now - start >= duration; }
};

float ease_in_out(float t);

// 反転: 置いた石は即時表示、反転石は横幅 1→0→1 で色が切り替わる（全石同時）
void draw_flip(const BoardView& v, const Board& after_flip, Pos placed, const std::vector<Pos>& flips, Cell player,
               float t);

// 潮汐: リング上の全石が 1 マス分スライドする。before は潮汐前の盤。
void draw_tide_slide(const BoardView& v, const Board& before, Dir dir, float t);

}  // namespace tidal::ui
