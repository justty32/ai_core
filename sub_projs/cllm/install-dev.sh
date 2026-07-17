#!/usr/bin/env bash
# install-dev.sh — 把 cllm 裝成「常駐開發環境」到一個 prefix（預設 ~/dev），變成可
# include/link 的東西（像 /usr/{include,lib,bin}），並搭好各語言環境：
#   C / C++（pkg-config cllm，JSON 用 jansson）、Lua 5.4+5.5（llm.so，JSON 用 dkjson）、
#   Fennel（用 5.4 的 llm.so + dkjson）、s7 Scheme（llm-s7 REPL，JSON 用 jq）、
#   Python（ctypes llm.py，JSON 用 stdlib）、Janet（native C 模組 llm.so，JSON 用 spork/json）、
#   Common Lisp（CFFI cllm.lisp，JSON 用 shasht）、Shell（llm CLI + jq）。
# 一鍵可重現：重跑即冪等覆蓋重建 cllm 那份。⚠ 別 rm -rf $PREFIX——prefix（~/dev）可能還住著
# 別的東西；要清 cllm 的部分只刪：include/cllm include/glaze lib/libcllm* lib/pkgconfig/cllm.pc
# lib/lua lib/python lib/janet lib/lisp lib/go bin/llm bin/llm-s7 share/cllm share/lua cllm/env.sh。
#
# 用：bash install-dev.sh
#   PREFIX=/somewhere   安裝前綴（預設 ~/dev）
#   LUA55_INC/LUA54_INC lua 頭檔目錄（預設 /usr/include、/usr/include/lua5.4）
# s7 原始碼已 vendor 在 repo（bindings/s7/vendor/，比照 lua 的 vendor/dkjson.lua）——零外部依賴。
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${PREFIX:-$HOME/dev}"
LUA55_INC="${LUA55_INC:-/usr/include}"
LUA54_INC="${LUA54_INC:-/usr/include/lua5.4}"

echo "== [1/7] 建置＋安裝核心（libcllm.so／llm CLI／headers／pkgconfig）→ $PREFIX =="
cmake --preset linux-debug -DCMAKE_INSTALL_PREFIX="$PREFIX" >/dev/null
cmake --build --preset linux-debug >/dev/null
cmake --install build >/dev/null
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
CF="$(pkg-config --cflags cllm)"; LF="$(pkg-config --libs cllm)"

echo "== [1b] C++ 便利層（llm.hpp／llm_reflect.hpp）＋ glaze 標頭 =="
cp "$HERE/bindings/cpp/llm.hpp" "$HERE/bindings/cpp/llm_reflect.hpp" "$PREFIX/include/cllm/"
GLAZE_INC="$(find "$HERE/build/vcpkg_installed" -maxdepth 3 -type d -name glaze -path '*/include/*' | head -1)"
if [ -n "$GLAZE_INC" ]; then
  rm -rf "$PREFIX/include/glaze"
  cp -r "$GLAZE_INC" "$PREFIX/include/glaze"   # header-only；llm_reflect.hpp 的反射糖要它
else
  echo "  ⚠ 找不到 vcpkg 的 glaze 標頭 → llm_reflect.hpp 需自備 -I"
fi

echo "== [2/7] Lua 模組（5.5 給 lua、5.4 給 fennel；模組 ABI 綁 lua 版本）＋ dkjson =="
mkdir -p "$PREFIX/lib/lua/5.4" "$PREFIX/lib/lua/5.5" "$PREFIX/share/lua"
gcc -O2 -fPIC -shared -I"$LUA55_INC" $CF "$HERE/bindings/lua/llm.c" $LF -o "$PREFIX/lib/lua/5.5/llm.so"
gcc -O2 -fPIC -shared -I"$LUA54_INC" $CF "$HERE/bindings/lua/llm.c" $LF -o "$PREFIX/lib/lua/5.4/llm.so"
cp "$HERE/bindings/lua/vendor/dkjson.lua" "$PREFIX/share/lua/dkjson.lua"   # 純 lua，5.4/5.5/fennel 共用

