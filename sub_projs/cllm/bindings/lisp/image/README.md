# Common Lisp 映像：產出 · 利用（CLI/lib）· 執行期修改

← [bindings](../../README.md)｜需先 `source ~/dev/cllm/env.sh`

把 cllm 綁定＋你的碼「烤」進 SBCL **映像**（image）——一個含堆疊狀態的可重載/可執行檔。三件事都在本夾實測過。

## 1. 產出映像

```bash
source ~/dev/cllm/env.sh
sbcl --script save-image.lisp exe    # → ./cllm-image（可執行映像）
sbcl --script save-image.lisp core   # → cllm.core（core 映像，當 lib 基底）
```
CFFI 用 `use-foreign-library` 註冊 libcllm，**SBCL 重啟映像時自動重載 .so**——故烤進去後 `cllm:ask` 開箱即用，不必再 quickload/load。

## 2. 利用映像

**當 CLI（可執行映像）**：
```bash
./cllm-image 你好 "$CLLM_FIXTURES/fake/chat/completions"   # 離線
./cllm-image 用一句話自我介紹                              # 走內建 localhost（真後端）
```

**當 lib（core 映像，快啟動基底）**：
```bash
sbcl --core cllm.core --noinform --non-interactive \
  --eval '(format t "~a~%" (cllm:ask "你好" "…/fake/chat/completions"))'
# 或把 cllm.core 當你所有 cllm 腳本的基底，省掉每次載綁定的成本
```

**為什麼要映像＝啟動快**（本機實測）：

| 方式 | 啟動到出答案 |
|------|-------------|
| 冷載（每次 `quickload cffi` + `load cllm.lisp`）| ~268 ms |
| 可執行映像 `./cllm-image` | **~5 ms** |
| core 映像 `sbcl --core` | **~4 ms** |

## 3. 執行期修改程式（活映像精髓）

```bash
sbcl --script runtime-modify.lisp
```
示範兩種「跑著就改、不重啟」（[runtime-modify.lisp](runtime-modify.lisp)）：
- **① 重定義傳播**：早已編好的 `caller`，重定義 `greet` 後立即走新版——呼叫綁的是「符號」而非當時的函式值，故換定義即全域生效（連舊碼也跟著換）。
- **② 熱換**：背景執行緒跑著，中途 `setf` 換掉它呼叫的行為，輸出從 v1 tick 切成 v2 tick，全程不重啟。

**真正的互動玩法**（本檔的靜態腳本只是證明機制）：從 **nvim**（conjure／vlime）或 **emacs**（SLIME／SLY）經 **SWANK/SLYNK** 連進「正在跑的映像」，即時 `(defun …)` 重定義任何函式、當場看效果；迭代到滿意後 `(save-lisp-and-die)` 把新版**固化成新映像**。這就是「改程式 = 改一個活著的系統」而非「改檔案再重啟」。

> 一句話：SBCL 三面俱全——映像能**存**（save-lisp-and-die）、能**當 exe/lib 用**、能**跑著改**。s7 與 Fennel 各有取捨，見 [../../s7/image/](../../s7/image/README.md)、[../../fennel/image/](../../fennel/image/README.md)。
