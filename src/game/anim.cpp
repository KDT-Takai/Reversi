#include "game/anim.hpp"

#include <algorithm>
#include <cmath>

#include "game/dx.hpp"

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

void ring_disc_position(const BoardView& v, int ring_idx, Dir dir, float t, float& x, float& y) {
    const float e = ease_in_out(t);
    const int step = dir == Dir::CW ? 1 : kRingSize - 1;
    const Pos from = kRing[ring_idx];
    const Pos to = kRing[(ring_idx + step) % kRingSize];
    x = v.cx(from.col) + (v.cx(to.col) - v.cx(from.col)) * e;
    y = v.cy(from.row) + (v.cy(to.row) - v.cy(from.row)) * e;
}

void draw_tide_slide(const BoardView& v, const Board& before, Dir dir, float t) {
    const float radius = v.cell * 0.42f;
    // 内側 6x6 は動かない
    for (int r = 1; r < kSize - 1; ++r)
        for (int c = 1; c < kSize - 1; ++c) draw_disc(v.cx(c), v.cy(r), radius, before[r][c]);
    for (int i = 0; i < kRingSize; ++i) {
        const Pos from = kRing[i];
        const Cell cell = before[from.row][from.col];
        if (cell == Cell::Empty) continue;
        float x = 0.0f;
        float y = 0.0f;
        ring_disc_position(v, i, dir, t, x, y);
        draw_disc(x, y, radius, cell);
    }
}

void draw_ring_sweep(const BoardView& v, Dir dir, float t) {
    // 光の頭はリング上を一周し、後ろに尾を引く
    constexpr int kTail = 9;
    const float head = t * kRingSize;
    const float sign = dir == Dir::CW ? 1.0f : -1.0f;
    for (int k = 0; k < kTail; ++k) {
        const float pos = head - static_cast<float>(k);
        if (pos < 0.0f) break;
        int idx = static_cast<int>(pos) % kRingSize;
        if (sign < 0) idx = (kRingSize - idx) % kRingSize;
        const Pos p = kRing[idx];
        const float a = (1.0f - static_cast<float>(k) / kTail) * std::sin(3.14159265f * t);
        const float x1 = v.x + v.cell * p.col;
        const float y1 = v.y + v.cell * p.row;
        SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(200 * a));
        DrawBoxAA(x1, y1, x1 + v.cell, y1 + v.cell, GetColor(120, 230, 255), TRUE);
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}

void draw_tide_cutin(Dir dir, float t) {
    // 0..0.2 で左から滑り込み、0.75..1 で右へ抜ける
    float offset = 0.0f;
    if (t < 0.2f) offset = -(1.0f - ease_in_out(t / 0.2f)) * kScreenW;
    else if (t > 0.75f) offset = ease_in_out((t - 0.75f) / 0.25f) * kScreenW;
    const float band_y = 300.0f;
    const float band_h = 120.0f;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
    DrawBoxAA(offset, band_y, offset + kScreenW, band_y + band_h, GetColor(8, 40, 60), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawLineAA(offset, band_y, offset + kScreenW, band_y, color::accent(), 3.0f);
    DrawLineAA(offset, band_y + band_h, offset + kScreenW, band_y + band_h, color::accent(), 3.0f);
    // 帯の中を流れる波線
    for (int k = 0; k < 2; ++k) {
        float px = offset;
        float py = band_y + 30.0f + k * 60.0f;
        for (int x = 16; x <= kScreenW; x += 16) {
            const float fx = offset + static_cast<float>(x);
            const float fy = band_y + 30.0f + k * 60.0f + std::sin(x * 0.03f + t * 20.0f + k) * 6.0f;
            SetDrawBlendMode(DX_BLENDMODE_ADD, 90);
            DrawLineAA(px, py, fx, fy, color::accent(), 2.0f);
            px = fx;
            py = fy;
        }
    }
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    // [↻] 潮　汐 | 外周が時計回りに流れる — 横一列に並べて重ならないようにする
    const std::wstring title = L"潮　汐";
    const std::wstring sub = dir == Dir::CW ? L"外周が時計回りに流れる" : L"外周が反時計回りに流れる";
    const int title_w = text_width(title, fonts().huge);
    const int sub_w = text_width(sub, fonts().normal);
    const int gap = 36;
    const int icon_w = 60;
    const int left = static_cast<int>(offset) + (kScreenW - (icon_w + gap + title_w + gap + sub_w)) / 2;
    draw_dir_icon(static_cast<float>(left + icon_w / 2), band_y + band_h / 2, 28.0f, dir, color::accent());
    draw_text(left + icon_w + gap, static_cast<int>(band_y) + 18, title, color::text(), fonts().huge);
    draw_text(left + icon_w + gap + title_w + gap, static_cast<int>(band_y) + 46, sub, color::accent(),
              fonts().normal);
}

}  // namespace tidal::ui
