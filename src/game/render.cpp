#include "game/render.hpp"

#include "game/dx.hpp"

#include <algorithm>
#include <cmath>

namespace tidal::ui {

namespace {

constexpr float kPi = 3.14159265f;

Fonts g_fonts;

unsigned rgb(int r, int g, int b) { return GetColor(r, g, b); }

unsigned lerp_rgb(int r1, int g1, int b1, int r2, int g2, int b2, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return rgb(static_cast<int>(r1 + (r2 - r1) * t), static_cast<int>(g1 + (g2 - g1) * t),
               static_cast<int>(b1 + (b2 - b1) * t));
}

void set_alpha(float a) { SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255)); }
void reset_blend() { SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0); }

}  // namespace

bool BoardView::hit(int px, int py, Pos& out) const {
    const float fx = (static_cast<float>(px) - x) / cell;
    const float fy = (static_cast<float>(py) - y) / cell;
    if (fx < 0.0f || fy < 0.0f) return false;
    const int col = static_cast<int>(fx);
    const int row = static_cast<int>(fy);
    if (!in_bounds(row, col)) return false;
    out = {row, col};
    return true;
}

// 配色: 墨の藍を基調に、胡粉の白文字・瑠璃色の潮汐アクセント・金茶の警告（和の印刷物風）
namespace color {
unsigned bg() { return rgb(13, 19, 30); }
unsigned panel() { return rgb(21, 28, 42); }
unsigned panel_edge() { return rgb(122, 116, 100); }
unsigned text() { return rgb(235, 232, 222); }
unsigned text_dim() { return rgb(154, 152, 142); }
unsigned accent() { return rgb(88, 140, 190); }  // 瑠璃色（潮汐）
unsigned warn() { return rgb(228, 188, 112); }   // 金茶
}  // namespace color

void init_fonts() {
    const TCHAR* sans = L"Yu Gothic UI";
    g_fonts.caption = CreateFontToHandle(sans, 18, 3, DX_FONTTYPE_ANTIALIASING_4X4);
    g_fonts.normal = CreateFontToHandle(sans, 24, 4, DX_FONTTYPE_ANTIALIASING_4X4);
    // 見出し系は明朝体。見つからない場合は Gothic にフォールバック。
    const TCHAR* serif = L"Yu Mincho";
    if (CheckFontName(serif, -1) != TRUE) serif = L"MS PMincho";
    g_fonts.large = CreateFontToHandle(serif, 36, 1, DX_FONTTYPE_ANTIALIASING_4X4);
    if (g_fonts.large < 0) g_fonts.large = CreateFontToHandle(sans, 36, 6, DX_FONTTYPE_ANTIALIASING_4X4);
    g_fonts.huge = CreateFontToHandle(serif, 72, 1, DX_FONTTYPE_ANTIALIASING_4X4);
    if (g_fonts.huge < 0) g_fonts.huge = CreateFontToHandle(sans, 72, 8, DX_FONTTYPE_ANTIALIASING_4X4);
}

const Fonts& fonts() { return g_fonts; }

void draw_background(int now_ms) {
    DrawBox(0, 0, kScreenW, kScreenH, color::bg(), TRUE);
    // ゆっくり揺れる波線（潮汐の雰囲気づけ）
    const float t = static_cast<float>(now_ms) / 1000.0f;
    set_alpha(0.10f);
    for (int k = 0; k < 6; ++k) {
        const float base = 120.0f + k * 105.0f;
        float px = 0.0f;
        float py = base + std::sin(t * 0.6f + k) * 10.0f;
        for (int x = 20; x <= kScreenW; x += 20) {
            const float fx = static_cast<float>(x);
            const float fy = base + std::sin(fx * 0.012f + t * 0.6f + k) * 10.0f;
            DrawLineAA(px, py, fx, fy, rgb(52, 92, 132), 1.5f);  // 藍の波線
            px = fx;
            py = fy;
        }
    }
    reset_blend();
}

