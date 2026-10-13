#pragma once
// 盤面・石・HUD 部品の描画（DxLib 層）

#include <string>
#include <vector>

#include "core/board.hpp"

namespace tidal::ui {

inline constexpr int kScreenW = 1280;
inline constexpr int kScreenH = 720;

// 盤の描画位置とマスサイズ。メイン盤とヘルプの小さな図で共用する。
struct BoardView {
    float x;
    float y;
    float cell;

    float cx(int col) const { return x + cell * (static_cast<float>(col) + 0.5f); }
    float cy(int row) const { return y + cell * (static_cast<float>(row) + 0.5f); }
    float size() const { return cell * kSize; }
    bool hit(int px, int py, Pos& out) const;
};

inline constexpr BoardView kMainBoard{56.0f, 56.0f, 76.0f};

namespace color {
unsigned bg();
unsigned panel();
unsigned panel_edge();
unsigned text();
unsigned text_dim();
unsigned accent();  // 潮汐（海）の色
unsigned warn();
}  // namespace color

struct Fonts {
    int caption = -1;
    int normal = -1;
    int large = -1;
    int huge = -1;
};

void init_fonts();
const Fonts& fonts();

void draw_background(int now_ms);

// ring_glow: 0..1 潮汐リングの強調度（次回潮汐が近いほど強い）
void draw_board_base(const BoardView& v, int now_ms, float ring_glow);
void draw_disc(float cx, float cy, float radius, Cell c, float xscale = 1.0f, float alpha = 1.0f);
void draw_discs(const BoardView& v, const Board& b);
// 合法手マス: 金色の脈打つ枠 + 中央の点（hover 時は強調）
void draw_legal_marker(const BoardView& v, Pos p, int now_ms, bool hover);
// hover 中の合法手に置いた場合のプレビュー石（半透明）
void draw_ghost(const BoardView& v, Pos p, Cell c);
// その手で裏返る石を橙の輪で示す
void draw_flip_preview(const BoardView& v, const std::vector<Pos>& flips);
void draw_last_move(const BoardView& v, Pos p);
// 盤の外周に次回潮汐の向きを示す矢印を描く
void draw_tide_arrows(const BoardView& v, Dir dir, int now_ms, float strength);
// 円弧矢印アイコン（↻/↺）
void draw_dir_icon(float cx, float cy, float r, Dir dir, unsigned col);

int text_width(const std::wstring& s, int font);
void draw_text(int x, int y, const std::wstring& s, unsigned col, int font);
void draw_text_center(int cx, int y, const std::wstring& s, unsigned col, int font);
void draw_panel(int x, int y, int w, int h);

struct Button {
    int x;
    int y;
    int w;
    int h;
    std::wstring label;

    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

void draw_button(const Button& b, int mouse_x, int mouse_y, bool enabled = true, bool selected = false);

}  // namespace tidal::ui
