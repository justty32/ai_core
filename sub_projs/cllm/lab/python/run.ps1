# run.ps1 — Windows（PowerShell）版：跑 lab/python 的 play.py，預設打離線 fixture。
#
# 對應 POSIX 的 run.sh，但不靠 ~/dev/cllm/env.sh／pkg-config（Windows 無）。改為就地把
# 環境湊齊：
#   ① PYTHONPATH ← core/bindings/python（play.py 的 `import llm` 從這裡找）
#   ② LIBCLLM    ← core/build/libcllm.dll（llm.py 現已依平台自動選 .dll，這裡仍顯式指明最穩）
#   ③ PATH       ← 前置 core/build（play.py 第 7 步 shell-out 呼 llm.exe）
#   ④ fixtures   ← 用「真磁碟路徑」組 file:///C:/… 傳給 play.py（⚠ 不是 MSYS 的 /c/…）
#
# 用：
#   pwsh -File run.ps1                         # 離線 fixture（預設）
#   pwsh -File run.ps1 http://localhost:1234/v1/   # 打真後端（LM Studio 等；結尾要有 /）
$ErrorActionPreference = "Stop"
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$core   = Resolve-Path (Join-Path $here "..\..\core")
$dll    = Join-Path $core "build\libcllm.dll"
$fxdir  = Join-Path $core "test\fixtures"

if (-not (Test-Path $dll)) {
  Write-Error "找不到 $dll —— 先建置 cllm：cd core; cmake --preset mingw-debug; cmake --build --preset mingw-debug"
}

# fixtures → file:///C:/… URI（play.py 的 ep() 直接字串串接，故 base 結尾要有 /）
$base = if ($args.Count -ge 1) { $args[0] } else {
  "file:///" + ((Resolve-Path $fxdir).Path -replace '\\','/') + "/"
}

$env:PYTHONPATH = (Resolve-Path (Join-Path $core "bindings\python")).Path
$env:LIBCLLM    = $dll
$env:PATH       = (Join-Path $core "build") + ";" + $env:PATH
# 中文輸出：確保 stdout 走 UTF-8（Windows 終端預設碼頁常為 950）
$env:PYTHONIOENCODING = "utf-8"

python (Join-Path $here "play.py") $base
exit $LASTEXITCODE
