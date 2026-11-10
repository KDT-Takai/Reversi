@echo off
rem =====================================================================
rem 起動.bat - Reversi 起動用パッチ
rem Visual Studio を開かず、ゲーム(実行画面)だけを起動します。
rem
rem 使い方:
rem   ダブルクリック / 起動.bat        msbuild でビルドしてからゲームを実行(VS は開かない)
rem   起動.bat nobuild                 ビルドせず今ある exe を即実行
rem   起動.bat vs                    Visual Studio 2022 を開いて F5 デバッグ(従来動作)
rem
rem 補足:
rem   - vswhere で Visual Studio 2022 (v17) の MSBuild を探します(なければ最新版にフォールバック)。
rem   - ビルド構成は Debug x64(F5 と同じ)。ビルド失敗時は警告を出します。
rem   - ゲームは既に実行中の場合は二重起動しません。
rem   - 実際の処理は下の ::ps で始まる PowerShell 本体で行います。
rem =====================================================================
title Reversi起動
setlocal EnableExtensions
cd /d "%~dp0"

set "REVERSI_DIR=%~dp0"
set "REVERSI_SLN=%~dp0Reversi.sln"
set "REVERSI_MODE=run"
if /i "%~1"=="nobuild" set "REVERSI_MODE=nobuild"
if /i "%~1"=="vs" set "REVERSI_MODE=vs"

powershell -NoProfile -ExecutionPolicy Bypass -Command "$o = Get-Content -LiteralPath '%~f0' | Where-Object { $_ -like '::ps *' } | ForEach-Object { $_.Substring(4) }; $t = Join-Path $env:TEMP 'reversi_launch.ps1'; Set-Content -LiteralPath $t -Value $o -Encoding Default; & $t; $rc = $LASTEXITCODE; Remove-Item -LiteralPath $t -ErrorAction SilentlyContinue; exit $rc"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  echo 警告: 起動に失敗しました。終了コード %RC% です。内容を確認してください。
  pause
)
exit /b %RC%

