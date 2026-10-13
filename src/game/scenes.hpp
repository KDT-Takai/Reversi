#pragma once
// 画面フロー状態機械（data-model.md: GameFlow / ui-contract.md: Screen flow）

#include <atomic>
#include <cstdint>
#include <random>
#include <string>
#include <thread>

#include "core/ai.hpp"
#include "core/match.hpp"
#include "game/anim.hpp"
#include "game/fx.hpp"
#include "game/render.hpp"

namespace tidal::ui {

enum class Scene { Title, ModeSelect, MatchSetup, Playing, Help, Result };
enum class Mode { VsAi, HotSeat, AiVsAi };

struct Input {
    int mouse_x = 0;
    int mouse_y = 0;
    bool clicked = false;  // 左ボタンが押された瞬間
    bool key_help = false;  // H キーが押された瞬間
    bool key_back = false;  // Esc キーが押された瞬間
};

// AI 思考用ワーカースレッド（research R6）。UI スレッドは poll するだけでブロックしない。
class AiWorker {
public:
    ~AiWorker() { cancel(); }
    void start(const Match& m, Level level, std::uint64_t seed);
    bool poll(Pos& out);
    void cancel();
    bool running() const { return running_; }

private:
    std::jthread thread_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> ready_{false};
    Pos result_{};
    bool running_ = false;
};

class App {
public:
    App();
    void update(const Input& in, int now_ms);
    void draw() const;
    bool quit() const { return quit_; }

private:
    // 対局中の演出段階
    enum class Stage { Idle, Flip, Tide, PassNotice, EndDelay };

    void update_title(const Input& in);
    void update_mode_select(const Input& in);
    void update_setup(const Input& in);
    void update_playing(const Input& in, float dt_sec);
    void update_result(const Input& in);

    void draw_title() const;
    void draw_mode_select() const;
    void draw_setup() const;
    void draw_playing_board() const;
    void draw_hud() const;
    void draw_result() const;

    void go(Scene to);  // フェードイン付き画面遷移（FR-015）
    void open_help();
    void start_match();
    void begin_turn();
    void play_move(Pos pos);
    void finish_event();
    void leave_match(Scene to);
    void enter_result();

    bool human_turn() const;
    Level level_for(Cell c) const;
    std::wstring player_label(Cell c) const;
    BoardView shaken_board() const;

    Scene scene_ = Scene::Title;
    Scene help_return_ = Scene::Title;
    bool quit_ = false;

    int now_ms_ = 0;
    int last_now_ = -1;
    int play_ms_ = 0;  // 対局時計（ヘルプ表示中は止まる）
    int fade_start_ms_ = -10000;
    int mouse_x_ = 0;
    int mouse_y_ = 0;

    // 対局設定
    Mode mode_ = Mode::VsAi;
    Level level_ = Level::Medium;          // vs AI の CPU の強さ
    Level black_level_ = Level::Medium;    // AI 対 AI
    Level white_level_ = Level::Medium;    // AI 対 AI
    Cell human_color_ = Cell::Black;  // vs AI: 人間の石色 / 同卓: プレイヤー1 の石色
    std::mt19937_64 rng_;

    // 対局状態
    Match match_;
    Stage stage_ = Stage::Idle;
    Tween tween_;
    MoveEvent event_;
    bool has_last_move_ = false;
    Pos last_move_{};
    Cell pass_player_ = Cell::Empty;
    int tides_fired_ = 0;
    int turn_start_ms_ = 0;
    bool ai_has_result_ = false;
    Pos ai_result_{};
    AiWorker ai_;

    // 演出
    Fx fx_;
    int result_start_ms_ = 0;
    bool celebrate_ = false;

    // タイトル画面の装飾盤
    Board title_board_{};
    Dir title_dir_ = Dir::CW;
    Tween title_tween_;
};

}  // namespace tidal::ui
