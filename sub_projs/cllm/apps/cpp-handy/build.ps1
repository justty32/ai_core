# build.ps1 — Windows（PowerShell）版：編 cpp-handy 四工具成 bin\<tool>.exe。
#
# 對應 POSIX 的 build.sh。純 shell-out 應用：不 link libcllm（真正的「手」在 runtime 轉呼
# 外部 llm/claude/sibling），只要一支 C++23 編譯器。Windows 額外連 shell32（util.hpp 的
# CommandLineToArgvW，把 UTF-8 中文參數還原——見該檔 HANDY_INIT_ARGV）。
#
# 用：
#   pwsh -File build.ps1                 # → bin\{llme,zhtw,wf,mail}.exe
#   $env:CXX="clang++"; pwsh -File build.ps1   # 換編譯器
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

$cxx = if ($env:CXX) { $env:CXX } else { "g++" }
$std = if ($env:STD) { $env:STD } else { "c++23" }
New-Item -ItemType Directory -Force -Path bin | Out-Null

foreach ($t in "llme", "zhtw", "wf", "mail") {
  Write-Host "· 編 $t：$cxx -std=$std -Isrc src\$t.cpp → bin\$t.exe"
  & $cxx "-std=$std" -O2 -Isrc "src\$t.cpp" -o "bin\$t.exe" -lshell32
  if ($LASTEXITCODE -ne 0) { Write-Error "編譯 $t 失敗（exit $LASTEXITCODE）" }
}
Write-Host "· 完成 → bin\{llme,zhtw,wf,mail}.exe"
