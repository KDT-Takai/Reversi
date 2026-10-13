#include "game/scenes.hpp"

#include "game/dx.hpp"

#include <algorithm>
#include <format>
#include <string>

#include "game/help.hpp"
#include "game/sound.hpp"

namespace tidal::ui {

namespace {

constexpr int kAiMinThinkMs = 450;  // AI が即答しても一瞬で打たれないための最低待ち
constexpr int kPassNoticeMs = 1500;
constexpr int kEndDelayMs = 900;
constexpr int kTitleTideEveryMs = 1600;

// ---- ボタン配置
const Button kTitleStart{140, 330, 320, 60, L"対局を始める"};
const Button kTitleHelp{140, 410, 320, 60, L"遊び方（H）"};
const Button kTitleQuit{140, 490, 320, 60, L"終了"};

const Button kModeEasy{240, 250, 240, 64, L"易"};
const Button kModeMedium{520, 250, 240, 64, L"普"};
const Button kModeStrong{800, 250, 240, 64, L"強"};
const Button kModeHotSeat{440, 440, 400, 64, L"2人で対局（同じ画面）"};
const Button kModeBack{540, 590, 200, 52, L"戻る（Esc）"};

const Button kSetupStart{490, 420, 300, 64, L"対局開始"};
const Button kSetupP1Black{300, 400, 320, 64, L"プレイヤー1 が黒"};
const Button kSetupP2Black{660, 400, 320, 64, L"プレイヤー2 が黒"};
const Button kSetupBack{540, 560, 200, 52, L"戻る（Esc）"};

constexpr int kHudX = 716;
constexpr int kHudY = 56;
constexpr int kHudW = 508;
constexpr int kHudH = 608;
const Button kHudHelp{kHudX + 24, kHudY + kHudH - 76, 220, 52, L"ヘルプ（H）"};
const Button kHudQuit{kHudX + kHudW - 244, kHudY + kHudH - 76, 220, 52, L"やめる（Esc）"};

const Button kResultAgain{kHudX + 24, kHudY + kHudH - 76, 220, 52, L"もう一度"};
const Button kResultTitle{kHudX + kHudW - 244, kHudY + kHudH - 76, 220, 52, L"タイトルへ"};

const wchar_t* level_name(Level lv) {
    switch (lv) {
        case Level::Easy: return L"易";
        case Level::Medium: return L"普";
        default: return L"強";
    }
}

Board make_title_board() {
    Board b = initial_board();
    const Cell pattern[] = {Cell::Black, Cell::Empty, Cell::White, Cell::Black, Cell::Empty, Cell::White, Cell::White};
    for (int i = 0; i < kRingSize; ++i) {
        const Pos p = kRing[i];
        b[p.row][p.col] = pattern[i % 7];
    }
    return b;
}

}  // namespace

// ============================================================ AiWorker

void AiWorker::start(const Match& m, Level level, std::uint64_t seed) {
    cancel();
    cancel_.store(false);
    ready_.store(false);
    running_ = true;
    thread_ = std::jthread([this, m, level, seed] {
        result_ = choose_move(m, level, seed, &cancel_);
        ready_.store(true, std::memory_order_release);
    });
}

bool AiWorker::poll(Pos& out) {
    if (!running_ || !ready_.load(std::memory_order_acquire)) return false;
    out = result_;
    running_ = false;
    return true;
}

void AiWorker::cancel() {
    cancel_.store(true);
    if (thread_.joinable()) thread_.join();
    running_ = false;
    ready_.store(false);
}

// ============================================================ App

App::App() : rng_(std::random_device{}()), title_board_(make_title_board()) {}

void App::update(const Input& in, int now_ms) {
    const int dt = last_now_ < 0 ? 0 : std::clamp(now_ms - last_now_, 0, 100);
    last_now_ = now_ms;
    now_ms_ += dt;
    mouse_x_ = in.mouse_x;
    mouse_y_ = in.mouse_y;

    switch (scene_) {
        case Scene::Title: update_title(in); break;
        case Scene::ModeSelect: update_mode_select(in); break;
        case Scene::MatchSetup: update_setup(in); break;
        case Scene::Playing:
            play_ms_ += dt;
            update_playing(in);
            break;
        case Scene::Help:
            if (in.key_back || in.key_help || (in.clicked && help_close_button().contains(in.mouse_x, in.mouse_y)))
                scene_ = help_return_;
            break;
        case Scene::Result: update_result(in); break;
    }
}

void App::open_help() {
    help_return_ = scene_;
    scene_ = Scene::Help;
}

// ------------------------------------------------------------ Title / Mode / Setup

void App::update_title(const Input& in) {
    if (title_tween_.done(now_ms_)) {
        // 装飾盤: 一定間隔で潮汐を繰り返す（初回は待機のみ）
        if (title_tween_.duration > 1) {
            title_board_ = rotate_ring(title_board_, title_dir_);
            title_dir_ = toggled(title_dir_);
        }
        title_tween_ = {now_ms_ + kTitleTideEveryMs, kTideMs};
    }
    if (in.key_help) return open_help();
    if (!in.clicked) return;
    if (kTitleStart.contains(in.mouse_x, in.mouse_y)) scene_ = Scene::ModeSelect;
    else if (kTitleHelp.contains(in.mouse_x, in.mouse_y)) open_help();
    else if (kTitleQuit.contains(in.mouse_x, in.mouse_y)) quit_ = true;
}

void App::update_mode_select(const Input& in) {
    if (in.key_back) {
        scene_ = Scene::Title;
        return;
    }
    if (!in.clicked) return;
    auto choose_ai = [&](Level lv) {
        mode_ = Mode::VsAi;
        level_ = lv;
        // 対 AI は石の色をランダム決定（FR-006）
        human_color_ = std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? Cell::Black : Cell::White;
        scene_ = Scene::MatchSetup;
    };
    if (kModeEasy.contains(in.mouse_x, in.mouse_y)) choose_ai(Level::Easy);
    else if (kModeMedium.contains(in.mouse_x, in.mouse_y)) choose_ai(Level::Medium);
    else if (kModeStrong.contains(in.mouse_x, in.mouse_y)) choose_ai(Level::Strong);
    else if (kModeHotSeat.contains(in.mouse_x, in.mouse_y)) {
        mode_ = Mode::HotSeat;
        scene_ = Scene::MatchSetup;
    } else if (kModeBack.contains(in.mouse_x, in.mouse_y)) {
        scene_ = Scene::Title;
    }
}

void App::update_setup(const Input& in) {
    if (in.key_back) {
        scene_ = Scene::ModeSelect;
        return;
    }
    if (!in.clicked) return;
    if (kSetupBack.contains(in.mouse_x, in.mouse_y)) {
        scene_ = Scene::ModeSelect;
        return;
    }
    if (mode_ == Mode::VsAi) {
        if (kSetupStart.contains(in.mouse_x, in.mouse_y)) start_match();
    } else {
        if (kSetupP1Black.contains(in.mouse_x, in.mouse_y)) {
            human_color_ = Cell::Black;
            start_match();
        } else if (kSetupP2Black.contains(in.mouse_x, in.mouse_y)) {
            human_color_ = Cell::White;
            start_match();
        }
    }
}

// ------------------------------------------------------------ Playing

void App::start_match() {
    ai_.cancel();
    match_ = Match{};
    has_last_move_ = false;
    tides_fired_ = 0;
    play_ms_ = 0;
    scene_ = Scene::Playing;
    begin_turn();
}

void App::leave_match(Scene to) {
    ai_.cancel();
    ai_has_result_ = false;
    scene_ = to;
}

bool App::human_turn() const { return mode_ == Mode::HotSeat || match_.turn == human_color_; }

void App::begin_turn() {
    turn_start_ms_ = play_ms_;
    ai_has_result_ = false;
    if (match_.must_pass()) {
        stage_ = Stage::PassNotice;
        pass_player_ = match_.turn;
        tween_ = {play_ms_, kPassNoticeMs};
        sound::play(sound::Se::Pass);
        return;
    }
    stage_ = Stage::Idle;
    if (!human_turn()) ai_.start(match_, level_, rng_());
}

void App::play_move(Pos pos) {
    auto ev = match_.apply_move(pos);
    if (!ev) return;  // 非法手は無視（FR-009）
    event_ = std::move(*ev);
    has_last_move_ = true;
    last_move_ = pos;
    stage_ = Stage::Flip;
    tween_ = {play_ms_, kFlipMs};
    if (event_.tide_fired) ++tides_fired_;
    sound::play(sound::Se::Place);
    if (!event_.flips.empty()) sound::play(sound::Se::Flip);
}

void App::finish_event() {
    if (event_.tide_fired && on_ring(last_move_)) {
        // 直前の着手がリング上なら、印も潮汐で一緒に動かす
        const int step = event_.tide_dir == Dir::CW ? 1 : kRingSize - 1;
        last_move_ = kRing[(ring_index(last_move_) + step) % kRingSize];
    }
    if (match_.ended()) {
        stage_ = Stage::EndDelay;
        tween_ = {play_ms_, kEndDelayMs};
        return;
    }
    begin_turn();
}

void App::update_playing(const Input& in) {
    if (in.key_help || (in.clicked && kHudHelp.contains(in.mouse_x, in.mouse_y))) return open_help();
    if (in.key_back || (in.clicked && kHudQuit.contains(in.mouse_x, in.mouse_y)))
        return leave_match(Scene::ModeSelect);

    switch (stage_) {
        case Stage::Flip:
            if (tween_.done(play_ms_)) {
                if (event_.tide_fired) {
                    stage_ = Stage::Tide;
                    tween_ = {play_ms_, kTideMs};
                    sound::play(sound::Se::Tide);
                } else {
                    finish_event();
                }
            }
            break;
        case Stage::Tide:
            if (tween_.done(play_ms_)) finish_event();
            break;
        case Stage::PassNotice:
            if (tween_.done(play_ms_)) {
                match_.pass();
                if (match_.ended()) {
                    stage_ = Stage::EndDelay;
                    tween_ = {play_ms_, kEndDelayMs};
                } else {
                    begin_turn();
                }
            }
            break;
        case Stage::EndDelay:
            if (tween_.done(play_ms_)) {
                scene_ = Scene::Result;
                const Result r = match_.result();
                if (r == Result::Draw) sound::play(sound::Se::Draw);
                else if (mode_ == Mode::VsAi && (r == Result::BlackWin) != (human_color_ == Cell::Black))
                    sound::play(sound::Se::Lose);
                else sound::play(sound::Se::Win);
            }
            break;
        case Stage::Idle:
            if (human_turn()) {
                Pos p;
                if (in.clicked && kMainBoard.hit(in.mouse_x, in.mouse_y, p)) play_move(p);
            } else {
                if (!ai_has_result_ && ai_.poll(ai_result_)) ai_has_result_ = true;
                if (ai_has_result_ && play_ms_ - turn_start_ms_ >= kAiMinThinkMs) {
                    ai_has_result_ = false;
                    play_move(ai_result_);
                }
            }
            break;
    }
}

void App::update_result(const Input& in) {
    if (in.key_back) return leave_match(Scene::Title);
    if (!in.clicked) return;
    if (kResultAgain.contains(in.mouse_x, in.mouse_y)) leave_match(Scene::ModeSelect);
    else if (kResultTitle.contains(in.mouse_x, in.mouse_y)) leave_match(Scene::Title);
}

// ------------------------------------------------------------ 表示用ラベル

std::wstring App::color_name(Cell c) const { return c == Cell::Black ? L"黒" : L"白"; }

std::wstring App::player_label(Cell c) const {
    if (mode_ == Mode::VsAi) return c == human_color_ ? L"あなた" : L"CPU";
    return c == human_color_ ? L"プレイヤー1" : L"プレイヤー2";
}

// ============================================================ Draw

void App::draw() const {
    draw_background(now_ms_);
    switch (scene_) {
        case Scene::Title: draw_title(); break;
        case Scene::ModeSelect: draw_mode_select(); break;
        case Scene::MatchSetup: draw_setup(); break;
        case Scene::Playing:
            draw_playing_board();
            draw_hud();
            break;
        case Scene::Help:
            if (help_return_ == Scene::Playing) {
                draw_playing_board();
                draw_hud();
            } else {
                draw_title();
            }
            draw_help(now_ms_, mouse_x_, mouse_y_);
            break;
        case Scene::Result:
            draw_playing_board();
            draw_result();
            break;
    }
}

void App::draw_title() const {
    const Fonts& f = fonts();
    draw_text(130, 120, L"潮汐オセロ", color::text(), f.huge);
    draw_text(136, 214, L"TIDAL REVERSI", color::accent(), f.large);
    draw_text(140, 270, L"8 手ごとに、盤の外周が石ごと回る。", color::text_dim(), f.normal);
    draw_button(kTitleStart, mouse_x_, mouse_y_);
    draw_button(kTitleHelp, mouse_x_, mouse_y_);
    draw_button(kTitleQuit, mouse_x_, mouse_y_);

    const BoardView v{690.0f, 150.0f, 54.0f};
    const bool sliding = now_ms_ >= title_tween_.start && title_tween_.duration > 1;
    draw_board_base(v, now_ms_, sliding ? 1.0f : 0.2f);
    if (sliding) draw_tide_slide(v, title_board_, title_dir_, title_tween_.progress(now_ms_));
    else draw_discs(v, title_board_);
    draw_tide_arrows(v, title_dir_, now_ms_, sliding ? 1.0f : 0.3f);
}

void App::draw_mode_select() const {
    const Fonts& f = fonts();
    draw_text_center(kScreenW / 2, 80, L"対局モードを選んでください", color::text(), f.large);
    draw_text_center(kScreenW / 2, 190, L"対コンピューター（強さを選択）", color::text_dim(), f.normal);
    draw_button(kModeEasy, mouse_x_, mouse_y_);
    draw_button(kModeMedium, mouse_x_, mouse_y_);
    draw_button(kModeStrong, mouse_x_, mouse_y_);
    draw_text_center(kScreenW / 2, 380, L"2人で対局", color::text_dim(), f.normal);
    draw_button(kModeHotSeat, mouse_x_, mouse_y_);
    draw_button(kModeBack, mouse_x_, mouse_y_);
}

void App::draw_setup() const {
    const Fonts& f = fonts();
    draw_panel(240, 120, 800, 400);
    if (mode_ == Mode::VsAi) {
        draw_text_center(kScreenW / 2, 160, std::format(L"対コンピューター（{}）", level_name(level_)), color::text_dim(),
                         f.normal);
        const std::wstring you = human_color_ == Cell::Black ? L"あなたは 黒（先手）です"
                                                              : L"あなたは 白（後手）です";
        draw_text_center(kScreenW / 2, 230, you, color::text(), f.large);
        draw_disc(kScreenW / 2.0f, 330.0f, 36.0f, human_color_);
        draw_text_center(kScreenW / 2, 380, L"石の色はランダムに決まりました。先手は常に黒です。", color::text_dim(),
                         f.caption);
        draw_button(kSetupStart, mouse_x_, mouse_y_);
    } else {
        draw_text_center(kScreenW / 2, 170, L"2人で対局", color::text_dim(), f.normal);
        draw_text_center(kScreenW / 2, 240, L"黒（先手）を持つプレイヤーを選んでください", color::text(), f.large);
        draw_text_center(kScreenW / 2, 320, L"交互に盤面をクリックして打ちます。", color::text_dim(), f.caption);
        draw_button(kSetupP1Black, mouse_x_, mouse_y_);
        draw_button(kSetupP2Black, mouse_x_, mouse_y_);
    }
    draw_button(kSetupBack, mouse_x_, mouse_y_);
}

void App::draw_playing_board() const {
    const BoardView& v = kMainBoard;
    const bool tide_anim = (stage_ == Stage::Flip || stage_ == Stage::Tide) && event_.tide_fired;
    const int next_in = tide_anim ? 0 : match_.tide.next_in();
    const Dir dir = tide_anim ? event_.tide_dir : match_.tide.next_dir;
    float glow = 0.0f;
    if (stage_ == Stage::Tide) glow = 1.0f;
    else if (next_in == 1 && !match_.ended()) glow = 0.9f;
    else if (next_in == 2 && !match_.ended()) glow = 0.45f;

    draw_board_base(v, play_ms_, glow);
    if (!match_.ended() || stage_ == Stage::Flip || stage_ == Stage::Tide)
        draw_tide_arrows(v, dir, play_ms_, glow);

    if (stage_ == Stage::Flip) {
        draw_flip(v, event_.board_after_flip, event_.pos, event_.flips, event_.player, tween_.progress(play_ms_));
        return;
    }
    if (stage_ == Stage::Tide) {
        draw_tide_slide(v, event_.board_after_flip, event_.tide_dir, tween_.progress(play_ms_));
        return;
    }
    draw_discs(v, match_.board);
    if (has_last_move_) draw_last_move(v, last_move_);

    if (scene_ == Scene::Playing && stage_ == Stage::Idle && human_turn()) {
        Pos hover{-1, -1};
        const bool on_board = kMainBoard.hit(mouse_x_, mouse_y_, hover);
        for (const auto& mv : match_.legal()) {
            const bool hovered = on_board && mv.pos == hover;
            draw_legal_marker(v, mv.pos, play_ms_, hovered);
            if (hovered) {
                draw_ghost(v, mv.pos, match_.turn);
                draw_flip_preview(v, mv.flips);
            }
        }
    }
}

void App::draw_hud() const {
    const Fonts& f = fonts();
    draw_panel(kHudX, kHudY, kHudW, kHudH);
    const int x = kHudX + 24;
    int y = kHudY + 18;

    // モード
    const std::wstring mode = mode_ == Mode::VsAi ? std::format(L"対コンピューター　CPU: {}", level_name(level_))
                                                   : std::wstring(L"2人で対局");
    draw_text(x, y, mode, color::text_dim(), f.caption);
    y += 34;

    // 手番
    if (!match_.ended()) {
        draw_disc(static_cast<float>(x + 16), static_cast<float>(y + 17), 15.0f, match_.turn);
        std::wstring turn;
        if (mode_ == Mode::VsAi && !human_turn())
            turn = std::format(L"CPU（{}）が考えています…", color_name(match_.turn));
        else
            turn = std::format(L"{}（{}）の番です", player_label(match_.turn), color_name(match_.turn));
        draw_text(x + 44, y + 2, turn, color::text(), f.normal);
    }
    y += 56;

    // スコア
    const Count n = count(match_.board);
    for (Cell c : {Cell::Black, Cell::White}) {
        const bool active = !match_.ended() && match_.turn == c;
        if (active) DrawBoxAA(static_cast<float>(x - 8), static_cast<float>(y - 6), static_cast<float>(x + kHudW - 40),
                              static_cast<float>(y + 46), GetColor(36, 60, 88), TRUE);
        draw_disc(static_cast<float>(x + 18), static_cast<float>(y + 20), 18.0f, c);
        draw_text(x + 50, y + 6, std::format(L"{}　{}", color_name(c), player_label(c)), color::text(), f.normal);
        const std::wstring score = std::to_wstring(c == Cell::Black ? n.black : n.white);
        draw_text(kHudX + kHudW - 40 - text_width(score, f.large), y, score, color::text(), f.large);
        y += 58;
    }
    y += 10;

    // 潮汐カウント（FR-004 / SC-004: 表示は TideClock から直接導出）
    const bool tide_anim = (stage_ == Stage::Flip || stage_ == Stage::Tide) && event_.tide_fired;
    const int next_in = tide_anim ? 0 : match_.tide.next_in();
    const Dir dir = tide_anim ? event_.tide_dir : match_.tide.next_dir;
    DrawRoundRectAA(static_cast<float>(x - 8), static_cast<float>(y), static_cast<float>(kHudX + kHudW - 16),
                    static_cast<float>(y + 190), 10, 10, 16, GetColor(18, 52, 70), TRUE);
    draw_text(x + 8, y + 12, L"次の潮汐まで", color::accent(), f.normal);
    const std::wstring num = std::to_wstring(next_in);
    draw_text(x + 20, y + 48, num, next_in <= 2 ? color::warn() : color::text(), f.huge);
    draw_text(x + 30 + text_width(num, f.huge), y + 92, L"手", color::text(), f.normal);
    draw_dir_icon(static_cast<float>(kHudX + kHudW - 150), static_cast<float>(y + 84), 26.0f, dir, color::accent());
    draw_text(kHudX + kHudW - 110, y + 70, dir == Dir::CW ? L"時計回り" : L"反時計回り", color::text(), f.caption);
    // 8 手分の目盛り（潮汐から何手経ったか）
    const int done = TideClock::kInterval - next_in;
    for (int i = 0; i < TideClock::kInterval; ++i) {
        const float px = static_cast<float>(x + 16 + i * 30);
        const float py = static_cast<float>(y + 160);
        if (i < done) DrawCircleAA(px, py, 9.0f, 20, color::accent(), TRUE);
        else DrawCircleAA(px, py, 9.0f, 20, color::panel_edge(), FALSE, 2.0f);
    }
    draw_text(x + 270, y + 150, L"外周 28 マスが回転", color::text_dim(), f.caption);
    y += 206;

    // メッセージ
    if (stage_ == Stage::PassNotice) {
        draw_text(x, y, std::format(L"{} は置ける場所がありません — パス", color_name(pass_player_)), color::warn(),
                  f.normal);
    } else if (tide_anim) {
        draw_text(x, y,
                  std::format(L"潮汐！ 外周が{}に 1 マス動きます", event_.tide_dir == Dir::CW ? L"時計回り" : L"反時計回り"),
                  color::warn(), f.normal);
    } else if (match_.tide.next_in() == 1 && !match_.ended()) {
        draw_text(x, y, L"次の手のあと、潮汐が起きます", color::accent(), f.normal);
    }
    if (stage_ == Stage::Idle && human_turn() && !match_.ended())
        draw_text(x, y + 40, L"金色の枠のマスに置けます", color::text_dim(), f.caption);

    draw_button(kHudHelp, mouse_x_, mouse_y_, scene_ == Scene::Playing);
    draw_button(kHudQuit, mouse_x_, mouse_y_, scene_ == Scene::Playing);
}

void App::draw_result() const {
    const Fonts& f = fonts();
    draw_panel(kHudX, kHudY, kHudW, kHudH);
    const Result r = match_.result();
    const Count n = count(match_.board);
    std::wstring headline;
    if (r == Result::Draw) {
        headline = L"引き分け";
    } else {
        const Cell winner = r == Result::BlackWin ? Cell::Black : Cell::White;
        if (mode_ == Mode::VsAi) headline = winner == human_color_ ? L"あなたの勝ち！" : L"CPU の勝ち";
        else headline = std::format(L"{}の勝ち！", color_name(winner));
    }
    const int cx = kHudX + kHudW / 2;
    draw_text_center(cx, kHudY + 60, L"対局終了", color::text_dim(), f.normal);
    draw_text_center(cx, kHudY + 110, headline, color::warn(), f.huge);
    if (r != Result::Draw && mode_ == Mode::HotSeat) {
        const Cell winner = r == Result::BlackWin ? Cell::Black : Cell::White;
        draw_text_center(cx, kHudY + 200, std::format(L"（{}）", player_label(winner)), color::text(), f.normal);
    }
    draw_disc(static_cast<float>(cx - 150), static_cast<float>(kHudY + 300), 26.0f, Cell::Black);
    draw_text(cx - 112, kHudY + 280, std::to_wstring(n.black), color::text(), f.large);
    draw_text_center(cx, kHudY + 284, L"―", color::text_dim(), f.normal);
    draw_disc(static_cast<float>(cx + 60), static_cast<float>(kHudY + 300), 26.0f, Cell::White);
    draw_text(cx + 98, kHudY + 280, std::to_wstring(n.white), color::text(), f.large);
    draw_text_center(cx, kHudY + 370, std::format(L"潮汐は {} 回起きました", tides_fired_),
                     color::text_dim(), f.caption);
    draw_button(kResultAgain, mouse_x_, mouse_y_);
    draw_button(kResultTitle, mouse_x_, mouse_y_);
}

}  // namespace tidal::ui