void draw_board_base(const BoardView& v, int now_ms, float ring_glow) {
    const float t = static_cast<float>(now_ms) / 1000.0f;
    const float s = v.size();
    // 盤の影と枠
    set_alpha(0.45f);
    DrawBoxAA(v.x - 6 + 8, v.y - 6 + 10, v.x + s + 6 + 8, v.y + s + 6 + 10, rgb(0, 0, 0), TRUE);
    reset_blend();
    DrawBoxAA(v.x - 8, v.y - 8, v.x + s + 8, v.y + s + 8, rgb(32, 24, 17), TRUE);
    DrawBoxAA(v.x - 8, v.y - 8, v.x + s + 8, v.y + s + 8, rgb(146, 126, 92), FALSE, 1.5f);  // 枠金の細線

    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) {
            const float x1 = v.x + v.cell * c;
            const float y1 = v.y + v.cell * r;
            unsigned col;
            if (on_ring({r, c})) {
                // リング: 海の色。位置ごとに位相をずらした波で常にゆらぐ
                const int idx = ring_index({r, c});
                const float wave = 0.5f + 0.5f * std::sin(t * 2.2f - idx * 0.45f);
                const float glow = std::clamp(0.15f + 0.35f * wave * (0.4f + ring_glow), 0.0f, 1.0f);
                col = lerp_rgb(24, 62, 102, 52, 112, 152, glow);  // プラシアンの海
            } else {
                col = rgb(24, 74, 52);  // 松葉色の内側
            }
            DrawBoxAA(x1, y1, x1 + v.cell, y1 + v.cell, col, TRUE);
        }
    const unsigned line = rgb(9, 32, 23);
    for (int i = 0; i <= kSize; ++i) {
        const float p = v.cell * i;
        DrawLineAA(v.x + p, v.y, v.x + p, v.y + s, line, 1.5f);
        DrawLineAA(v.x, v.y + p, v.x + s, v.y + p, line, 1.5f);
    }
    // リングと内側 6x6 の境界を強調
    DrawBoxAA(v.x + v.cell, v.y + v.cell, v.x + s - v.cell, v.y + s - v.cell, rgb(198, 196, 182), FALSE, 2.0f);
    if (ring_glow > 0.0f) {
        set_alpha(0.25f + 0.5f * ring_glow * (0.5f + 0.5f * std::sin(t * 6.0f)));
        DrawBoxAA(v.x - 3, v.y - 3, v.x + s + 3, v.y + s + 3, color::accent(), FALSE, 4.0f);
        reset_blend();
    }
    // 星（標準オセロの目印）
    for (int sr : {2, 6})
        for (int sc : {2, 6}) DrawCircleAA(v.x + v.cell * sc, v.y + v.cell * sr, v.cell * 0.06f, 16, line, TRUE);
}

void draw_disc(float cx, float cy, float radius, Cell c, float xscale, float alpha) {
    if (c == Cell::Empty) return;
    const float rx = std::max(radius * std::abs(xscale), 1.0f);
    set_alpha(0.35f * alpha);
    DrawOvalAA(cx + radius * 0.08f, cy + radius * 0.12f, rx, radius, 40, rgb(0, 0, 0), TRUE);
    if (alpha >= 1.0f) reset_blend();
    else set_alpha(alpha);
    if (c == Cell::Black) {
        DrawOvalAA(cx, cy, rx, radius, 40, rgb(28, 28, 32), TRUE);
        DrawOvalAA(cx - rx * 0.25f, cy - radius * 0.3f, rx * 0.45f, radius * 0.28f, 24, rgb(70, 70, 80), TRUE);
    } else {
        DrawOvalAA(cx, cy, rx, radius, 40, rgb(236, 236, 228), TRUE);
        DrawOvalAA(cx - rx * 0.25f, cy - radius * 0.3f, rx * 0.45f, radius * 0.28f, 24, rgb(255, 255, 255), TRUE);
        DrawOvalAA(cx, cy, rx, radius, 40, rgb(170, 170, 160), FALSE, 1.5f);
    }
    reset_blend();
}

void draw_discs(const BoardView& v, const Board& b) {
    for (int r = 0; r < kSize; ++r)
        for (int c = 0; c < kSize; ++c) draw_disc(v.cx(c), v.cy(r), v.cell * 0.42f, b[r][c]);
}

void draw_legal_marker(const BoardView& v, Pos p, int now_ms, bool hover) {
    const float x1 = v.x + v.cell * p.col;
    const float y1 = v.y + v.cell * p.row;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now_ms) / 1000.0f * 4.0f);
    const unsigned gold = rgb(226, 186, 110);  // 金茶
    // マス全体を金色で淡く塗り、枠で囲む（石と見間違えない形にする）
    set_alpha(hover ? 0.40f : 0.16f + 0.12f * pulse);
    DrawBoxAA(x1 + 3, y1 + 3, x1 + v.cell - 3, y1 + v.cell - 3, gold, TRUE);
    set_alpha(hover ? 1.0f : 0.65f + 0.35f * pulse);
    DrawBoxAA(x1 + 4, y1 + 4, x1 + v.cell - 4, y1 + v.cell - 4, gold, FALSE, hover ? 4.0f : 2.5f);
    reset_blend();
    if (!hover) DrawCircleAA(v.cx(p.col), v.cy(p.row), v.cell * 0.09f, 20, gold, TRUE);
}

void draw_ghost(const BoardView& v, Pos p, Cell c) {
    draw_disc(v.cx(p.col), v.cy(p.row), v.cell * 0.42f, c, 1.0f, 0.55f);
}

void draw_flip_preview(const BoardView& v, const std::vector<Pos>& flips) {
    for (Pos p : flips) {
        DrawCircleAA(v.cx(p.col), v.cy(p.row), v.cell * 0.44f, 32, rgb(216, 112, 72), FALSE, 3.0f);  // 朱色
    }
}