echo "== [3/7] s7 host（llm-s7，s7.c 編進去、self-contained；s7 原始碼在 repo vendor）=="
S7_DIR="$HERE/bindings/s7/vendor"
gcc -O2 -I"$S7_DIR" $CF "$HERE/bindings/s7/host.c" "$HERE/bindings/s7/llm_s7.c" "$S7_DIR/s7.c" \
  $LF -lm -ldl -o "$PREFIX/bin/llm-s7"

echo "== [4/7] Python（ctypes llm.py）＋ Common Lisp（CFFI cllm.lisp）=="
mkdir -p "$PREFIX/lib/python" "$PREFIX/lib/lisp"
cp "$HERE/bindings/python/llm.py" "$PREFIX/lib/python/llm.py"
cp "$HERE/bindings/lisp/cllm.lisp" "$PREFIX/lib/lisp/cllm.lisp"

echo "== [4b] Go 模組（cgo；整個 module 複製，含 example）=="
if command -v go >/dev/null 2>&1; then
  rm -rf "$PREFIX/lib/go/cllm"
  mkdir -p "$PREFIX/lib/go"
  cp -r "$HERE/bindings/go" "$PREFIX/lib/go/cllm"
  ( cd "$PREFIX/lib/go/cllm" && go build ./... ) && echo "  → lib/go/cllm（go build 過）"
else
  echo "  ⚠ 無 go → 跳過（裝了 go 後重跑）"
fi

echo "== [4c] Janet native 模組（llm.so；(import llm) → (llm/ask …)）=="
JANET_SYS=""   # janet 預設 tree（spork 等在此）；併進 JANET_PATH 讓 example 的 spork/json 仍可載
if command -v janet >/dev/null 2>&1; then
  JANET_INC="${JANET_INC:-}"
  if [ -z "$JANET_INC" ]; then
    for d in "$HOME/.local/include/janet" "$HOME/.local/include" /usr/include/janet /usr/local/include/janet; do
      [ -f "$d/janet.h" ] && JANET_INC="$d" && break
    done
  fi
  if [ -n "$JANET_INC" ] && [ -f "$JANET_INC/janet.h" ]; then
    mkdir -p "$PREFIX/lib/janet"
    gcc -O2 -fPIC -shared -I"$JANET_INC" $CF "$HERE/bindings/janet/llm.c" $LF -o "$PREFIX/lib/janet/llm.so"
    JANET_SYS="$(janet -e '(prin (dyn :syspath))' 2>/dev/null || true)"
    echo "  → lib/janet/llm.so（janet 頭檔：$JANET_INC；預設 tree：${JANET_SYS:-未偵測}）"
  else
    echo "  ⚠ 找不到 janet.h（設 JANET_INC 指到含 janet.h 的目錄後重跑）→ 跳過 Janet"
  fi
else
  echo "  ⚠ 無 janet → 跳過（裝了 janet 後重跑）"
fi

echo "== [5/7] examples + 離線 fixtures =="
mkdir -p "$PREFIX/share/cllm/examples"
rm -rf "$PREFIX/share/cllm/fixtures"
cp -r "$HERE/test/fixtures" "$PREFIX/share/cllm/fixtures"
for d in c cpp lua fennel s7 python lisp janet shell; do
  mkdir -p "$PREFIX/share/cllm/examples/$d"
  cp "$HERE/bindings/$d/example."* "$PREFIX/share/cllm/examples/$d/" 2>/dev/null || true
done

