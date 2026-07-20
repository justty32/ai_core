# 〈A Road to Lisp: Why Lisp〉摘要

## 一句話

一篇「為什麼值得學 Lisp」的入門宣言：作者 Elia Scotto 主張 Lisp 真正的價值不在任一單點功能，而在 **macro（可擴充語言本身）＋ homoiconicity（碼即資料）＋ REPL 活系統開發** 三者合起來，會改變你思考問題的方式——就算你永遠不拿它出貨。HN 討論串（298 分、311 則）反而比原文更值得讀：它把這種宣言該有的**冷靜反面**都補齊了。

## 來源

- 原文：Elia Scotto，〈A Road to Lisp: Why Lisp〉，2026-07-09，<https://scotto.me/blog/2026-07-09-why-lisp/>（系列首篇；主要以 Common Lisp 舉例，`defun`/`defmacro`/`loop`）
- 討論：Hacker News item 48845209（submitter `silcoon`，即作者本人；298 分、311 則、27 條頂層）<https://news.ycombinator.com/item?id=48845209>
- 抽取方式：原文經 WebFetch 讀取；HN 留言樹經 Algolia API（`https://hn.algolia.com/api/v1/items/48845209`）完整取回 311 節點後解碼。原文逐字引用若要落到對外文案，發布前再核一次原文用字。

## 原文重點（依序）

1. **macro＝可程式化的程式語言**：C／Rust 的 macro 多半在消樣板；Lisp 的 macro 讓你**發明新的語言構件**。文中示範自己造一個 CL 沒有的 `while`。
2. **homoiconicity（碼即資料）**：程式和資料都是 list，所以程式能操作程式（「會寫程式的程式」）——別的語言追了幾十年的能力。
3. **活系統開發（REPL-driven）**：把編輯器接到**跑著的 image**，重定義一個函式立刻生效，hot-reload 是內建而非外掛工具。
4. **可擴充的軟體**：用 macro 造的內部 DSL 可以開放給終端使用者（舉 CMS、數學繪圖為例）。

**可查核的具體宣稱**（大致成立）：Lisp 是「Fortran 之後仍在用的第二老語言」（Fortran 1957 / Lisp 1958）；AutoCAD 用 AutoLISP；Emacs（Emacs Lisp）被擴成 PDF 閱讀器／郵件／音樂播放器／視窗管理器（pdf-tools、mu4e/Gnus、EMMS、EXWM）。引用 Stallman「最強的語言是 Lisp」、以爵士樂類比（Armstrong「要問什麼是爵士，你就永遠不會懂」）、以及 Paul Graham 的 **Blub 悖論**解釋「為何程式員感受不到 Lisp 的好」。

## 討論串精華（比原文更值得看的部分）

分五條主線，我按「對我們這個 CL lab 有沒有用」排序：

1. **macro 到底特別在哪**（`sroerick` 誠實發問，28 則子樹）——最好的教學串。差別不是「編譯期生成碼」而是「**生成碼的碼**」，走訪並改寫 AST；關鍵在 homoiconicity，macro 的輸入就是普通 Lisp 的 list，所以幾乎不用學新技能（相較之下「Rust macro 是黑魔法」）。`_ph_` 的一句話最乾淨：**「macro 就是在編譯期執行的函式」**，拿到的是**未求值的字面引數**。`BeetleB` 給了經典例子：你**沒辦法**用函式自造 `if`（兩個分支都會先被求值），macro 才能控制求值時機。新手指路：Paul Graham《On Lisp》第 7–8 章。
2. **REPL／hot-reload 不是 Lisp 專利**（`zbentley` 起頭，46 則，串裡最大的爭吵）——重要的反面：REPL 是直譯語言的標配，hot-reload 也非獨有且未必常用。Lisper 的反駁是「我們講的 REPL 是把編輯器連上活行程、在情境中求值」。串的技術核心結論：**好的活體驗來自工具鏈（debugger、CLOS `change-class` 熱改類別、重編譯），不全來自 homoiconicity**——證據是 **Smalltalk 並非 homoiconic，活體驗卻同等甚至更好**。（此串末端淪為人身攻擊，只取技術部分。）
3. **Lisp 冷靜的批評在哪**（`abetusk`，32 則）——最該收藏的一串：
   - `coffeemug`：兩個真實缺點——太自由，沒紀律的人會沉迷「好玩的 Lisp」而不出貨；「它吃掉有能力的腦袋好幾年，產出卻配不上投入」。
   - `lenkite`／`Jtsummers`：**CL 標準自 1995 沒再更新**，沒有標準化的並行／async 模型，也沒長出泛型/可擴充序列（Clojure、Julia 在此超車）。反駁：社群事實標準的並行庫 bordeaux-threads、lparallel（見 awesome-cl）。
   - `pjc50` 名言：Lisp 是「程式語言界的 Stockhausen……一個粉絲圈，異常長壽，但從沒真正紅出去」。