void draw_last_move(const BoardView& v, Pos p) {
    DrawCircleAA(v.cx(p.col), v.cy(p.row), v.cell * 0.07f, 16, rgb(216, 84, 70), TRUE);
}

void draw_tide_arrows(const BoardView& v, Dir dir, int now_ms, float strength) {
    // 各辺の外側中央に、リングが動く向きの矢印（CW: 上辺→右、右辺→下、下辺→左、左辺→上）
    const float s = v.size();
    const float off = 26.0f;
    const float phase = std::fmod(static_cast<float>(now_ms) / 600.0f, 1.0f);
    const float sign = dir == Dir::CW ? 1.0f : -1.0f;
    struct Side {
        float cx, cy, dx, dy;
    };
    const Side sides[4] = {
        {v.x + s / 2, v.y - off, sign, 0},        // 上辺
        {v.x + s + off, v.y + s / 2, 0, sign},    // 右辺
        {v.x + s / 2, v.y + s + off, -sign, 0},   // 下辺
        {v.x - off, v.y + s / 2, 0, -sign},       // 左辺
    };
    set_alpha(0.35f + 0.6f * strength);
    for (const auto& sd : sides) {
        for (int k = -1; k <= 1; ++k) {
            const float shift = (k + phase) * 22.0f;
            const float px = sd.cx + sd.dx * shift;
            const float py = sd.cy + sd.dy * shift;
            const float nx = -sd.dy;
            const float ny = sd.dx;
            DrawTriangleAA(px + sd.dx * 8, py + sd.dy * 8, px - sd.dx * 4 + nx * 7, py - sd.dy * 4 + ny * 7,
                           px - sd.dx * 4 - nx * 7, py - sd.dy * 4 - ny * 7, color::accent(), TRUE);
        }
    }
    reset_blend();
}

void draw_dir_icon(float cx, float cy, float r, Dir dir, unsigned col) {
    // 300 度の円弧 + 終端の矢じり
    const int seg = 30;
    const float sign = dir == Dir::CW ? 1.0f : -1.0f;  // 画面座標（y 下向き）では角度増加 = 時計回り
    const float start = -kPi / 2 + sign * 0.5f;
    const float sweep = 2 * kPi - 1.0f;
    float px = cx + r * std::cos(start);
    float py = cy + r * std::sin(start);
    float a = start;
    for (int i = 1; i <= seg; ++i) {
        a = start + sign * sweep * static_cast<float>(i) / seg;
        const float x = cx + r * std::cos(a);
        const float y = cy + r * std::sin(a);
        DrawLineAA(px, py, x, y, col, 4.0f);
        px = x;
        py = y;
    }
    // 接線方向の矢じり
    const float tx = -std::sin(a) * sign;
    const float ty = std::cos(a) * sign;
    const float nx = std::cos(a);
    const float ny = std::sin(a);
    DrawTriangleAA(px + tx * 12, py + ty * 12, px + nx * 9, py + ny * 9, px - nx * 9, py - ny * 9, col, TRUE);
}

int text_width(const std::wstring& s, int font) {
    return GetDrawStringWidthToHandle(s.c_str(), static_cast<int>(s.size()), font);
}

void draw_text(int x, int y, const std::wstring& s, unsigned col, int font) {
    DrawStringToHandle(x, y, s.c_str(), col, font);
}

void draw_text_center(int cx, int y, const std::wstring& s, unsigned col, int font) {
    DrawStringToHandle(cx - text_width(s, font) / 2, y, s.c_str(), col, font);
}

void draw_panel(int x, int y, int w, int h) {
    set_alpha(0.92f);
    DrawRoundRectAA(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w),
                    static_cast<float>(y + h), 5, 5, 16, color::panel(), TRUE);
    reset_blend();
    DrawRoundRectAA(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w),
                    static_cast<float>(y + h), 5, 5, 16, color::panel_edge(), FALSE, 1.5f);
}

void draw_button(const Button& b, int mouse_x, int mouse_y, bool enabled, bool selected) {
    const bool hover = enabled && b.contains(mouse_x, mouse_y);
    unsigned fill = !enabled ? rgb(30, 34, 42) : (hover ? rgb(43, 58, 80) : rgb(26, 36, 52));
    if (selected) fill = rgb(44, 58, 82);
    const unsigned edge = selected ? color::warn() : (hover ? color::accent() : color::panel_edge());
    DrawRoundRectAA(static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.x + b.w),
                    static_cast<float>(b.y + b.h), 5, 5, 16, fill, TRUE);
    DrawRoundRectAA(static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.x + b.w),
                    static_cast<float>(b.y + b.h), 5, 5, 16, edge, FALSE, selected ? 2.5f : 1.5f);
    const int font = fonts().normal;
    const int th = 24;
    draw_text_center(b.x + b.w / 2, b.y + (b.h - th) / 2 - 2, b.label, enabled ? color::text() : color::text_dim(),
                     font);
}

}  // namespace tidal::ui
