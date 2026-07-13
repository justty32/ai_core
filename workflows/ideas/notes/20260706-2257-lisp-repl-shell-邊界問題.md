# Lisp / REPL / Shell：邊界在哪裡，以及 Lisp Machine 為什麼死、我們為什麼不會

> 記於 2026-07-06。一場從「Lisp 能當 shell script 嗎」一路推到「准入制 image 沒有 Lisp Machine 死因」的討論。
> 是 [20260702 回到初心](20260702-2003-回到初心-llm-as-function.md) §4.2「理想語言是 Lisp」的直接延續。

## 一、Lisp 能當 shell script 嗎 → 能，瓶頸只有兩個

技術上直接可行：shebang（`#!/usr/bin/env -S sbcl --script`）之後就是普通 POSIX 程式（argv/stdin/stdout/exit code），完全符合「shell 為一等公民」的函數模型。真正的門檻：

1. **啟動延遲**（對 one-shot 模型是命門）：Janet ~ms、Chez/Gauche/Guile ~10-20ms、SBCL `--script` ~20-50ms → 可用；JVM Clojure 秒級 → 出局；**Babashka**（GraalVM 原生 binary、~10ms、為 shell scripting 而生）是 2020 年代的標準答案。
2. **process 編排人體工學**：bash 的不可替代性在「管線是語法」。已有解：scsh（1994 始祖，`(run (| (ls) (grep "foo")))`）、Rash（Racket shell 方言）、Babashka process、Guile popen。最大實戰案例 **GNU Guix / Shepherd**：整個發行版的套件管理與 init 是 Guile script。

runtime 可得性 vs least dependency 的緩解：選單一靜態 binary（Babashka / Janet / Chicken 編譯）。

## 二、REPL＝persistent 的特化實作（不是新東西，是已蓋過兩次的模式）

REPL 把啟動延遲歸零（spawn ~10ms → 送 S-expr ~μs）、warm state 跨呼叫存活。原型：`emacs --daemon` + `emacsclient -e`；nREPL / Swank / Guile `--listen` 同形。

對照本系統：軸 2 `persistent`、「重量級函式變 server」、LLM Entry Manager `--socket`、core_handy `impl/serve.hpp`——**REPL daemon 就是這個模式的 Lisp 版**，protocol 換成 S-expression 本身。最自然的落法：

```
外界（hub / 笨模型 / pipeline）── 仍是 text-in/text-out one-shot CLI
  → 瘦 client（如 emacsclient，~ms）
  → Unix socket
  → Lisp image daemon（常駐，內裝幾百個 tiny function）
```

對外守九軸契約的 shell 公民；對內是同 image 的符號。**順便解掉 SFC 的檔案爆炸問題**——image 就是天然的 function center。

新能力：**熱定義**。聰明模型生函數 → `eval` 進活 image → 立刻可呼叫可測試，不夠格 `unintern` 撤掉。image ＝ 可動態發照/吊照的函數註冊表，與固化飛輪一比一對應。Hub 掃描從「spawn N 個 process 問 --metadata」變一句 introspection。

代價（兩個真的痛）：
1. **image 漂移 vs 鐵則「repo 才是持久層」**（Smalltalk 百年病）。解法是紀律：image 永遠只是快取，任何 eval 進去的定義必同時落檔進 repo，daemon 隨時可殺、可從 repo 冷重建且結果一致。「熱定義」限定成「熱載入」。
2. **爆炸半徑**：放棄 Unix process 隔離。→ 見 §五。

張力：REPL 是有狀態介面，對笨模型更難（污染的 binding 禍及後續）。「REPL 給聰明模型探索期、one-shot 契約給笨模型消費期」剛好對上時間維度分工。

## 三、REPL 能在特定場景替代 shell 嗎 → 判準

捅破窗戶紙：**shell 本來就是 REPL**（read-eval-print 一個爛語言）。真問題是「什麼場景下 shell 的原語（process＋位元組流＋檔案）是錯的原語」。

