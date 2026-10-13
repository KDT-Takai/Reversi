# Implementation Plan: 潮汐オセロ (Tidal Reversi)

**Branch**: `001-novel-othello` | **Date**: 2026-10-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-novel-othello/spec.md`

## Summary

標準オセロ規則に「8実手ごとに最外周リングが交互方向で1マス回転する」潮汐メカニクスを加えたデスクトップゲーム。
純粋 C++20 のルールコア（DxLib 非依存・決定論的）と DxLib による描画/入力/音の層を分離し、Visual Studio 2022
ソリューション 1 本でビルド可能にする。AI は同一コアを用いた探索レベル 3 段階（易/普/強）。

## Technical Context

**Language/Version**: C++20 (MSVC, /W4 warning-clean)

**Primary Dependencies**: DxLib Ver3.25a（プロジェクト内 vendoring: `vendor/DxLib/`。公式 https://dxlib.xsrv.jp/DxLib/DxLib_VC3_25a.zip から取得済み。ヘッダ 5 種 + x64 MD/MDd lib 一式）

**Storage**: なし（v1 は永続化なし。FR-013 範囲外）

**Testing**: `CoreTests` コンソールプロジェクト（assert ベースの决定论的テストランナー。コアの合法手・反転・潮汐・AI 合法手検証）

**Target Platform**: Windows x64 デスクトップアプリ（固定ウィンドウ 1280x720）

**Project Type**: desktop-app（単一ソリューション、ゲーム exe + テスト exe の 2 プロジェクト）

**Performance Goals**: 描画 60 FPS、入力応答 <100ms、AI 強レベルの 1 手思考 <3s（UI スレッドを止めない: 思考はワーカースレッド）

**Constraints**: オフライン動作、追加サードパーティ依存なし、効果音はプログラム生成 PCM（外部アセットなし）

**Scale/Scope**: コア ~800 行 + UI ~900 行 + テスト ~300 行（推定）。8x8 盤・単一画面フロー

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status | Basis |
|------|--------|-------|
| I. ルールコアの独立性 | PASS | `src/core/` は DxLib を一切 include しない純 C++20。`CoreTests` はグラフィクスなしで実行 |
| II. 単純さ (YAGNI) | PASS | FR-013 の範囲外機能をすべて排除。設定永続化・オンライン対戦なし |
| III. プレイヤー体験の優先 | PASS | 60FPS ループ、AI 思考は別スレッド、合法手/潮汐カウントを常時表示、日本語 UI |
| IV. 明示された要件のみ実装 | PASS | spec はインタビュー 3 ラウンドで確定済み。NEEDS CLARIFICATION ゼロ |
| V. 単一ビルド経路 | PASS | DxLib は `vendor/DxLib/` に自己完結。`.sln` 1 本で MSBuild ビルド、手作業手順なし |

**Post-Phase-1 re-check**: PASS（追加依存なし、構造はコア/UI 分離を維持）

## Project Structure

### Documentation (this feature)

```text
specs/001-novel-othello/
├── plan.md              # this file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (UI contract)
└── tasks.md             # Phase 2 output (/speckit-tasks - NOT created here)
```

### Source Code (repository root)

```text
Reversi.sln
vendor/DxLib/                 # vendored DxLib Ver3.25a (headers + x64 libs) — committed to repo
├── include/                  # DxLib.h, DxDataTypeWin.h 等 5 headers
└── lib/                      # x64 MD/MDd static libs (pragma auto-link 対応)
src/
├── core/                     # 純 C++20 ルールコア（DxLib include 禁止）
│   ├── board.hpp             # 8x8 盤・リング定義・潮汐回転
│   ├── rules.hpp             # 合法手生成・反転適用・パス・終局判定
│   ├── match.hpp             # 対局状態機械（TideClock 含む）
│   └── ai.hpp                # 評価関数 + alpha-beta（易/普/強）+ 潮汐先読み
├── game/                     # DxLib 層
│   ├── main.cpp              # エントリ・ウィンドウ・メインループ
│   ├── scenes.hpp/.cpp       # タイトル/モード選択/対局/結果/ヘルプ の状態遷移
│   ├── render.hpp/.cpp       # 盤面・石・合法手マーカー・潮汐リング強調・カウント HUD
│   ├── anim.hpp/.cpp         # 反転アニメ・潮汐スライドアニメ（入力ロック）
│   └── sound.hpp/.cpp        # 合成 PCM 効果音（着石/反転/潮汐/勝敗）
└── tests/
    └── main.cpp              # CoreTests エントリ（core/*.hpp を直接コンパイル、assert 群）
```

**Structure Decision**: 単一ソリューション `Reversi.sln` に 2 つの C++ プロジェクト:
`TidalReversi`（ゲーム exe, src/core + src/game）と `CoreTests`（コンソール exe, src/core + src/tests）。
コアは共有ソースを両プロジェクトが直接コンパイルする（中間ライブラリを作らない = 単純さ）。
DxLib は `vendor/DxLib/` を include/lib ディレクトリとして vcxproj に相対パス設定。

## Complexity Tracking

> No violations — gate table all PASS. No extra justification needed.