4. **Lisp × LLM**（`malkia` 問「拿 Lisp 跟 LLM 迭代是不是更順」）——很當下的一條：
   - 括號平衡曾是模型的痛，「那是去年的事」，Claude Code／Codex 現在處理得來；狂掉括號通常代表**撞到 context 上限**（開新 session）。
   - 反覆出現的強論點：**給 agent 一個真的 Lisp REPL**（slynk/nrepl），它就從「猜」轉成「探活狀態」，更快更省。有人描述 agent 透過 ClojureScript REPL 走訪 DOM、同時用 nREPL 監看 k8s。
   - 反面（`pjc50`）：「若你有 LLM 又不在乎程式碼本身，何必叫它寫 Lisp？」
5. **DSL：威力還是反模式**（`doug_durham`）——商用實務裡 DSL 常是反模式（有人造了沒文件的迷你語言就離職）。最好的 Lisp 專屬反駁（`martinflack`）：看不懂的 macro（如 `LOOP`）可以當場 **`macroexpand`** 成基本形式，抽象永遠不會是黑箱。

**幾則有記憶點的哲學留言**：`GMoromisato`（最高票）「光明面 vs 黑暗面」——光明面約束有缺陷的程式員（禁 goto、靜態型別），黑暗面給你力量（macro、運算子重載、自我修改）；Lisp 明明是黑暗面，卻連光明面的人也景仰，「純粹到同時體現兩者」。`doug_durham` 的冷水也值得記：「**語言從來不是答案……Lisp 沒有比 Visual Basic 高級，兩個都是圖靈完備**；語言是工具，像衝擊鑽 vs 旋轉鑽」。

## 對我（cl-lab）的用途

- **macro／conditions 是 CL 的兩塊靈魂**這個判斷，被討論串背書：最有價值的教學串正好都圍著 macro（碼即資料、控制求值、`macroexpand` 除魅）。可回頭強化 [docs/07-macro.md](../../docs/07-macro.md)，把「不能用函式造 `if`」「`macroexpand` 當場除魅 `LOOP`」這兩個具體例子收進去。
- **REPL 的賣點要講準**：別把「有 REPL」當賣點（那是標配），要講清楚 CL 版本的差異化——**接編輯器到活 image + CLOS 熱改類別/實例**。這正好對上本 repo 已有的 Swank/Conjure 設定（[docs/06-編輯器與-repl.md](../../docs/06-編輯器與-repl.md)），值得把「為什麼這比一般 REPL 強」補一句。
- **誠實的反面**（1995 後標準沒動、無標準並行、易沉迷不出貨）值得在教學裡據實提一筆，避免變成又一篇「propaganda」。並行的務實解：bordeaux-threads / lparallel。
- **Lisp × LLM「給 agent 真 REPL」**這條，跟 cl-lab 已有 swank 直接相關——是一個可玩的實驗方向（讓 agent 透過 Swank 探活狀態），可記成 idea。
- 延伸讀物線索：Paul Graham《On Lisp》(ch.7–8, 免費全文)、SICP、Coalton／Shen（CL 上的靜態型別）、awesome-cl、《The Bipolar Lisp Programmer》《The Lisp Curse》（批評面）。

## Open questions（待回訪）

- 作者預告下一篇會回答「怎麼開始學 Lisp」（留言 `silcoon`「Next week I will try to answer…」）——出了可續 digest。
- 「給 LLM agent 接 Swank REPL 去探活狀態」在 cl-lab 上實際做起來多順？值得一個小實驗（歸 learn / idea）。