::ps $ErrorActionPreference = 'Continue'
::ps $DIR = $env:REVERSI_DIR.TrimEnd('\')
::ps $SLN = $env:REVERSI_SLN
::ps $MODE = $env:REVERSI_MODE
::ps $EXE = Join-Path $DIR 'bin\x64\Debug\TidalReversi.exe'
::ps function Get-GameProc { Get-Process -Name 'TidalReversi' -ErrorAction SilentlyContinue }
::ps # Visual Studio 2022 (v17) を vswhere で探す。見つからなければ最新版にフォールバック。
::ps function Find-Install {
::ps   $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
::ps   if (-not (Test-Path -LiteralPath $vswhere)) { return $null }
::ps   $install = & $vswhere -latest -version '[17.0,18.0)' -property installationPath
::ps   if (-not $install) { $install = & $vswhere -latest -property installationPath }
::ps   return $install
::ps }
::ps if ($MODE -eq 'run' -or $MODE -eq 'nobuild') {
::ps   if (Get-GameProc) { Write-Output 'ゲームはすでに実行中です。何も行いません。'; exit 0 }
::ps   if ($MODE -eq 'run') {
::ps     $install = Find-Install
::ps     if (-not $install) { Write-Output 'エラー: Visual Studio が見つかりません。Visual Studio 2022 をインストールしてください。'; exit 1 }
::ps     $msbuild = Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe'
::ps     if (-not (Test-Path -LiteralPath $msbuild)) { Write-Output ('エラー: MSBuild.exe が見つかりません: ' + $msbuild); exit 1 }
::ps     Write-Output ('ビルド中... ' + $SLN)
::ps     & $msbuild $SLN /p:Configuration=Debug /p:Platform=x64 /v:minimal /nologo
::ps     if ($LASTEXITCODE -ne 0) { Write-Output ('エラー: ビルドに失敗しました(終了コード ' + $LASTEXITCODE + ')。'); exit 5 }
::ps   }
::ps   if (-not (Test-Path -LiteralPath $EXE)) { Write-Output ('エラー: 実行ファイルがありません: ' + $EXE); Write-Output '先に 起動.bat(ビルド込み)を実行してください。'; exit 6 }
::ps   Start-Process $EXE -WorkingDirectory $DIR
::ps   Write-Output 'ゲームを起動しました。'
::ps   exit 0
::ps }
::ps # vs モード: Visual Studio を開いて F5 と同じ Debug.Start を行う。
::ps if (-not (Test-Path -LiteralPath $SLN)) { Write-Output ('エラー: ソリューションが見つかりません: ' + $SLN); exit 1 }
::ps $install = Find-Install
::ps if (-not $install) { Write-Output 'エラー: Visual Studio が見つかりません。Visual Studio 2022 をインストールしてください。'; exit 1 }
::ps $devenv = Join-Path $install 'Common7\IDE\devenv.exe'
::ps if (-not (Test-Path -LiteralPath $devenv)) { Write-Output ('エラー: devenv.exe が見つかりません: ' + $devenv); exit 1 }
::ps Write-Output ('Visual Studio を起動します: ' + $SLN)
::ps Start-Process $devenv -ArgumentList ('"' + $SLN + '"')
::ps if (Get-GameProc) { Write-Output 'すでにデバッグ中です。何も行いません。'; exit 0 }
::ps $progids = @('VisualStudio.DTE.17.0', 'VisualStudio.DTE.18.0')
::ps $dte = $null
::ps $deadline = (Get-Date).AddSeconds(120)
::ps while ((Get-Date) -lt $deadline) {
::ps   foreach ($p in $progids) {
::ps     try {
::ps       $cand = [Runtime.InteropServices.Marshal]::GetActiveObject($p)
::ps       if ($cand.Solution.IsOpen -and $cand.Solution.FullName -eq $SLN) { $dte = $cand; break }
::ps     } catch {}
::ps   }
::ps   if ($dte) { break }
::ps   Start-Sleep -Seconds 1
::ps }
::ps if (-not $dte) { Write-Output 'エラー: Visual Studio への接続がタイムアウトしました。Visual Studio 上で F5 を押してください。'; exit 2 }
::ps Write-Output ('Visual Studio ' + $dte.Version + ' に接続しました')
::ps try {
::ps   $dte.ExecuteCommand('Debug.Start')
::ps   Write-Output 'デバッグ開始コマンドを送信しました'
::ps } catch {
::ps   Write-Output ('Debug.Start 失敗: ' + $_.Exception.Message)
::ps   try {
::ps     $dte.ExecuteCommand('Project.SetStartupProject', 'TidalReversi')
::ps     $dte.ExecuteCommand('Debug.Start')
::ps     Write-Output 'TidalReversi をスタートアッププロジェクトに設定してデバッグを開始しました'
::ps   } catch {
::ps     Write-Output 'エラー: 自動デバッグを開始できませんでした。Visual Studio 上で F5 を押してください。'
::ps     exit 3
::ps   }
::ps }
::ps # ビルド~アプリ起動(F5 と同じ完了状態)を最大10分まで待つ。進捗を30秒ごとに表示。
::ps $start = Get-Date
::ps $ok = $false
::ps $lastNote = 0
::ps while (((Get-Date) - $start).TotalSeconds -lt 600) {
::ps   if (Get-GameProc) { $ok = $true; break }
::ps   $el = [int]((Get-Date) - $start).TotalSeconds
::ps   if ($el -ge $lastNote + 30) { $lastNote = $el; Write-Output ('ビルド/起動を待っています... ' + $el + '秒') }
::ps   Start-Sleep -Milliseconds 500
::ps }
::ps if ($ok) {
::ps   Write-Output 'デバッグが開始されました。'
::ps } else {
::ps   Write-Output '警告: デバッグの起動が検知できません。Visual Studio を確認してください。'
::ps   exit 4
::ps }
