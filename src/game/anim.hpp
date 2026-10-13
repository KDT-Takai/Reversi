#pragma once
// 反転アニメ・潮汐スライドアニメ（ui-contract.md: Animation contract）

#include <vector>

#include "core/board.hpp"
#include "game/render.hpp"

namespace tidal::ui {

inline constexpr int kFlipMs = 300;
inline constexpr int kTideMs = 500;          // リングのスライド時間
inline constexpr int kTideStageMs = 1000;    // 潮汐演出全体（FR-014）
inline constexpr int kTideSlideStartMs = 250;  // 演出開始からスライド開始まで

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

// 潮汐中のリング石の現在位置（スライド進捗 t）
void ring_disc_position(const BoardView& v, int ring_idx, Dir dir, float t, float& x, float& y);

// 外周を回転方向へ駆け抜ける光の帯（t: 0..1 で一周）
void draw_ring_sweep(const BoardView& v, Dir dir, float t);

// 「潮 汐」カットイン帯（t: 0..1）
void draw_tide_cutin(Dir dir, float t);

}  // namespace tidal::ui
