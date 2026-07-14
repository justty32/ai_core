#!/usr/bin/env bash
# build.sh — 建置全部產物到 build/：liblua.a（vendored Lua 5.5）、host.exe（內嵌 Lua 的 C++ host）、
#            lua.exe（標準 Lua 直譯器，給 debug／一般跑腳本用；內建 arg 表）
# 用（Git Bash / Linux bash）：
#   ./build.sh          # 全建（build/{liblua.a, host.exe, lua.exe}）
#   ./build.sh host     # 只重建 host.exe（liblua.a 已在時最常用）
#   ./build.sh luaexe   # 只重建 lua.exe
#   ./build.sh lua      # 只重建 liblua.a
#
# 跨平台：Windows（MinGW，本機 C:\dev\mingw64\bin）＋ Linux（原生 g++/gcc，如 Manjaro）。
# 產物名一律帶 .exe（Linux 不在意副檔名、指令跨平台一致）；全落 build/（gitignored）。
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build

# ── 平台偵測：Windows 需 MinGW + -municode（wmain 寬字元）＋ WinHTTP（native/http.c）；
#    Linux 原生、HTTP 走 libcurl。HTTPLIBS 連結到 host.exe/lua.exe（liblua.a 含 http.o 引用其符號）。
#
# ★ LUAPLAT：Lua 的平台開關。Windows 上 luaconf.h 由 _WIN32 自動定義 LUA_USE_WINDOWS，
#   POSIX 這邊卻要自己給 -DLUA_USE_LINUX，否則 io.popen 直接是「'popen' not supported」的
#   錯誤樁（liolib.c 的 l_popen 只在 LUA_USE_POSIX 下才接真 popen）、os.tmpname 也退回 tmpnam。
#   LUA_USE_LINUX 只再拉一個 -ldl（readline 是 lua.c 執行期 dlopen，不需 -lreadline）。
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    export PATH="/c/dev/mingw64/bin:$PATH"
    MUNICODE="-municode"
    HTTPLIBS="-lwinhttp"
    LUAPLAT=""          # luaconf.h 見 _WIN32 自己開 LUA_USE_WINDOWS
    PLATLIBS=""
    ;;
  *)
    MUNICODE=""
    HTTPLIBS="-lcurl"
    LUAPLAT="-DLUA_USE_LINUX"
    PLATLIBS="-ldl"     # LUA_USE_LINUX 連帶開 LUA_USE_DLOPEN
    ;;
esac

build_lua() {
  echo "[build] build/liblua.a（vendored Lua 除 lua.c/luac.c ＋ 我方 native/*.c）"
  local objs=()
  # vendored Lua 全部（除兩個 main）＋ native/*.c（cjson＝JSON codec、http＝HTTP 傳輸，經 linit.c 註冊）
  for f in $(ls vendor/lua/*.c | grep -vE '/(lua|luac)\.c$') native/*.c; do
    gcc -O2 $LUAPLAT -c "$f" -I vendor/lua -o "build/$(basename "${f%.c}").o"
    objs+=("build/$(basename "${f%.c}").o")
  done
  ar rcs build/liblua.a "${objs[@]}"
  rm -f "${objs[@]}"
  echo "[build] build/liblua.a 完成"
}

build_host() {
  echo "[build] build/host.exe"
  g++ -std=c++20 -O2 $LUAPLAT host.cpp -I vendor/lua -L build -llua -lm $MUNICODE $HTTPLIBS $PLATLIBS -o build/host.exe
  echo "[build] build/host.exe 完成"
}

build_luaexe() {
  echo "[build] build/lua.exe（標準直譯器 = lua.c + liblua.a）"
  gcc -O2 $LUAPLAT vendor/lua/lua.c -I vendor/lua -L build -llua -lm $HTTPLIBS $PLATLIBS -o build/lua.exe
  echo "[build] build/lua.exe 完成"
}

# ── compile_commands.json：給 clangd 用的 compilation database（編輯器中立：nvim／VSCode 皆讀）。
#   寫到「專案根」（不是 build/）——clangd 預設從當前檔案往上找這個檔，放 build/ 它不會找。
#   純 shell 手寫 JSON（鐵律零外部相依，不上 bear/compiledb）：對每個 .c/.cpp 原始檔各造一筆
#   {directory, file, command}，command 用「-c 編譯單元」形式（而非上面 build_* 實際下的連結指令）——
#   兩者差在連結旗標（-L/-l/$MUNICODE/$HTTPLIBS/$PLATLIBS），那些只影響連結、不影響單一檔案的
#   語法/型別/巨集分析，clangd 只要編譯期旗標就夠；唯一會動到編譯語意的是 $LUAPLAT
#   （-DLUA_USE_LINUX 那個巨集，見上面「LUAPLAT」註解），這條照抄目前偵測到的平台值，
#   故 compile_commands.json 會隨 build.sh 執行當下的平台自動反映正確旗標。
#   vendor/lua/lua.c／luac.c 平常不落進 liblua.a（各自的兩個 main），但這裡仍給一筆——
#   clangd 開這兩支檔時才看得懂 Lua 原始碼、不噴 include 找不到。
write_compile_commands() {
  local root="$PWD"
  local out="$root/compile_commands.json"
  echo "[build] compile_commands.json（clangd 用；涵蓋 host.cpp／native/*.c／vendor/lua/*.c）"

  # JSON 字串逃脫：反斜線先跳、雙引號再跳（路徑含空白/反斜線時安全）
  json_esc() {
    local s="$1"
    s="${s//\\/\\\\}"
    s="${s//\"/\\\"}"
    printf '%s' "$s"
  }

  local entries=()
  add_entry() {  # $1=相對檔案路徑  $2=完整編譯命令
    entries+=("  {\"directory\": \"$(json_esc "$root")\", \"file\": \"$(json_esc "$root/$1")\", \"command\": \"$(json_esc "$2")\"}")
  }

  add_entry "host.cpp" "g++ -std=c++20 -O2 $LUAPLAT -c host.cpp -I vendor/lua -o build/host.o"

  for f in vendor/lua/*.c native/*.c; do
    local base; base="$(basename "${f%.c}")"
    add_entry "$f" "gcc -O2 $LUAPLAT -c $f -I vendor/lua -o build/${base}.o"
  done

  {
    echo "["
    local n=${#entries[@]} i=0
    for e in "${entries[@]}"; do
      i=$((i + 1))
      if [ "$i" -lt "$n" ]; then echo "$e,"; else echo "$e"; fi
    done
    echo "]"
  } > "$out"
  echo "[build] compile_commands.json 完成（$out）"
}

case "${1:-all}" in
  lua)    build_lua ;;
  host)   build_host ;;
  luaexe) build_luaexe ;;
  all)    build_lua; build_host; build_luaexe ;;
  *) echo "用法：./build.sh [all|lua|host|luaexe]"; exit 1 ;;
esac
write_compile_commands
echo "[build] OK"
