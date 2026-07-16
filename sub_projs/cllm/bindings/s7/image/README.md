# s7 映像：產出 · 利用（CLI/lib）· 執行期修改

← [bindings](../../README.md)｜需先 `source ~/repo/dev/env.sh`（並 `export S7_DIR=…`）

**s7 沒有 SBCL 那種記憶體映像 dump**（存不了活著的堆疊）。它的「image」＝**AOT 編成自帶直譯器的獨立 C 執行檔**——s7.c ＋ 綁定 ＋你的 scheme，全編進一個 binary。三件事各有做法，都實測過。

## 1. 產出映像（＝烤成獨立執行檔）

把你的 scheme 邏輯以字串 `s7_eval_c_string` 烤進 host，和 s7.c 一起編（見 [baked.c](baked.c)）：
```bash
source ~/repo/dev/env.sh
export S7_DIR=/path/to/s7           # 含 s7.c/s7.h
gcc -O2 baked.c ../llm_s7.c "$S7_DIR/s7.c" -I"$S7_DIR" \
    $(pkg-config --cflags --libs cllm) -lm -ldl -o cllm-s7-image
```
（本機 ext4 實測編過、`./cllm-s7-image 你好 <endpoint>` 跑出答案。）

> 更輕的做法：直接用 `install-dev.sh` 已裝好的 `llm-s7`（本身就是 s7.c＋綁定＋libcllm 的自帶映像），把你的 `.scm` 當參數餵它——不必自己 bake。bake 只在你要「單一自帶邏輯的執行檔」時才需要。

## 2. 利用映像

**當 CLI**：`./cllm-s7-image 你好 <endpoint>`（或 `llm-s7 script.scm …`）。
**當 lib**：s7 本身就是 C 庫——把 `s7.c ＋ llm_s7.c` 嵌進你的 C 程式、呼叫 `llm_s7_init(sc)`，`llm-ask` 就進了你的直譯器（`baked.c` 的 `main()` 正是這個模式）。要暴露更多 API 就在 `llm_s7.c` 加 `s7_define_function`。

## 3. 執行期修改程式

```bash
llm-s7 runtime-modify.scm
```
[runtime-modify.scm](runtime-modify.scm)：早定義的 `caller`，`set!` 換掉 `greet` 後**立即走新版**（同 Lisp 的重定義傳播）。

**互動玩法**：`llm-s7` 不給腳本參數即進**活的 Scheme REPL**，貼新 `(define …)` 當場重定義；嵌入端用 `s7_eval_c_string` 在執行期灌新碼（遊戲/服務常這樣熱更腳本邏輯）。

> ⚠ **取捨**：s7 的執行期修改是活的，但**改動無法存成映像**（無 dump）——要「固化」就重編執行檔。對照 SBCL 能 `save-lisp-and-die` 把跑著改出來的狀態存成新映像（見 [../../lisp/image/](../../lisp/image/README.md)）。
