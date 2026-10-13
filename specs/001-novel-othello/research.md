# Phase 0 Research: 潮汐オセロ (Tidal Reversi)

All Technical Context unknowns resolved. No NEEDS CLARIFICATION remains.

## R1. DxLib の取得とリンク

- **Decision**: DxLib Ver3.25a VisualStudio 用を公式 URL `https://dxlib.xsrv.jp/DxLib/DxLib_VC3_25a.zip`
  から取得し、必要分のみを抽出して `vendor/DxLib/`（include 5 ヘッダ + x64 lib）としてリポジトリに vendoring 済み（実測 136MB）。
- **Rationale**: 憲法 V（単一ビルド経路・自己完結）。公式パッケージのリンク機構は `DxDataTypeWin.h` 内の
  `#pragma comment(lib, ...)` による自動選択で、`_MSC_VER>=1900 && _DLL && _WIN64` の分岐が
  `DxLib/DxLibW/DxUseCLib/DxUseCLibW/DxDrawFunc` の `_vs2015_x64_MD(d).lib` を選ぶことを実ファイルで確認した。
  vcxproj には AdditionalIncludeDirectories / AdditionalLibraryDirectories の設定のみが必要（追加依存指定は不要）。
- **Alternatives considered**: 公式の全 lib 同梱（690MB — 過大）/ GitHub ミラー yumetodo/DxLib（非公式）/
  NuGet（憲法の追加依存制限に抵触）。いずれも不採用。

## R2. Unicode と日本語テキスト描画

- **Decision**: `UNICODE` を定義して DxLib のワイド文字 API（DrawString 等の _W 版）を使う。
  自前ソースは UTF-8 (BOM) で書き、リテラルは `L"..."` ワイド文字列とする。
- **Rationale**: MSVC は BOM 付き UTF-8 ソースを正しく解釈し、ワイドリテラル経由なら日本語が環境ロケールに
  依存せず安定。Shift-JIS の char* リテラルはシステムロケール依存で壊れやすい。pragma は UNICODE 分岐で
  DxLibW/DxUseCLibW を選ぶ（vendoring 済み）。
- **Alternatives considered**: Shift-JIS ソース + /source-charset:.932（ロケール依存で脆弱）、UTF-8 char* を
  実行時に変換（余分コード）。不採用。

## R3. 潮汐メカニクスのデータ構造

- **Decision**: 最外周リングを時計回りの 28 個の (r,c) 配列 `RING` として 1 度だけ構築し（(0,0) 起点:
  上辺→右辺→下辺逆走→左辺上り）、回転は `dst[(i+1)%28] = src[i]`（時計）/ `dst[(i-1+28)%28] = src[i]`（反時計）の
  一時配列コピー。内側 6x6 は不変。TideClock は「実手数 counter・次回まで残り・次回方向」のみ保持。
- **Rationale**: 盤の直線反転評価とは独立で、回転は O(28)。方向交互は bool トグル。計数は match 状態機械の
  「着手成功」経路だけでインクリメントし、パス・非法手では不変（spec の計数規則と一致）。終局判定を潮汐より
  先に評価する順序を match.apply_move の契約とする（Edge Case「終局優先」）。
- **Alternatives considered**: リングを盤とは別ストレージに持つ（盤と二重管理でバグ源）。不採用。

## R4. AI（易/普/強）と潮汐先読み

- **Decision**: 同一の評価関数 + alpha-beta 探索、深さで差別化: 易=深さ1+手質上位から重み付きランダム、
  普=深さ4、強=深さ7（着手順序付き）。評価項目: 位置重み（角>辺>危険辺内側）、可搬性差、確定石数、
  **潮汐近接ボーナス**: 次回潮汐まで残り手数 R が小さい場合、リング上の石について「R 手後に潮汐を適用した
  仮想盤」での位置価値を再計算して加算（R<=4 のときのみ、コスト O(合法手×28)）。
- **Rationale**: 深さ差別は実装が単一で手質差が保証しやすい（SC-005）。潮汐先読みは「R 手後に実際に起きる
  変換」を 1 回適用するだけで spec の「潮汐後の盤面を見据えた選択」を満たす。完全な多手潮汐シミュレーションは
  探索コストに対して効果が小さいため不要（YAGNI）。
- **Alternatives considered**: ポジションテーブル事前計算、NN エンジン（依存・复杂度過大）、手毎に全潮汐適用の
  完全シミュレーション（探索爆発）。不採用。

## R5. 描画・アニメーション・効果音

- **Decision**: 60FPS 固定ループ。反転アニメは石の横幅係数 1→0→1（約 300ms）、潮汐スライドはリング全石を
  弧に沿って補間（約 500ms）。アニメ中は入力ロック（spec の「アニメ中输入は状態を壊さない」を満たす最小実装）。
  効果音は PCM をメモリ生成して `SoundMemLoad` で再生（着石=短いクリック、反転=上昇ノイズ、潮汐=低い掃引音、
  勝敗=和音）。外部アセットゼロ。
- **Rationale**: SC-006 の即時応答と憲法 III を満たす最小構成。合成音は追加ファイル不要で単一ビルド経路に親和。
- **Alternatives considered**: 外部効果音アセット（配布复杂度）、スレッド非同期アニメ（同期アニメで十分）。不採用。

## R6. スレッドモデル

- **Decision**: メインループは UI/入力/描画のみ。AI 思考は `std::jthread` でワーカースレッド 1 本、結果は
  atomic フラグ + ミューテックス付き結果スロットで受け取る。思考中は盤面クリックを受け付けない（FR-009）。
- **Rationale**: UI 応答性（SC-006）と「AI 思考中の連打も状態を壊さない」の直接充足。コアは不変コピーに対する
  純関数なのでスレッドセーフ。
- **Alternatives considered**: 同期思考（強レベルで UI フreeze）、フレーム分割探索（実装复杂）。不採用。

## R7. テスト戦略（憲法 I: コアはグラフィクスなしで単体検証可能）

- **Decision**: `CoreTests` は assert ベースの console runner（追加テストフレームワークなし）。対象:
  合法手生成（標準ケース/角/辺/内側）、反転結果、パス・終局・引き分け判定、**潮汐回転の正しさ**（時計/反時計、
  角→辺移動、8 実手で正確に 1 回発火、パスは計数しない）、AI が常に合法手のみを返す（乱席 200 試行・シード固定）、
  潮汐カウント表示値と match 内部状態の一致（SC-004 の自動部分）。
- **Rationale**: 決定論的コア + 固定シードで再現性 100%。MSBuild で一緒にビルドされ quickstart から実行できる。
- **Alternatives considered**: GoogleTest/Catch2（追加依存 = 憲法違反）、CMakectest（二重ビルド経路）。不採用。

## R8. ビルド設定

- **Decision**: `Reversi.sln` + x64 Debug/Release。`/std:c++20 /W4 /utf-8 UNICODE _CRT_SECURE_NO_WARNINGS`、
  IncludePath=`vendor/DxLib/include`、AdditionalLibraryDirectories=`vendor/DxLib/lib`（pragma 自動リンク）。
  MSBuild は vswhere で所在解決（quickstart に記載）。
- **Rationale**: 警告ゼロ・単一経路。DxLib サンプル VS2022 用 vcxproj と同じリンク方式を確認済み。
- **Alternatives considered**: CMake（二重経路）。不採用。
