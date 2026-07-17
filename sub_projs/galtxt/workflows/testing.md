# testing — 跑測試 / 驗證（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

> 本檔是「單檔工作流」，膨脹了就照 [DEV-GUIDE](../DEV-GUIDE.md) 升級成資料夾型。

## 指令

- **快速驗證（Claude 自己跑、鐵律要求的那套）**：
  - `cd gen_v0 && lua main.lua`——示範全數自帶 assert（含確定性逐位元自檢），跑完印「全部示範完成」即綠。
  - `cd gen_v1 && lua main.lua`——事實庫九示範全數自帶 assert，跑完印「九個示範全部通過」即綠。
  - `cd gen_v1_f && fennel main.fnl`——同上的 Fennel 版（Fennel 1.6+）。
  - `cd gen_v1_l && sbcl --script main.lisp`——同上的 Common Lisp 版（SBCL）。
  - `cd realizer_f && fennel main.fnl`——八股 realizer 玩具版（Fennel＋macro），六 demo 全自帶 assert，跑完印「八股 realizer（Fennel＋macro）立住了」即綠。
  - `cd realizer_l && sbcl --script main.lisp`——Common Lisp 壓縮版＋director 搜索層（realizer 90＋director 48＋main 39），九 demo 全自帶 assert（含 director 搜最優／intent 甜vs靜／direct-n 自動生 3 種台詞流）。
  - `cd uni && sbcl --script main.lisp`——**統一節點＋一支參數化 `search*`**（kernel/scene/search_kernel/search/main 五檔各≤62 行）：一種節點跑所有粒度＋`search*` 可分(director)/耦合(全場預算)皆跑，十 demo 全自帶 assert。
  - `cd uniform_probe/synth && sbcl --script synth.lisp`／各 `uniform_probe/aN/`——統一節點五路探索＋綜合（bench，各自帶 assert）。
- **完整驗證**：全部都跑。

## 測試分類

有「部分測試需要特殊環境」（本機 LLM daemon、外部模型服務、實機）時在此寫清楚分類與各環境能跑哪些——這是「離線也能開發」的關鍵。跑不了的環境依賴驗證 → 記 [WAIT_USER](../WAIT_USER.md)。

- （待補）