echo "== [6/7] env.sh =="
mkdir -p "$PREFIX/cllm"
cat > "$PREFIX/cllm/env.sh" <<EOF
#!/usr/bin/env sh
# cllm 常駐開發環境。用： source "$PREFIX/cllm/env.sh"
DEV="$PREFIX"
export PATH="\$DEV/bin:\$PATH"                              # llm、llm-s7
export PKG_CONFIG_PATH="\$DEV/lib/pkgconfig:\${PKG_CONFIG_PATH:-}"  # C/C++：pkg-config cllm
export LD_LIBRARY_PATH="\$DEV/lib:\${LD_LIBRARY_PATH:-}"    # 保險（.pc 已帶 rpath）
export LIBCLLM="\$DEV/lib/libcllm.so"                      # Python ctypes / CL CFFI 定位 .so
export PYTHONPATH="\$DEV/lib/python:\${PYTHONPATH:-}"       # import llm
export CLLM_LISP="\$DEV/lib/lisp/cllm.lisp"                # (load CLLM_LISP) 用 cllm:ask
export LUA_CPATH_5_5="\$DEV/lib/lua/5.5/?.so;;"            # lua（5.5）require "llm"
export LUA_CPATH_5_4="\$DEV/lib/lua/5.4/?.so;;"            # lua5.4 / fennel require "llm"
export LUA_PATH_5_5="\$DEV/share/lua/?.lua;;"              # lua（5.5）require "dkjson"
export LUA_PATH_5_4="\$DEV/share/lua/?.lua;;"              # lua5.4 / fennel require "dkjson"
export JANET_PATH="\$DEV/lib/janet${JANET_SYS:+:$JANET_SYS}"  # janet (import llm)；併預設 tree 供 spork/json
export CLLM_FIXTURES="file://\$DEV/share/cllm/fixtures/"   # 離線 fixture 基底（自測用）
EOF

echo "== [7/7] share/cllm/README.md =="
cat > "$PREFIX/share/cllm/README.md" <<'EOF'
# cllm 常駐開發環境（~/dev）

`libcllm.so`（對外 C ABI，唯一入口 `llm_ask`）裝成可 include/link 的常駐前綴。
由 `<cllm repo>/install-dev.sh` 產生、可重現（重跑即冪等覆蓋；⚠ 勿 rm -rf 整個 prefix，裡面可能還有別的東西）。

先 `source ~/dev/cllm/env.sh`。離線自測用 `$CLLM_FIXTURES`（假回應，不必開後端）；
接真後端則省略 endpoint（走內建 `http://localhost:1234/v1/chat/completions`）。

| 語言 | 怎麼用（＋JSON 庫） | 範例 |
|------|--------|------|
| C    | `cc x.c $(pkg-config --cflags --libs cllm jansson)`；`#include <cllm/cabi.h>`；JSON=jansson | examples/c |
| C++  | `g++ -std=c++23 x.cpp $(pkg-config --cflags --libs cllm)`；`<cllm/llm.hpp>`（便利層 `llm::`）＋`<cllm/llm_reflect.hpp>`（`ask_as<T>` 反射糖）；底層薄鏡像 `<cllm/cabi.hpp>`（`llm::abi`）；JSON=glaze | examples/cpp |
| Lua  | `require("llm")`（`lua`＝5.5、`lua5.4`＝5.4）；JSON=`require("dkjson")` | examples/lua |
| Fennel | `(require :llm)`（跑在 lua 5.4）；JSON=`(require :dkjson)` | examples/fennel |
| s7   | `llm-s7 script.scm`（`llm-ask` 已內建）；JSON=jq（shell-out） | examples/s7 |
| Python | `import llm`（純 ctypes）；JSON=stdlib `json` | examples/python |
| Janet | `(import llm)`（native C 模組）；JSON=`spork/json` | examples/janet |
| Common Lisp | `(load (uiop:getenv "CLLM_LISP"))`→`cllm:ask`；JSON=`shasht`（quicklisp） | examples/lisp |
| Go | `import "cllm"`（cgo；`replace cllm => lib/go/cllm`）；JSON=stdlib `encoding/json` | lib/go/cllm/example |
| Shell | `llm 你好`（unix filter；`llm --help`）；JSON=`jq` | examples/shell |

所有語言 API 一致：`ask(prompt[, endpoint], …)` + `on_delta` 串流回呼，回完整答案字串。
每個 example 都示範：基本 ask、串流、schema→JSON 解析、tools＋on_tool、media 輸出（on_media）、
media 輸入＋modalities、以及從該語言 shell-out 呼叫 `llm` CLI。
離線自測：`bash <cllm repo>/test/bindings_smoke.sh`（九語言一鍵）。
EOF

echo ""
echo "✅ 完成。下一步：source \"$PREFIX/cllm/env.sh\""
echo "   自測（離線，逐語言）：見 $PREFIX/share/cllm/examples/<lang>/"
