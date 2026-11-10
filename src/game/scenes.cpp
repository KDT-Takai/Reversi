#include "game/scenes.hpp"

#include "game/dx.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include "game/help.hpp"
#include "game/sound.hpp"

namespace tidal::ui {

namespace {

constexpr int kAiMinThinkMs = 450;      // AI が即答しても一瞬で打たれないための最低待ち
constexpr int kAiVsAiMinThinkMs = 700;  // 観戦用の 1 手ごとの間（FR-016）
constexpr int kPassNoticeMs = 1500;
constexpr int kEndDelayMs = 900;
constexpr int kTitleTideEveryMs = 1600;
constexpr int kFadeMs = 300;
constexpr int kResultSlideMs = 450;
constexpr int kConfettiMs = 2500;

// ---- ボタン配置
const Button kTitleStart{140, 330, 320, 60, L"対局を始める"};
const Button kTitleHelp{140, 410, 320, 60, L"遊び方（H）"};
const Button kTitleQuit{140, 490, 320, 60, L"終了"};

const Button kModeEasy{240, 250, 240, 64, L"易"};
const Button kModeMedium{520, 250, 240, 64, L"普"};
const Button kModeStrong{800, 250, 240, 64, L"強"};
const Button kModeHotSeat{240, 440, 380, 64, L"2人で対局（同じ画面）"};
const Button kModeAiVsAi{660, 440, 380, 64, L"AI 対 AI（観戦）"};
const Button kModeBack{540, 590, 200, 52, L"戻る（Esc）"};

const Button kSetupStart{490, 420, 300, 64, L"対局開始"};
const Button kSetupP1Black{300, 400, 320, 64, L"プレイヤー1 が黒"};
const Button kSetupP2Black{660, 400, 320, 64, L"プレイヤー2 が黒"};
const Button kSetupBack{540, 560, 200, 52, L"戻る（Esc）"};

constexpr Level kLevels[3] = {Level::Easy, Level::Medium, Level::Strong};
const Button kSetupBlackLv[3] = {{560, 214, 140, 56, L"易"}, {720, 214, 140, 56, L"普"}, {880, 214, 140, 56, L"強"}};
const Button kSetupWhiteLv[3] = {{560, 300, 140, 56, L"易"}, {720, 300, 140, 56, L"普"}, {880, 300, 140, 56, L"強"}};

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

const wchar_t* color_name(Cell c) { return c == Cell::Black ? L"黒" : L"白"; }

Board make_title_board() {
    Board b = initial_board();
    const Cell pattern[] = {Cell::Black, Cell::Empty, Cell::White, Cell::Black, Cell::Empty, Cell::White, Cell::White};
    for (int i = 0; i < kRingSize; ++i) {
        const Pos p = kRing[i];
        b[p.row][p.col] = pattern[i % 7];
    }
    return b;
}

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

// 盤の中心からマスへ向かう単位ベクトル（飛沫を外へ散らす向き）
void outward(const BoardView& v, float x, float y, float& dx, float& dy) {
    const float cx = v.x + v.size() / 2;
    const float cy = v.y + v.size() / 2;
    const float len = std::max(1.0f, std::hypot(x - cx, y - cy));
    dx = (x - cx) / len;
    dy = (y - cy) / len;
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

void App::go(Scene to) {
    scene_ = to;
    fade_start_ms_ = now_ms_;
}

void App::update(const Input& in, int now_ms) {
    const int dt = last_now_ < 0 ? 0 : std::clamp(now_ms - last_now_, 0, 100);
    last_now_ = now_ms;
    now_ms_ += dt;
    mouse_x_ = in.mouse_x;
    mouse_y_ = in.mouse_y;
    const float dt_sec = static_cast<float>(dt) / 1000.0f;

    switch (scene_) {
        case Scene::Title: update_title(in); break;
        case Scene::ModeSelect: update_mode_select(in); break;
        case Scene::MatchSetup: update_setup(in); break;
        case Scene::Playing:
            play_ms_ += dt;
            update_playing(in, dt_sec);
            break;
        case Scene::Help:
            if (in.key_back || in.key_help || (in.clicked && help_close_button().contains(in.mouse_x, in.mouse_y)))
                scene_ = help_return_;
            break;
        case Scene::Result: update_result(in); break;
    }
    fx_.update(dt_sec);
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
    if (kTitleStart.contains(in.mouse_x, in.mouse_y)) go(Scene::ModeSelect);
    else if (kTitleHelp.contains(in.mouse_x, in.mouse_y)) open_help();
    else if (kTitleQuit.contains(in.mouse_x, in.mouse_y)) quit_ = true;
}

void App::update_mode_select(const Input& in) {
    if (in.key_back) return go(Scene::Title);
    if (!in.clicked) return;
    auto choose_ai = [&](Level lv) {
        mode_ = Mode::VsAi;
        level_ = lv;
        // 対 AI は石の色をランダム決定（FR-006）
        human_color_ = std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? Cell::Black : Cell::White;
        go(Scene::MatchSetup);
    };
    if (kModeEasy.contains(in.mouse_x, in.mouse_y)) choose_ai(Level::Easy);
    else if (kModeMedium.contains(in.mouse_x, in.mouse_y)) choose_ai(Level::Medium);
    else if (kModeStrong.contains(in.mouse_x, in.mouse_y)) choose_ai(Level::Strong);
    else if (kModeHotSeat.contains(in.mouse_x, in.mouse_y)) {
        mode_ = Mode::HotSeat;
        go(Scene::MatchSetup);
    } else if (kModeAiVsAi.contains(in.mouse_x, in.mouse_y)) {
        mode_ = Mode::AiVsAi;
        go(Scene::MatchSetup);
    } else if (kModeBack.contains(in.mouse_x, in.mouse_y)) {
        go(Scene::Title);
    }
}

void App::update_setup(const Input& in) {
    if (in.key_back) return go(Scene::ModeSelect);
    if (!in.clicked) return;
    if (kSetupBack.contains(in.mouse_x, in.mouse_y)) return go(Scene::ModeSelect);
    switch (mode_) {
        case Mode::VsAi:
            if (kSetupStart.contains(in.mouse_x, in.mouse_y)) start_match();
            break;
        case Mode::HotSeat:
            if (kSetupP1Black.contains(in.mouse_x, in.mouse_y)) {
                human_color_ = Cell::Black;
                start_match();
            } else if (kSetupP2Black.contains(in.mouse_x, in.mouse_y)) {
                human_color_ = Cell::White;
                start_match();
            }
            break;
        case Mode::AiVsAi:
            for (int i = 0; i < 3; ++i) {
                if (kSetupBlackLv[i].contains(in.mouse_x, in.mouse_y)) black_level_ = kLevels[i];
                if (kSetupWhiteLv[i].contains(in.mouse_x, in.mouse_y)) white_level_ = kLevels[i];
            }
            if (kSetupStart.contains(in.mouse_x, in.mouse_y)) start_match();
            break;
    }
}

// ------------------------------------------------------------ Playing

void App::start_match() {
    ai_.cancel();
    fx_.clear();
    match_ = Match{};
    has_last_move_ = false;
    tides_fired_ = 0;
    play_ms_ = 0;
    go(Scene::Playing);
    begin_turn();
}

void App::leave_match(Scene to) {
    ai_.cancel();
    ai_has_result_ = false;
    fx_.clear();
    go(to);
}

bool App::human_turn() const {
    switch (mode_) {
        case Mode::HotSeat: return true;
        case Mode::VsAi: return match_.turn == human_color_;
        default: return false;
    }
}

Level App::level_for(Cell c) const {
    if (mode_ == Mode::AiVsAi) return c == Cell::Black ? black_level_ : white_level_;
    return level_;
}

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
    if (!human_turn()) ai_.start(match_, level_for(match_.turn), rng_());
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

void App::enter_result() {
    go(Scene::Result);
    result_start_ms_ = now_ms_;
    const Result r = match_.result();
    const bool human_lost = mode_ == Mode::VsAi && r != Result::Draw &&
                            (r == Result::BlackWin) != (human_color_ == Cell::Black);
    celebrate_ = r != Result::Draw && !human_lost;
    if (celebrate_) fx_.confetti(140);
    if (r == Result::Draw) sound::play(sound::Se::Draw);
    else if (human_lost) sound::play(sound::Se::Lose);
    else sound::play(sound::Se::Win);
}

void App::update_playing(const Input& in, float dt_sec) {
    (void)dt_sec;
    if (in.key_help || (in.clicked && kHudHelp.contains(in.mouse_x, in.mouse_y))) return open_help();
    if (in.key_back || (in.clicked && kHudQuit.contains(in.mouse_x, in.mouse_y)))
        return leave_match(Scene::ModeSelect);

    switch (stage_) {
        case Stage::Flip:
            if (tween_.done(play_ms_)) {
                if (event_.tide_fired) {
                    stage_ = Stage::Tide;
                    tween_ = {play_ms_, kTideStageMs};
                    sound::play(sound::Se::Tide);
                } else {
                    finish_event();
                }
            }
            break;
        case Stage::Tide: {
            // 飛沫: スライド開始の瞬間に外周全体から一斉に、スライド中は動く石から散らす（FR-014）
            const int elapsed = play_ms_ - tween_.start;
            const float slide_t = clamp01(static_cast<float>(elapsed - kTideSlideStartMs) / kTideMs);
            const bool sliding = elapsed >= kTideSlideStartMs && slide_t < 1.0f;
            const BoardView& v = kMainBoard;
            for (int i = 0; i < kRingSize; ++i) {
                const Pos p = kRing[i];
                const bool stone = event_.board_after_flip[p.row][p.col] != Cell::Empty;
                float x = v.cx(p.col);
                float y = v.cy(p.row);
                if (stone) ring_disc_position(v, i, event_.tide_dir, slide_t, x, y);
                float dx = 0.0f;
                float dy = 0.0f;
                outward(v, x, y, dx, dy);
                if (sliding && slide_t < 0.08f) fx_.foam(x, y, dx, dy, stone ? 6 : 3);
                else if (sliding && stone) fx_.foam(x, y, dx, dy, 1);
            }
            if (tween_.done(play_ms_)) finish_event();
            break;
        }
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
            if (tween_.done(play_ms_)) enter_result();
            break;
        case Stage::Idle:
            if (human_turn()) {
                Pos p;
                if (in.clicked && kMainBoard.hit(in.mouse_x, in.mouse_y, p)) play_move(p);
            } else {
                const int min_think = mode_ == Mode::AiVsAi ? kAiVsAiMinThinkMs : kAiMinThinkMs;
                if (!ai_has_result_ && ai_.poll(ai_result_)) ai_has_result_ = true;
                if (ai_has_result_ && play_ms_ - turn_start_ms_ >= min_think) {
                    ai_has_result_ = false;
                    play_move(ai_result_);
                }
            }
            break;
    }
}

void App::update_result(const Input& in) {
    if (celebrate_ && now_ms_ - result_start_ms_ < kConfettiMs) fx_.confetti(2);
    if (in.key_back) return leave_match(Scene::Title);
    if (!in.clicked) return;
    if (kResultAgain.contains(in.mouse_x, in.mouse_y)) leave_match(Scene::ModeSelect);
    else if (kResultTitle.contains(in.mouse_x, in.mouse_y)) leave_match(Scene::Title);
}

// ------------------------------------------------------------ 表示用ラベル

std::wstring App::player_label(Cell c) const {
    switch (mode_) {
        case Mode::VsAi: return c == human_color_ ? L"あなた" : L"CPU";
        case Mode::HotSeat: return c == human_color_ ? L"プレイヤー1" : L"プレイヤー2";
        default: return std::format(L"CPU・{}", level_name(level_for(c)));
    }
}

BoardView App::shaken_board() const {
    BoardView v = kMainBoard;
    if (scene_ != Scene::Playing && scene_ != Scene::Help) return v;
    if (stage_ != Stage::Tide) return v;
    // 盤の揺れ: 最大 6px、潮汐演出の進行とともに減衰（FR-014）
    const float elapsed = static_cast<float>(play_ms_ - tween_.start);
    const float amp = 6.0f * (1.0f - clamp01(elapsed / kTideStageMs));
    v.x += amp * std::sin(elapsed * 0.09f);
    v.y += amp * 0.6f * std::cos(elapsed * 0.13f);
    return v;
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
            if (stage_ == Stage::Tide) draw_tide_cutin(event_.tide_dir, tween_.progress(play_ms_));
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
    fx_.draw();

    // 画面遷移のフェードイン（FR-015）
    const float fade = 1.0f - clamp01(static_cast<float>(now_ms_ - fade_start_ms_) / kFadeMs);
    if (fade > 0.0f) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255 * fade));
        DrawBox(0, 0, kScreenW, kScreenH, GetColor(4, 10, 18), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

void App::draw_title() const {
    const Fonts& f = fonts();
    draw_text(130, 120, L"潮汐オセロ", color::text(), f.huge);
    draw_text(138, 216, L"T I D A L  R E V E R S I", color::text_dim(), f.large);
    draw_text(140, 270, L"8 手ごとに、盤の外縁が石ごと流れる。", color::text_dim(), f.normal);
    draw_button(kTitleStart, mouse_x_, mouse_y_);
    draw_button(kTitleHelp, mouse_x_, mouse_y_);
    draw_button(kTitleQuit, mouse_x_, mouse_y_);

    const BoardView v{690.0f, 150.0f, 54.0f};
    const bool sliding = now_ms_ >= title_tween_.start && title_tween_.duration > 1;
    draw_board_base(v, now_ms_, sliding ? 1.0f : 0.2f);
    if (sliding) {
        draw_ring_sweep(v, title_dir_, title_tween_.progress(now_ms_));
        draw_tide_slide(v, title_board_, title_dir_, title_tween_.progress(now_ms_));
    } else {
        draw_discs(v, title_board_);
    }
    draw_tide_arrows(v, title_dir_, now_ms_, sliding ? 1.0f : 0.3f);
}

void App::draw_mode_select() const {
    const Fonts& f = fonts();
    draw_text_center(kScreenW / 2, 80, L"対局モード", color::text(), f.large);
    draw_text_center(kScreenW / 2, 190, L"対コンピューター", color::text_dim(), f.normal);
    draw_button(kModeEasy, mouse_x_, mouse_y_);
    draw_button(kModeMedium, mouse_x_, mouse_y_);
    draw_button(kModeStrong, mouse_x_, mouse_y_);
    draw_text_center(kScreenW / 2, 380, L"その他のモード", color::text_dim(), f.normal);
    draw_button(kModeHotSeat, mouse_x_, mouse_y_);
    draw_button(kModeAiVsAi, mouse_x_, mouse_y_);
    draw_button(kModeBack, mouse_x_, mouse_y_);
}

void App::draw_setup() const {
    const Fonts& f = fonts();
    draw_panel(240, 120, 800, 400);
    switch (mode_) {
        case Mode::VsAi: {
            draw_text_center(kScreenW / 2, 160, std::format(L"対コンピューター（{}）", level_name(level_)),
                             color::text_dim(), f.normal);
            const std::wstring you = human_color_ == Cell::Black ? L"あなたは 黒（先手）です"
                                                                  : L"あなたは 白（後手）です";
            draw_text_center(kScreenW / 2, 230, you, color::text(), f.large);
            draw_disc(kScreenW / 2.0f, 330.0f, 36.0f, human_color_);
            draw_text_center(kScreenW / 2, 380, L"石の色はランダムに決まりました。先手は常に黒です。",
                             color::text_dim(), f.caption);
            draw_button(kSetupStart, mouse_x_, mouse_y_);
            break;
        }
        case Mode::HotSeat:
            draw_text_center(kScreenW / 2, 170, L"2人で対局", color::text_dim(), f.normal);
            draw_text_center(kScreenW / 2, 240, L"黒（先手）を持つプレイヤー", color::text(), f.large);
            draw_text_center(kScreenW / 2, 320, L"交互に盤面をクリックして打ちます。", color::text_dim(), f.caption);
            draw_button(kSetupP1Black, mouse_x_, mouse_y_);
            draw_button(kSetupP2Black, mouse_x_, mouse_y_);
            break;
        case Mode::AiVsAi:
            draw_text_center(kScreenW / 2, 146, L"AI 対 AI（観戦）", color::text(), f.large);
            draw_disc(330.0f, 242.0f, 22.0f, Cell::Black);
            draw_text(366, 226, L"黒（先手）", color::text(), f.normal);
            draw_disc(330.0f, 328.0f, 22.0f, Cell::White);
            draw_text(366, 312, L"白（後手）", color::text(), f.normal);
            for (int i = 0; i < 3; ++i) {
                draw_button(kSetupBlackLv[i], mouse_x_, mouse_y_, true, black_level_ == kLevels[i]);
                draw_button(kSetupWhiteLv[i], mouse_x_, mouse_y_, true, white_level_ == kLevels[i]);
            }
            draw_button(kSetupStart, mouse_x_, mouse_y_);
            break;
    }
    draw_button(kSetupBack, mouse_x_, mouse_y_);
}

void App::draw_playing_board() const {
    const BoardView v = shaken_board();
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
        const int elapsed = play_ms_ - tween_.start;
        draw_ring_sweep(v, event_.tide_dir, tween_.progress(play_ms_));
        draw_tide_slide(v, event_.board_after_flip, event_.tide_dir,
                        clamp01(static_cast<float>(elapsed - kTideSlideStartMs) / kTideMs));
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
    std::wstring mode;
    switch (mode_) {
        case Mode::VsAi: mode = std::format(L"対コンピューター　CPU: {}", level_name(level_)); break;
        case Mode::HotSeat: mode = L"2人で対局"; break;
        case Mode::AiVsAi:
            mode = std::format(L"AI 対 AI（観戦）　黒: {} / 白: {}", level_name(black_level_), level_name(white_level_));
            break;
    }
    draw_text(x, y, mode, color::text_dim(), f.caption);
    y += 34;

    // 手番
    if (!match_.ended()) {
        draw_disc(static_cast<float>(x + 16), static_cast<float>(y + 17), 15.0f, match_.turn);
        std::wstring turn;
        if (!human_turn())
            turn = std::format(L"{}（{}）が考えています…", player_label(match_.turn), color_name(match_.turn));
        else
            turn = std::format(L"{}（{}）の番です", player_label(match_.turn), color_name(match_.turn));
        draw_text(x + 44, y + 2, turn, color::text(), f.normal);
    }
    y += 56;

    // スコア
    const Count n = count(match_.board);
    for (Cell c : {Cell::Black, Cell::White}) {
        const bool active = !match_.ended() && match_.turn == c;
        if (active)
            DrawBoxAA(static_cast<float>(x - 8), static_cast<float>(y - 6), static_cast<float>(x + kHudW - 40),
                      static_cast<float>(y + 46), GetColor(32, 44, 64), TRUE);
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
                    static_cast<float>(y + 190), 5, 5, 16, GetColor(16, 34, 52), TRUE);
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
                  std::format(L"潮汐！ 外周が{}に 1 マス動きます",
                              event_.tide_dir == Dir::CW ? L"時計回り" : L"反時計回り"),
                  color::warn(), f.normal);
    } else if (match_.tide.next_in() == 1 && !match_.ended()) {
        draw_text(x, y, L"次の手のあと、潮汐が起きます", color::accent(), f.normal);
    }
    if (stage_ == Stage::Idle && human_turn() && !match_.ended())
        draw_text(x, y + 40, L"金色の枠のマスへ置く", color::text_dim(), f.caption);

