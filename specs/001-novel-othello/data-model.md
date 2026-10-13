# Data Model: 潮汐オセロ (Tidal Reversi)

すべて `src/core/` の純 C++20 型として定義（DxLib 依存ゼロ）。値意味・不変コピーで操作する。

## Entity: Board

8x8 の盤。

| Field | Type | Validation |
|-------|------|------------|
| cells[8][8] | `Cell` enum {EMPTY, BLACK, WHITE} | 初期化は標準初期配置（d4/e5=WHITE, d5/e4=BLACK）のみ |
| RING (静的) | `array<Pos,28>` | 時計回り列挙: 上辺 (0,0)→(0,7)、右辺 (1,7)→(7,7)、下辺 (7,6)→(7,0)、左辺 (6,0)→(1,0)。全 28 distinct |

Operations（純関数）:
- `legal_moves(board, player) -> vector<Move>` — 8 方向の挟み検査。盤外参照なし。
- `apply_flip(board, move) -> Board` — 指定手の反転を適用した新盤を返す（元盤は不変）。
- `rotate_ring(board, dir) -> Board` — dir ∈ {CW, CCW}。リング 28 マスの内容を 1 マス回転。内側 36 マスは逐字コピー。
- `count(board) -> {black, white}`

Validation rules:
- 合法手は EMPTY マスに置く手のみ。挟み列の終端は同一玩家の石で、間に相手の石が 1 つ以上。
- `rotate_ring` は盤の石総数を変えない（テストで不変式検証）。

## Entity: TideClock

| Field | Type | Validation |
|-------|------|------------|
| hands_played | int | 着手成功時のみ +1。パスでは不変（spec 計数規則） |
| next_in | int | `8 - (hands_played % 8)`。表示は常にこの値（SC-004 の自動検証対象） |
| next_dir | Dir ∈ {CW, CCW} | 初回 CW。発生時に反転 |

State transition: `on_move_applied()`: hands_played++ ; if hands_played % 8 == 0 → 発火（呼び出し側が終局判定後に適用）; next_dir.toggle()

## Entity: Match

| Field | Type | Validation |
|-------|------|------------|
| board | Board | — |
| turn | Cell (BLACK/WHITE) | 交互 |
| tide | TideClock | — |
| consecutive_passes | int | 2 で終局。着手成功で 0 リセット |
| state | enum {PLAYING, ENDED} | 終局条件: 連続パス or 盤満了 or 双方合法手なし |
| result | enum {BLACK_WIN, WHITE_WIN, DRAW} | ENDED のときのみ有効。スコア比較で決定 |

`Match.apply_move(pos)` の契約（順序は spec「法則の適用順」と一致）:
1. pos が非法手 → 変更なし（false を返す）
2. 反転適用 → turn 交換 → tide.on_move_applied()
3. 終局判定（盤満了/双方合法手なし/連続パス）→ ENDED ならここで停止（**潮汐は行わない**）
4. hands_played % 8 == 0 なら `board = rotate_ring(board, 発生方向)`
5. 次の玩家の合法手集合を計算（無ければ次呼びでパス処理）

`Match.pass()` の契約: consecutive_passes++、turn 交換、tide は不変。2 で ENDED。

## Entity: Move

| Field | Type |
|-------|------|
| pos | Pos {row, col} |
| flips | vector<Pos>（反転される石、適用順に列挙） |

## Entity: AI Engine

| Field | Type | Validation |
|-------|------|------------|
| level | enum {EASY, MEDIUM, STRONG} | 深さ 1(+ランダム)/4/7（research R4） |
| rng seed | uint64_t | テストでは固定シードで決定論的 |

`choose_move(match, level) -> Move` — 返す手は必ず `legal_moves` の成员。合法手なしは呼ばれない（呼び出し側が保証）。
評価関数: 位置重み + 可搬性差 + 確定石 + 潮汐近接ボーナス（次回潮汐まで R<=4 のとき、R 手後の仮想盤で再評価）。

## Entity: GameFlow (UI 層の状態機械, src/game/)

Title → ModeSelect(対AI[易/普/強] | 同卓) → [vs AI: ColorRandomize 通知] → Playing ⇄ HelpOverlay → Result → ModeSelect
- HelpOverlay は対局状態を保存して復帰（FR-011）。
- アニメ再生中は入力ロックフラグ（Edge Case 対応）。

## Relationships

- Match 1—1 TideClock, Match 1—1 Board（値コピーで不変性維持）
- AI Engine は Match の read-only 参照から手を選ぶ（状態変更は match.apply_move 経由のみ）
- GameFlow は Match を 1 つ保持し、UI イベントを変換して apply_move/pass を呼ぶ