shell 贏的唯一（但很大的）場景：**呼叫的多數是既有外部程式**＋零安裝普及性。

REPL 嚴格優於 shell 的四條件（命中越多越該換）：
1. 呼叫的多數是「自己人」（in-image 函數）——經 shell 是純稅。
2. 步驟間要傳結構化資料——shell 每個接縫打碎成位元組流，quoting 地獄由此生。
3. 需要 session 狀態——持久 binding vs 環境變數＋暫存檔。
4. 駕駛者不是人類打字——括號稅是對人手指收的；LLM 生 S-expr 無負擔。

已真實發生的先例：Jupyter 在資料領域把 shell 排擠掉；Guix 取代整個發行版的 shell script 層；Emacs 老手活在 eshell。

**銳利的自我觀察**：九軸＋`--metadata`＋entry `format`/`schema`（7/3），本質是在 shell 文字管線上重建「有型別的函數呼叫」語意——REPL 世界原生就有的東西。我們一直在把 shell 慢慢改造成 REPL。

邊界劃分結論：**greenfield 內圈（系統自產、守契約函數間的組合）→ REPL 可替代 shell；邊界與外圈（brownfield CLI 宇宙、跨機部署、人類、笨模型 one-shot）→ shell 留著**。與「brownfield 一律經 wrapper」同構。

判準一句話：**外部程式呼叫佔比高用 shell；接縫結構化資料需求高用 REPL**。系統演化方向（schema/format/固化）明顯往後者走——內圈遲早長成 REPL。

## 四、Lisp Machine 史鑑 ＋ Python＝dict-Lisp

### 4.1 Lisp OS 真的存在過
MIT CADR → Symbolics Genera / LMI / TI Explorer / Xerox Interlisp-D：微碼以上全是 Lisp，tagged architecture 硬體。現代活標本 Mezzano；務實後裔 **Guix System**（kernel 外包 Linux，userland 全 Guile）。

### 4.2 REPL 就是 shell，字面為真
Genera 沒有 shell，只有 **Listener**（REPL）。無 process 概念、單一位址空間、所有函數（含 OS 內部）可 `Meta-.` 當場改。＝「內圈 100% REPL 化」的整台機器。

**怎麼死的：死在邊界上**。零隔離（一個爛函數毀全機）、專用硬體貴敵不過 commodity＋Unix/C（〈Worse is Better〉）、AI 寒冬。死因清單與「shell 贏的場景」完全重合——隔離、普及、便宜。→ 內圈 REPL 化有完整先例；把邊界也 REPL 化的嘗試死過一次。

### 4.3 Python ＝ 更好懂的 Lisp（list → dict）
Norvig〈Python for Lisp Programmers〉：Python 有 Lisp 全部本質特性**除了 macro**。活證據 **Hy**（S-expr 一比一編譯到 Python AST）——證明 Python 語意骨架就是 Lisp 形狀。

「list 換 dict」比表面深：Lisp 萬能結構是 cons/list（程式碼也是它）；Python 萬能結構是 dict（object/module/class/kwargs 全是 dict）。**dict 世界的通用序列化＝ JSON**——`--metadata` 選 JSON，整個系統已站在 dict-Lisp 這邊。

「就這一個差別」漏掉的關鍵：**語法殺死同像性**——Python 程式碼不是資料，沒有真 macro、沒有「read 而不 eval 拿宣告」。

### 4.4 合流：本系統是什麼
- Lisp 失去的**同像性**，用 **JSON metadata** 補回 data 半邊：`--metadata`＝「不執行也能讀的宣告」≈ read S-expression。
- Lisp 失去的 **macro**，由 **LLM 頂替**：讀 metadata、生程式碼——**LLM 就是本系統的 macroexpander**（與 §十三「語意的 tracing JIT」同一件事的兩種說法）。
- Lisp Machine 死掉的**邊界**，用 shell/process 契約守住。
- 加一條硬理由：**便宜小模型訓練分布裡 Python >> Lisp 幾個數量級**——笨模型寫 Python 遠比寫 Lisp 可靠，對「笨模型消費者」前提幾乎決定性。規範層語言中立、Lisp 理想載體、Python 務實載體（7/2 定調），繞 Lisp Machine 一圈回來更站得住。

