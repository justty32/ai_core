# run.ps1 — Windows（PowerShell）版：編譯＋跑 lab/cpp 的 play.cpp，預設打離線 fixture。
#
# 對應 POSIX 的 run.sh，但不靠 pkg-config（Windows 無）。改為就地把編譯條件湊齊：
#   ① include layout：play.cpp 用 <cllm/llm.hpp> <cllm/llm_reflect.hpp>，其中 llm.hpp 又
#      #include <cllm/cabi.hpp>（再牽出一串 cabi*.{h,hpp}）。這些頭散在 core/bindings/cpp/
#      與 core/src/，故先組一個 .cllm-include/cllm/ 夾把它們全放進去（冪等；已存在就略過）。
#   ② glaze：-I core/build/vcpkg_installed/x64-mingw-static/include（llm_reflect.hpp 要）。
#   ③ 連結：-L core/build -lcllm（吃 libcllm.dll.a）；執行期把 core/build 前置到 PATH 讓
#      libcllm.dll 找得到。
#   ④ fixtures：用真磁碟路徑組 file:///C:/… 傳給 .play.exe（⚠ 不是 MSYS 的 /c/…）。
#
# 用：
#   pwsh -File run.ps1                              # 離線 fixture（預設）
#   pwsh -File run.ps1 http://localhost:1234/v1/    # 打真後端（結尾要有 /）
$ErrorActionPreference = "Stop"
$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$core    = Resolve-Path (Join-Path $here "..\..\core")
$build   = Join-Path $core "build"
$dll     = Join-Path $build "libcllm.dll"
$glaze   = Join-Path $build "vcpkg_installed\x64-mingw-static\include"
$fxdir   = Join-Path $core "test\fixtures"

if (-not (Test-Path $dll))   { Write-Error "找不到 $dll —— 先建置 cllm：cd core; cmake --preset mingw-debug; cmake --build --preset mingw-debug" }
if (-not (Test-Path $glaze)) { Write-Error "找不到 glaze include（$glaze）—— 先建置 cllm（vcpkg 會裝好 glaze）" }

# ① 組 <cllm/…> include layout：把便利層＋cabi 全頭複製進 .cllm-include/cllm/（冪等覆蓋）
$inc = Join-Path $here ".cllm-include"
$cllmdir = Join-Path $inc "cllm"
New-Item -ItemType Directory -Force -Path $cllmdir | Out-Null
Copy-Item (Join-Path $core "bindings\cpp\llm.hpp")         $cllmdir -Force
Copy-Item (Join-Path $core "bindings\cpp\llm_reflect.hpp") $cllmdir -Force
Get-ChildItem (Join-Path $core "src") -Filter "cabi*.h"   | Copy-Item -Destination $cllmdir -Force
Get-ChildItem (Join-Path $core "src") -Filter "cabi*.hpp" | Copy-Item -Destination $cllmdir -Force

# ② 編譯
$exe = Join-Path $here ".play.exe"
$gpp = @("-std=c++23", (Join-Path $here "play.cpp"),
         "-I$inc", "-I$glaze", "-L$build", "-lcllm", "-o", $exe)
& g++ @gpp
if ($LASTEXITCODE -ne 0) { Write-Error "g++ 編譯失敗（exit $LASTEXITCODE）" }

# ③ 執行：build 前置 PATH（找 libcllm.dll 與 llm.exe），console 走 UTF-8（顯示中文）
$env:PATH = $build + ";" + $env:PATH
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$base = if ($args.Count -ge 1) { $args[0] } else {
  "file:///" + ((Resolve-Path $fxdir).Path -replace '\\','/') + "/"
}
& $exe $base
exit $LASTEXITCODE
