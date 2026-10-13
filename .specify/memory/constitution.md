<!--
Sync Impact Report:
Version change: (template scaffold) → 1.0.0 (initial ratification)
Modified principles: [PRINCIPLE_1..5_NAME] → I.ルールコアの独立性 / II.単純さ / III.プレイヤー体験の優先 / IV.明示された要件のみ実装 / V.単一ビルド経路
Added sections: Technology & Platform Constraints, Development Workflow, Governance(確定内容)
Removed sections: none (template placeholders all resolved)
Follow-up TODOs: none
-->

# Reversi (革新的オセロ) Constitution

## Core Principles

### I. ルールコアの独立性 (Rules-Core Independence)

ゲームルール（盤面・合法手・反転・勝敗判定）は DxLib や描画コードに一切依存しない純粋な C++20 コアとして実装する。
コアは決定論的で、グラフィクスなしで単体検証可能でなければならない。

理由: ルール誤りはゲームの信頼性を根源的に壊す。コアを独立させれば検証・改修が安価になる。

### II. 単純さ (Simplicity / YAGNI)

機能は仕様で要求された最小構成から始める。投機的な機能（使われる保証のない設定、前倒しの汎用化や最適化）は入れてはならない。
複雑さを入れる場合は仕様側の根拠を明示する。

理由: 範囲の膨張は完成度を下げる。小さなコードベースは検証と修正が速い。

### III. プレイヤー体験の優先 (Player-Experience First)

入力への応答は 100 ms 以内に視覚的に反映され、描画は安定 60 FPS を維持する。UI 文言は日本語で、
盤面状態・合法手・得点は一目で判読できる。エラー時はプレイヤー向けメッセージを表示し、クラッシュしない。

理由: ゲームの価値は操作感と可読性で決まる。ロジックが正しくても伝わらなければ失敗である。

### IV. 明示された要件のみ実装 (Spec-Driven Scope)

曖昧な仕様を推測で実装しない。仕様の空白はインタビュー（/speckit-clarify 等）で解決してから確定する。
実装中に新たな要件が必要になった場合は、まず仕様へ反映する。

理由: 本プロジェクトは要件化を設計の起点とする契約で始まる。推測実装は手戻りの最大要因である。

### V. 単一ビルド経路 (Single Build Path)

成果物は Visual Studio のソリューションファイル（MSBuild）だけでビルド可能でなければならない。
ビルドのための手作業手順（ライブラリの手動コピー等）を要求してはならない。依存ライブラリはプロジェクト内に自己完結で配置する。

理由: 「クローンしてビルドすれば動く」状態が検証可能性の最低条件である。

## Technology & Platform Constraints

- 対象 OS: Windows x64（Windows 10 以降）。
- 言語: C++20（MSVC）。警告レベル /W4 で警告ゼロを維持する。
- 開発環境: Visual Studio 2022 と互換性のあるソリューション形式 (.sln/.vcxproj)。ビルドは MSBuild で行う。
- グラフィクス/入力ライブラリ: DxLib（プロジェクト内に vendoring し、相対パスで解決する）。
- 追加のサードパーティ依存は認めない（追加は憲法改正事項）。

## Development Workflow

- 各機能は spec → plan → tasks の順で文書化してから実装する。
- コミットは tasks.md のタスク単位で行い、コミットメッセージにタスク番号を含める。
- 機能の完了判定には spec の受入シナリオに基づく手動テストチェックリストの実行を含める。
- レビュー時は本憲法の各原則への適合を確認し、違反は修正か正当化の記録を要求する。

## Governance

本憲法は開発慣習に優先する。改正は (1) 差分の文書化、(2) ユーザー（発注者）の承認、(3) 影響を受ける
spec/plan の更新手順の明記を必要とする。バージョンは意味的バージョニングに従う（MAJOR: 原則の削除・再定義、
MINOR: 原則・節の追加、PATCH: 文言の明確化）。コンプライアンス確認は各機能完了時のチェックで行う。

**Version**: 1.0.0 | **Ratified**: 2026-10-13 | **Last Amended**: 2026-10-13