## 五、邊界問題能靠好的 Lisp lib 解嗎 → 一半可以，分界線本身有教育意義

三個死因分開驗屍：
- **硬體經濟**：已自動消失（歷史死因，非結構）。
- **普及性**：工具鏈解（單一靜態 binary），不用 lib 解。
- **隔離**：真問題。結構性事實——**隔離不是加上去的能力，是拿掉的能力**。lib 提供 capability（安全路徑）；隔離要求 constraint（全域保證無人共享狀態）＝全域性質，lib 管不到沒 import 它的程式碼。類比：不能用 library 給 C 加記憶體安全。
  - Lisp image 的具體失效模式（純 Lisp 記憶體安全，比 C 窄但仍在）：死迴圈 thread 殺不乾淨、失控配置撐爆共用 heap、global state 污染、FFI 一穿全毀——都不是 lib 層攔得住的。

兩個存在性證明——框架可以解，**條件是往下多要一層（runtime 承諾）**：
- **Erlang/OTP ＝ 活下來的 Lisp Machine**：BEAM 在一個 VM 內做到真隔離——shared-nothing（per-process heap、訊息複製、per-process GC）＋ supervision tree ＋ let-it-crash。是 runtime 層保證：非法狀態（共享記憶體）無法被表達。
- **Racket**（Lisp 家族最接近）：custodian（資源歸屬強制回收）、sandbox（記憶體/時間上限）、places（shared-nothing 平行）——成立因為 runtime 配合。
- 現代工業迴響：V8 isolates、WASM sandbox。

精確結論：**「好的 lib」不夠，「lib ＋ runtime 承諾」才夠**。CL runtime 沒這承諾故 CL lib 做不到；Erlang/Racket 做得到因為多要了一層。

### ★ ai_core 的轉折：准入制 image 改變了這道題
- **開放 image**（Lisp Machine：任何人隨時改任何東西）：紀律不可全域成立 → 隔離必須 runtime 強制 → lib 不夠。
- **准入制 image**（ai_core：函數進 image 先過憑證閘門〔必要性＋穩定性〕；九軸宣告〔stateful/guarantee/uncertainty〕＝函數對自己紀律的聲明，**准入時驗證**而非執行時攔截）：lib 提供安全路徑＋閘門保證大家都走這條路 → **lib 級「隔離 by construction」成立**。
- 殘餘風險（重的、FFI、uncertainty 高的）走既有逃生門：**踢出去當 OS process**（＝「重量級函式變 server」原則）。
- OTP let-it-crash ＝「笨模型 one-shot、搞砸就 retry」同一思想：不防禦錯誤，失敗單元死掉重來，supervisor（retry/guard）兜底。

> **一句話總結**：純 lib 解不了開放 image 的隔離（能力形式的極限）；但「lib ＋ 准入治理 ＋ OS 逃生門」可以——三樣本架構已全有。**我們不是要重建 Lisp Machine，是要建「只收持證函數的 Lisp Machine」——那個版本沒有死因。**

## 六、對後續工作的含義（待消化，非決定）

1. REPL daemon 可考慮定位成軸 2 persistent 的特化實作（守 serve 契約、對外仍 shell 公民），一次解啟動成本／SFC 檔案爆炸／hub 掃描三題。
2. 兩條紀律是前提：image 可從 repo 冷重建（鐵則）；重/不穩函數保留 process 隔離逃生門。
3. 語言選擇的層次更清楚了：契約層 JSON（dict-Lisp 半邊）、笨模型消費層 Python/shell（訓練分布＋one-shot 可靠性）、組合/探索層才是 Lisp 的候選地盤。
