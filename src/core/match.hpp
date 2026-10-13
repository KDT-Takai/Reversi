#pragma once
// 対局状態機械と潮汐時計（data-model.md: TideClock / Match）

#include <optional>
#include <vector>

#include "core/board.hpp"
#include "core/rules.hpp"

namespace tidal {

struct TideClock {
    static constexpr int kInterval = 8;  // 8 実手ごと（双方 4 手ごと）

    int hands_played = 0;  // 石を置いた実手のみ計数（パスは計数しない）
    Dir next_dir = Dir::CW;  // 初回は時計回り、発生ごとに反転

    int next_in() const { return kInterval - hands_played % kInterval; }
};

enum class MatchState { Playing, Ended };
enum class Result { BlackWin, WhiteWin, Draw };

// 1 手の適用結果。UI はこれを使って反転・潮汐のアニメーションを組み立てる。
struct MoveEvent {
    Cell player = Cell::Empty;
    Pos pos;
    std::vector<Pos> flips;
    Board board_after_flip{};  // 反転解決直後（潮汐前）の盤
    bool tide_fired = false;
    Dir tide_dir = Dir::CW;  // tide_fired のときの回転方向
    bool ended = false;
};

class Match {
public:
    Board board = initial_board();
    Cell turn = Cell::Black;  // 先手は常に黒
    TideClock tide;
    int consecutive_passes = 0;
    MatchState state = MatchState::Playing;

    bool ended() const { return state == MatchState::Ended; }

    std::vector<Move> legal() const { return legal_moves(board, turn); }

    bool must_pass() const { return !ended() && !has_legal_move(board, turn); }

    // 規則の適用順（spec「法則の適用順」）:
    //   非法手→変更なし / 反転解決→手番交代→実手計数 / 終局判定（成立なら潮汐なし）/
    //   8 実手到達なら潮汐 / 潮汐後に双方合法手なしなら終局
    std::optional<MoveEvent> apply_move(Pos pos) {
        if (ended()) return std::nullopt;
        auto flips = flips_for(board, pos, turn);
        if (flips.empty()) return std::nullopt;

        MoveEvent ev;
        ev.player = turn;
        ev.pos = pos;
        ev.flips = flips;
        board = apply_flip(board, Move{pos, std::move(flips)}, turn);
        ev.board_after_flip = board;
        turn = opponent(turn);
        consecutive_passes = 0;
        ++tide.hands_played;

        if (is_game_over(board)) {
            state = MatchState::Ended;
            ev.ended = true;
            return ev;
        }

        if (tide.hands_played % TideClock::kInterval == 0) {
            ev.tide_fired = true;
            ev.tide_dir = tide.next_dir;
            board = rotate_ring(board, tide.next_dir);
            tide.next_dir = toggled(tide.next_dir);
            if (is_game_over(board)) {
                state = MatchState::Ended;
                ev.ended = true;
            }
        }
        return ev;
    }

    // 合法手がないときのみ有効。実手には数えず潮汐時計は不変。
    bool pass() {
        if (!must_pass()) return false;
        ++consecutive_passes;
        turn = opponent(turn);
        if (consecutive_passes >= 2) state = MatchState::Ended;
        return true;
    }

    Result result() const {
        const Count n = count(board);
        if (n.black > n.white) return Result::BlackWin;
        if (n.white > n.black) return Result::WhiteWin;
        return Result::Draw;
    }

    static bool is_game_over(const Board& b) {
        return is_full(b) || (!has_legal_move(b, Cell::Black) && !has_legal_move(b, Cell::White));
    }
};

}  // namespace tidal
