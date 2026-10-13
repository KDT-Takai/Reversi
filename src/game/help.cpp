#include "game/help.hpp"

#include "game/dx.hpp"

#include <string>

#include "core/board.hpp"

namespace tidal::ui {

namespace {

// Before/After 具体例: 角の黒石が潮汐で流される局面
Board example_before() {
    Board b = initial_board();
    b[0][0] = Cell::Black;
    b[0][1] = Cell::White;
    b[1][0] = Cell::White;
    b[0][5] = Cell::White;
    b[0][6] = Cell::Black;
    b[2][7] = Cell::Black;
    b[7][3] = Cell::White;
    b[2][2] = Cell::Black;
    return b;
}

void draw_lines(int x, int y, std::initializer_list<const wchar_t*> lines, int font, int pitch) {
    for (const wchar_t* s : lines) {
        draw_text(x, y, s, color::text(), font);
        y += pitch;
    }
}

}  // namespace

const Button& help_close_button() {
    static const Button b{kScreenW / 2 - 140, kScreenH - 78, 280, 50, L"閉じる（Esc / H）"};
    return b;
}

void draw_help(int now_ms, int mouse_x, int mouse_y) {
    const Fonts& f = fonts();
    draw_panel(30, 24, kScreenW - 60, kScreenH - 48);
    draw_text_center(kScreenW / 2, 40, L"潮汐オセロ の遊び方", color::accent(), f.large);

    int y = 100;
    draw_text(70, y, L"■ 基本ルール", color::warn(), f.normal);
    y += 36;
    draw_lines(90, y,
               {L"・自分の石で相手の石をはさむと、はさんだ石が自分の色に変わります。",
                L"・はさめる場所がないときはパス。両者とも置けないか盤が埋まると終局。",
                L"・終局時に石の多いほうが勝ち（同数は引き分け）。"},
               f.caption, 28);
    y += 28 * 3 + 20;

    draw_text(70, y, L"■ 潮汐（このゲームだけのルール）", color::warn(), f.normal);
    y += 36;
    draw_lines(90, y,
               {L"・石を置いた手が 8 手たまるごとに「潮汐」が起きます（パスは数えません）。",
                L"・潮汐では、盤のいちばん外側の輪（青い 28 マス）の石と空きマスが",
                L"   まとめて 1 マス回転します。内側の 6×6 は動きません。",
                L"・回転の向きは 時計回り → 反時計回り → 時計回り … と毎回入れ替わります。",
                L"・次の潮汐までの手数と向きは、画面右に常に表示されます。",
                L"・終局となる手では潮汐は起きません。"},
               f.caption, 28);
    y += 28 * 6 + 20;

    draw_text(70, y, L"■ 戦略ヒント", color::warn(), f.normal);
    y += 36;
    draw_lines(90, y,
               {L"・角は永遠ではありません。取った角も潮汐で辺へ流されます。",
                L"・次の潮汐で角に流れ込む石を読んで、辺を押さえましょう。"},
               f.caption, 28);

    // Before / After の盤面図（core の実データを実描画）
    const Board before = example_before();
    const Board after = rotate_ring(before, Dir::CW);
    const BoardView vb{850.0f, 150.0f, 22.0f};
    const BoardView va{1062.0f, 150.0f, 22.0f};
    draw_text_center(static_cast<int>(vb.x + vb.size() / 2), 112, L"潮汐の直前", color::text(), f.caption);
    draw_text_center(static_cast<int>(va.x + va.size() / 2), 112, L"直後（時計回り）", color::text(), f.caption);
    draw_board_base(vb, now_ms, 0.0f);
    draw_discs(vb, before);
    draw_board_base(va, now_ms, 0.0f);
    draw_discs(va, after);
    draw_dir_icon(1044.0f, 238.0f, 11.0f, Dir::CW, color::accent());

    // 角の黒石の移動を示す丸印
    DrawCircleAA(vb.cx(0), vb.cy(0), 12.0f, 24, color::warn(), FALSE, 3.0f);
    DrawCircleAA(va.cx(1), va.cy(0), 12.0f, 24, color::warn(), FALSE, 3.0f);
    draw_lines(850, 360,
               {L"左上の角にあった黒石は右へ流され、",
                L"左辺の白石が角に入りました。",
                L"内側の石は 1 つも動いていません。"},
               f.caption, 28);

    draw_button(help_close_button(), mouse_x, mouse_y);
}

}  // namespace tidal::ui
