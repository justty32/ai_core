# project.janet — 給 jpm / janet-lsp 認得的專案宣告（編輯器補全靠這份）。
# 一般不用 jpm 也能編：見 build.sh（直接 gcc）。用 jpm 時需先 source ~/dev/cllm/env.sh
# 讓 pkg-config 找得到 cllm，並補上 janet 頭檔的 include。
(declare-project
  :name "cllm-janet"
  :description "cllm 的 Janet binding（native C 模組，(import llm) → (llm/ask …)）")

# declare-native：把 llm.c 編成可 (import llm) 的原生模組。
# ⚠ 需要 cllm 的 cflags/libs——jpm 不吃 pkg-config，故這裡直接寫死常見旗標；
#   換前綴時改這裡，或直接用 build.sh（推薦，走 pkg-config）。
(declare-native
  :name "llm"
  :source ["llm.c"]
  :cflags [";default-cflags" "-I/home/lorkhan/dev/include/cllm"]
  :lflags ["-L/home/lorkhan/dev/lib" "-Wl,-rpath,/home/lorkhan/dev/lib" "-lcllm"])
