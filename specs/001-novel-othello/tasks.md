# Tasks: 潮汐オセロ (Tidal Reversi)

**Input**: Design documents from `/specs/001-novel-othello/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ui-contract.md, quickstart.md

**Tests**: 含む。根拠: 憲法 I（ルールコアはグラフィクスなしで単体検証可能であること）と quickstart の CoreTests ゲート。テストは対応実装より前に書く。

**Organization**: ユーザーストーリー単位でグループ化し、各ストーリー独立に実装・検証可能にする。

## Format: `[ID] [P?] [Story] Description`

- **[P]**: 並列実行可（別ファイル、未完了タスクへの依存なし）
- **[Story]**: US1..US4（spec.md のユーザーストーリー）

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: ソリューション骨格とビルド経路の確立（憲法 V）

- [X] T001 Create source tree per plan.md: src/core/, src/game/, src/tests/ with empty placeholder files
- [X] T002 Create Reversi.sln + TidalReversi.vcxproj (x64 Debug/Release, C++20, /W4 /utf-8, UNICODE + _CRT_SECURE_NO_WARNINGS defines, IncludePath=vendor/DxLib/include, AdditionalLibraryDirectories=vendor/DxLib/lib, SubSystem Windows) in repo root
- [X] T003 [P] Create CoreTests.vcxproj (console app; compiles src/core/*.hpp users + src/tests/main.cpp only; no DxLib) in repo root
- [X] T004 Verify empty build: msbuild Reversi.sln /p:Configuration=Release /p:Platform=x64 succeeds for BOTH projects (WinMain stub, test main stub)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: 純 C++20 ルールコア + 自動テスト（全ストーリーの前提）

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T005 Implement Board entity in src/core/board.hpp: `Cell{EMPTY,BLACK,WHITE}`, `Pos{row,col}`, `Board = array<array<Cell,8>,8>`, standard initial setup ((3,3)/(4,4)=WHITE, (3,4)/(4,3)=BLACK per data-model), `count()`. NO DxLib includes
- [X] T006 Implement ring + tide rotation in src/core/board.hpp: static `RING` = 28 Pos clockwise exactly as data-model.md ("上辺 (0,0)→(0,7)、右辺 (1,7)→(7,7)、下辺 (7,6)→(7,0)、左辺 (6,0)→(1,0)"); `rotate_ring(board, Dir::CW|CCW)` via temp copy; invariants: stone count unchanged, inner 6x6 unchanged
- [X] T007 Implement legal-move generation + flip application in src/core/rules.hpp: `legal_moves(board, player) -> vector<Move{pos,flips}>` (8-direction bracket scan, terminal same-player, ≥1 opponent stone between), `apply_flip` returning new Board (source immutable)
- [X] T008 Implement TideClock + Match state machine in src/core/match.hpp: fields per data-model.md (`hands_played` incremented ONLY on successful move; `next_in = 8 - hands_played % 8`; `next_dir` CW first, toggles each occurrence); `apply_move` contract order: illegal→no-op false → flip+turn swap+tide.on_move_applied → END CHECK (board full / no legal moves both sides / consecutive passes) ENDED stops here **no tide** → if hands%8==0 rotate_ring(next_dir) ; `pass()` (consecutive_passes++, tide unchanged, 2→ENDED); result enum BLACK_WIN/WHITE_WIN/DRAW by count
- [X] T009 Implement assert-based test runner skeleton in src/tests/main.cpp (test registry, per-test output, nonzero exit on any failure, fixed seeds for determinism)
- [X] T010 Write core rules tests in src/tests/main.cpp: legal moves (corner/edge/interior cases), flip results exact stone lists, pass handling, both end conditions, draw (32-32 → DRAW)
- [X] T011 Write tide tests in src/tests/main.cpp: CW/CCW rotation correctness incl. corner→edge and edge→corner movement; exactly one rotation per 8 real moves across a simulated 40-move game; passes do NOT advance hands_played; end-priority (move that ends game at hands%8==0 does NOT rotate); next_in display value always equals internal state
- [X] T012 Gate: build CoreTests and run → ALL PASS, exit code 0

**Checkpoint**: Rule core verified without graphics (Constitution I satisfied)

---

## Phase 3: User Story 1 - 潮汐付きの一試合を完走 (Priority: P1) 🎯 MVP

**Goal**: 同卓2人操作で完全な対局（盤/HUD/潮汐演出/アニメ/音/勝敗表示）が遊べる

**Independent Test**: quickstart.md 手動検証 #1(同卓経路)・#2・#3・#4・#7 を実行できる

- [X] T013 [US1] Implement app entry + main loop in src/game/main.cpp: DxLib init, fixed 1280x720 window titled 「潮汐オセロ」, 60FPS loop, ESC handling, scene dispatch hook, clean exit
- [X] T014 [US1] Implement GameFlow state machine in src/game/scenes.hpp/.cpp: TITLE → MODE_SELECT(同卓 entry) → PLAYING → RESULT (勝敗/引き分け + 最終スコア + もう一度/タイトル) per contracts/ui-contract.md; Japanese UI strings as wide literals
- [X] T015 [US1] Implement board renderer in src/game/render.hpp/.cpp: 8x8 grid, black/white discs, outer-ring visual emphasis (潮汐対象の判別可能表示), legal-move ghost markers, HUD: 手番表示・黒/白スコア（実時間更新）
- [X] T016 [US1] Implement click→cell input mapping in src/game/scenes.cpp: board offset/cell-size transform; 盤外・非法手クリック無視; dispatch to Match.apply_move/pass
- [X] T017 [US1] Implement flip animation in src/game/anim.hpp/.cpp: placed disc instant, flipped discs width-scale 1→0→1 over ~300ms simultaneous; input lock until complete
- [X] T018 [US1] Implement tide presentation in src/game/render.cpp + scenes.cpp: HUD 「次の潮汐まで: N 手」+ 方向アイコン ↻/↺ driven ONLY by TideClock.next_in/next_dir (SC-004); ring slide animation ~500ms on occurrence; count 0→8 switch after animation completes
- [X] T019 [US1] Implement synthesized sound in src/game/sound.hpp/.cpp: in-memory PCM via SoundMemLoad — 着石クリック/反転上昇ノイズ/潮汐掃引音/パス警告音/勝敗和音; hook into match events; no external assets
- [X] T020 [US1] Implement pass flow in src/game/scenes.cpp: 「◯ は置ける場所がありません — パス」1.5s display then continue; end detection → RESULT
- [ ] T021 [US1] Validate: run quickstart manual scenarios #2,#3,#4,#7 on hot-seat path; fix discrepancies

**Checkpoint**: US1 fully playable (MVP)

---

## Phase 4: User Story 2 - コンピューターと3段階で対戦 (Priority: P1)

**Goal**: 易/普/強 AI 対局、潮汐を見据えた手、UI 応答性維持

**Independent Test**: CoreTests の AI 合法手保証 + quickstart #1・#8

- [X] T022 [US2] Implement AI engine in src/core/ai.hpp: shared evaluation (position weights corner>edge>x-square, mobility diff, tide-proximity bonus: when next_in ≤4 average position weights of current and pending-rotation board) + alpha-beta over Match copies (tide fires inside search); levels EASY=depth1 weighted-random among top (+random chance), MEDIUM=depth4, STRONG=iterative deepening up to depth7 with move ordering and 2.5s budget; seeded RNG for determinism
- [X] T023 [US2] Write AI tests in src/tests/main.cpp: fixed-seed 200 self-play plausibility runs — returned moves always legal; deterministic given seed; tide-proximity branch exercised (force next_in=1 position)
- [X] T024 [US2] Implement MODE_SELECT AI branch + MATCH_SETUP in src/game/scenes.cpp: 対コンピューター→易/普/強選択→石色ランダム決定（std::random_device）と通知「あなたは黒/白です」; 先手は常に黒
- [X] T025 [US2] Implement AI worker thread in src/game/scenes.cpp + anim: std::jthread one-shot thinking, mutex-guarded result slot, atomic done flag; input locked while thinking (FR-009); HUD 「CPU: 易/普/強」display vs-AI only
- [ ] T026 [US2] Validate: quickstart #1・#8; observe AI takes corner-class points when offered (SC-005 spot check)

**Checkpoint**: US1 + US2 independently functional

---

## Phase 5: User Story 3 - 2人で同じ画面で対局 (Priority: P2)

**Goal**: 同卓モードの明示的フロー（プレイヤー選択・手番明示）完成

**Independent Test**: quickstart #6

- [X] T027 [US3] Complete hot-seat flow in src/game/scenes.cpp: 「2人で対局」entry + MATCH_SETUP 黒番プレイヤー selection; ensure 手番表示 alternates correctly and both players see identical tide events
- [ ] T028 [US3] Validate: quickstart #6 (alternating clicks, turn display correctness)

**Checkpoint**: US1+US2+US3 independently functional

---

## Phase 6: User Story 4 - ゲーム内ヘルプ (Priority: P2)

**Goal**: 潮汐ルール説明 + Before/After 具体例、中断・復帰

**Independent Test**: quickstart #5

- [X] T029 [US4] Implement HELP scene in src/game/help.hpp/.cpp: content per ui-contract.md §Help (基本3行/潮汐規則/Before→After 盤面図2枚（core の初期盤＋回転後盤を実描画）/戦略ヒント); open from TITLE and PLAYING (H key + button), close restores exact state
- [ ] T030 [US4] Validate: quickstart #5

**Checkpoint**: All user stories functional

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T031 Zero-warning enforcement: build Debug+Release with /W4, fix all warnings in both projects
- [ ] T032 [P] Edge-case hardening pass: rapid clicks during AI thinking/animation (no state corruption), window minimize/restore redraw correctness
- [X] T033 [P] Write README.md (Japanese): 遊び方（潮汐ルール要約）・ビルド/テスト手順（quickstart 準拠）・画面説明
- [ ] T034 Final validation: CoreTests all-pass + full quickstart manual checklist #1–#8 executed; record results in specs/001-novel-othello/validation.md

---

## Phase 8: 追加要求 — 演出強化と AI 対 AI (FR-014〜FR-016, US5)

- [X] T035 Implement particle/effects system in src/game/fx.hpp/.cpp (spawn, update by dt, draw; foam/confetti kinds) and screen-shake offset
- [X] T036 Tide effects in src/game/scenes.cpp + render: 1000ms tide stage with cut-in band, ring light sweep in rotation direction, foam particles from moving ring stones, decaying board shake (FR-014)
- [X] T037 Transitions/result effects in src/game/scenes.cpp: scene fade-in ~300ms, result panel slide-in, score count-up, confetti on win (none when human loses vs AI) (FR-015)
- [X] T038 [US5] AI vs AI mode in src/game/scenes.cpp: MODE_SELECT entry, MATCH_SETUP per-color level selection, board clicks ignored, ≥700ms per move pacing, labels/HUD/result texts (FR-016)
- [X] T039 Validate: build 0 warnings, CoreTests pass, screenshots of tide effect / result / AI vs AI to completion

## Dependencies & Execution Order

- **Setup (P1 phase)** → **Foundational (phase 2, blocking gate T012)** → US1 → {US2, US3, US4} → Polish
- Within Foundational: T005→T006/T007→T008; tests T010/T011 after T009+T005-T008
- Within US1: T013→T014→T015→T016→(T017,T018,T019,T020 can parallelize by file)→T021
- US2 depends on core (phase 2) only, not on US1 internals except scene hooks (T024/T025 extend T014)
- US3 is mostly validation of already-built hot-seat path (deliberately small)
- Polish after all desired stories

## Parallel Opportunities

- T003 ∥ T002; T010 ∥ T011; T017/T018/T019/T020 (separate files); T032 ∥ T033
- After T012: US1 then US2∥(US3,US4 once scenes exist)

## Implementation Strategy

MVP = Phase1+2+US1 → validate hot-seat tide game end-to-end → add AI (US2) → modes polish (US3/US4) → warnings/docs/final validation. Commit per task group with task IDs in message (constitution workflow rule).

## Notes

- Core files NEVER include DxLib headers (Constitution I). Game layer never mutates core state except via Match methods.
- All Japanese strings are wide literals in UTF-8(BOM) sources with UNICODE defined (research R2).
