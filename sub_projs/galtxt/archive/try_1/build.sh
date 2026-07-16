#!/usr/bin/env bash
# build.sh —— 編 s7host.exe（argv-aware s7 host：s7.c 當庫＋我方 s7host.c）。
#
# s7 runtime 原始碼（s7.c/s7.h）不在本 repo 版控，來自另一個 repo：s7-playground。
# 用環境變數 S7_DIR 指定其路徑；沒指定就依平台試預設候選路徑，都找不到就報錯教你怎麼補
# （去那顆 repo 跑 setup.sh 抓 s7.c/s7.h）。
#
# 跨平台：Windows（MinGW，-municode＋shim_include 補 <sys/utsname.h>）
#        ＋ Linux（原生 gcc，WITH_C_LOADER 預設開，需 -ldl -Wl,-export-dynamic；
#          shim_include 絕不能進 include path —— 會蓋掉系統真正的 <sys/utsname.h>）。
# 產物固定叫 s7host.exe（Linux 不在意副檔名、指令跨平台一致；.gitignore 已蓋）。
#
# 用法：
#   ./build.sh                 # 用預設候選路徑找 s7-playground
#   S7_DIR=/path/to/s7-playground ./build.sh   # 手動指定
set -euo pipefail
cd "$(dirname "$0")"

find_s7_dir() {
  if [ -n "${S7_DIR:-}" ]; then
    if [ -f "$S7_DIR/s7.c" ]; then echo "$S7_DIR"; return 0; fi
    echo "S7_DIR=$S7_DIR 底下沒有 s7.c" >&2
    return 1
  fi
  local candidates=()
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
      candidates=("C:/code/mine/pas/derived/s7-playground")
      ;;
    *)
      candidates=("$HOME/repo/pas/derived/s7-playground")
      ;;
  esac
  local c
  for c in "${candidates[@]}"; do
    if [ -f "$c/s7.c" ]; then echo "$c"; return 0; fi
  done
  return 1
}

S7_DIR="$(find_s7_dir)" || {
  echo "[build] 找不到 s7 原始碼（s7.c/s7.h）。" >&2
  echo "  試過的路徑都沒有 s7.c；請先去 s7-playground repo 跑 setup.sh 抓回原始碼：" >&2
  echo "    cd <s7-playground repo> && bash setup.sh" >&2
  echo "  （Linux 預設找 \$HOME/repo/pas/derived/s7-playground；" >&2
  echo "   Windows 預設找 C:/code/mine/pas/derived/s7-playground）" >&2
  echo "  或用環境變數指定： S7_DIR=/path/to/s7-playground ./build.sh" >&2
  exit 1
}
echo "[build] 用 s7 原始碼： $S7_DIR"

# ── 平台偵測：Windows 需 MinGW + -municode（wmain 寬字元）＋ shim_include（補 MinGW 缺的
#    <sys/utsname.h>）；Linux 原生 gcc，WITH_C_LOADER 預設開（FFI），需 -ldl 讓 dlopen 可用、
#    -Wl,-export-dynamic 讓現編的 *_s7.so 能連回 s7 符號。shim_include 絕不可進 Linux include
#    path —— Linux 有真正的 <sys/utsname.h>，shim 版會蓋掉它、且其假 uname() 反而更糟。
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    export PATH="/c/dev/mingw64/bin:$PATH"
    EXTRA_FLAGS="-municode"
    EXTRA_INCLUDES="-I ./shim_include"
    EXTRA_LIBS=""
    ;;
  *)
    EXTRA_FLAGS=""
    EXTRA_INCLUDES=""
    EXTRA_LIBS="-ldl -Wl,-export-dynamic"
    ;;
esac

echo "[build] s7host.exe"
gcc s7host.c "$S7_DIR/s7.c" -o s7host.exe \
    -I "$S7_DIR" $EXTRA_INCLUDES \
    -O2 -lm $EXTRA_FLAGS $EXTRA_LIBS
echo "[build] s7host.exe 完成"