    draw_button(kHudHelp, mouse_x_, mouse_y_, scene_ == Scene::Playing);
    draw_button(kHudQuit, mouse_x_, mouse_y_, scene_ == Scene::Playing);
}

void App::draw_result() const {
    const Fonts& f = fonts();
    const float elapsed = static_cast<float>(now_ms_ - result_start_ms_);
    // パネルは右からスライドイン（FR-015）
    const int ox = static_cast<int>((1.0f - ease_in_out(clamp01(elapsed / kResultSlideMs))) * 560.0f);
    draw_panel(kHudX + ox, kHudY, kHudW, kHudH);

    const Result r = match_.result();
    const Count n = count(match_.board);
    std::wstring headline;
    std::wstring sub;
    if (r == Result::Draw) {
        headline = L"引き分け";
    } else {
        const Cell winner = r == Result::BlackWin ? Cell::Black : Cell::White;
        if (mode_ == Mode::VsAi) {
            headline = winner == human_color_ ? L"あなたの勝ち" : L"CPU の勝ち";
        } else {
            headline = std::format(L"{}の勝ち", color_name(winner));
            sub = std::format(L"（{}）", player_label(winner));
        }
    }
    const int cx = kHudX + ox + kHudW / 2;
    draw_text_center(cx, kHudY + 24, L"対局終了", color::text_dim(), f.normal);

    // 見出しは少し遅れて上から落ちてくる
    const float h = clamp01((elapsed - 350.0f) / 400.0f);
    if (h > 0.0f) {
        const int drop = static_cast<int>((1.0f - ease_in_out(h)) * -40.0f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(255 * h));
        draw_text_center(cx, kHudY + 110 + drop, headline, color::warn(), f.huge);
        if (!sub.empty()) draw_text_center(cx, kHudY + 200 + drop, sub, color::text(), f.normal);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // 最終スコアのカウントアップ
    const float u = ease_in_out(clamp01((elapsed - 350.0f) / 1000.0f));
    const int shown_black = static_cast<int>(std::lround(n.black * u));
    const int shown_white = static_cast<int>(std::lround(n.white * u));
    draw_disc(static_cast<float>(cx - 150), static_cast<float>(kHudY + 300), 26.0f, Cell::Black);
    draw_text(cx - 112, kHudY + 280, std::to_wstring(shown_black), color::text(), f.large);
    draw_text_center(cx, kHudY + 284, L"―", color::text_dim(), f.normal);
    draw_disc(static_cast<float>(cx + 60), static_cast<float>(kHudY + 300), 26.0f, Cell::White);
    draw_text(cx + 98, kHudY + 280, std::to_wstring(shown_white), color::text(), f.large);
    draw_text_center(cx, kHudY + 370, std::format(L"潮汐は {} 回起きました", tides_fired_), color::text_dim(),
                     f.caption);

    Button again = kResultAgain;
    Button title = kResultTitle;
    again.x += ox;
    title.x += ox;
    draw_button(again, mouse_x_, mouse_y_);
    draw_button(title, mouse_x_, mouse_y_);
}

}  // namespace tidal::ui
