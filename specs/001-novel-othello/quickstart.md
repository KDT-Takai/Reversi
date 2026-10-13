# Quickstart: 潮汐オセロ (Tidal Reversi)

## Prerequisites

- Windows 10/11 x64
- Visual Studio 2022（v143）または Visual Studio 18（v145）の「デスクトップ用 C++」ワークロード（ツールセットは vcxproj が自動選択）
- その他のインストール不要（DxLib は `vendor/DxLib/` に同梱済み）

## Build

```powershell
# リポジトリルートで。vswhere で MSBuild を解決:
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild Reversi.sln /p:Configuration=Release /p:Platform=x64 /m
```

成果物: `bin/x64/Release/TidalReversi.exe`, `bin/x64/Release/CoreTests.exe`（Debug も同様）。

## Automated validation (コアの正しさ — 憲法 I)

```powershell
.\bin\x64\Release\CoreTests.exe
# 期待: 全テスト PASS、終了コード 0。失敗時は各 assert の内容と再現シードを表示
```

カバー範囲: 合法手/反転/パス/終局/引き分け、潮汐回転（時計・反時計・8 実手で 1 発火・パス非計数）、
AI の合法手保証（固定シード 200 試行）、潮汐カウント一致（SC-004 自動部分）。

## Manual validation scenarios (spec 受入シナリオ対応)

| # | 手順 | 期待結果 |
|---|------|----------|
| 1 | TidalReversi.exe 起動→開始→対AI(普) | 石色のランダム通知後に対局開始。盤・手番・スコア・潮汐カウント表示（US1-1） |
| 2 | 8 手打つ | 8 手目の処理直後にリングスライドアニメ+掃引音。9 手目は回転後盤で続行。方向アイコンが ↻→↺ に変化（US1-3, R3） |
| 3 | 対局中任意時点のカウント照合 | 表示「次の潮汐まで N 手」と実際の発生間隔が常に 8（SC-004） |
| 4 | 角を取り、8 手後にその角マスに他石が移動する様子を観察 | 標準定石（角不変前提）が通用しない局面の目撃（SC-003） |
| 5 | ヘルプを開く→閉じる | 対局が同じ状態で復帰。タイトルからも閲覧可（US4） |
| 6 | 同卓モードで交互にクリック | 手番表示が交互に切り替わる。非法手クリックは無反応（US3, FR-009） |
| 7 | 終局まで進行（または合法手消失） | 勝敗/引き分け+最終スコア表示。もう一度→再対局（US1-6） |
| 8 | AI 思考中に連打 | 反応なし・クラッシュなし。AI の手が盤面に反映（Edge Cases） |

## Performance smoke

- FPS: 対局中に体感滑らか（60FPS 固定ループ）。AI 強レベルの 1 手は最大 ~3s で、その間 UI は応答（SC-006）。
