# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **gen_v1 開工：事實/敘事風格分離**（2026-07-17 開）：設計定調落 [gen_v1/00_設計.md](gen_v1/00_設計.md)（四柱：facts/act/realizer 分層、知識視圖、LOD＋canon 鎖、分層網＋查詢介面六動詞），**事實庫玩具版已三語言並排落地**（[gen_v1/](gen_v1/README.md) Lua／[gen_v1_f/](gen_v1_f/README.md) Fennel／[gen_v1_l/](gen_v1_l/README.md) Common Lisp・SBCL——九示範逐條對齊、全 assert 綠、重跑逐位元一致；Fennel「放棄」定調已在 AGENTS 翻案）。open（依序）：①三線比較收斂——使用者挑主力語言後另兩線定去留；②director——gen_v0 引擎升格成「搜尋 act 序列」、經 speculate 咬合 facts；③realizer（敘事風格層）——**已有跑通的玩具原型，Fennel／Common Lisp 雙版**：[realizer_f/](realizer_f/README.md)（Fennel＋macro）＋[realizer_l/](realizer_l/README.md)（**CL 壓縮版，單檔 109 行 vs Fennel 六檔 224**，使用者定調「壓縮帶來智能」——filler 兩表併一、scene 塌成扁平 item 序、抽 say）。核心：`tmpl` macro 把句子模板當 list（字串=字面、符號=槽）**編譯期折成字串接合**＝同像性「模板即程式」；`G(母題,風格座標)→整場河堤台詞`，固定旁白框＋多句子模板＋兩條護欄，六 demo 逐位元同、全綠；驗證入 [testing](workflows/testing.md)。兩版並存也是[三線收斂](WAIT_USER.md)的對照點。**CL 版已接 director 搜索層**（[director.lisp](realizer_l/director.lisp)）：不再顯式給風格座標，改窮舉模板×填充＋成本（prior＋intent＋history）挑最優；`intent`（甜/靜）搜出不同風格、`direct-n` 每輪 history 逼換措辭**自動生 N 種台詞流＝閉環最初手寫「20 種」**（九 demo 全綠）。註：此為 realizer 側「風格搜尋」director，與本設計上游「搜 act 序列」的 director 同名異層——後者仍待 gen_v1 動工（見開頭 gen_v1 open ②）。其手工前身兩份：[台詞流_場景二_河堤_20種](corpus/劇本/最後的櫻花雪/台詞流_場景二_河堤_20種.md)（固定事實層→20 種實現，逼出 realizer 三軸＝表達度/回應風格/母題載體）＋[模板_場景二_河堤_八股拆解](corpus/劇本/最後的櫻花雪/模板_場景二_河堤_八股拆解.md)（八股法：固定旁白框、切五段 beat、逐段多生台詞、段 D 做成 slot 模板；§七 示範**由下而上 induce**——先生變體→對齊→抽公因式產模板，逼出 top-down 漏掉的「變化動詞」槽與「認可」可空槽＝realizer 該從語料 induce 而非憑空寫；已對映 gen 的 beat 文法/句庫/槽填充/requires_pick，動 realizer 時可直接把第四節灌 `library[段]`、第五節灌 slot 表）＋[句子模板庫_場景二_河堤](corpus/劇本/最後的櫻花雪/句子模板庫_場景二_河堤.md)（把 induce 法推到整場五段每個台詞槽，每槽 2–3 種結構相異模板＋填充物＝realizer 完整候選表；共用「變動母題軸」＋三承接約束＝requires_pick，可直接 `G(母題+風格座標)→整場台詞`）；④已知簡化清單見 [gen_v1/README](gen_v1/README.md)。

- **統一：gen_v0 引擎 ≅ realizer director（一台搜尋機）＋uniform_probe 五路探索（in-flight）**（2026-07-17）：分析落 [一台搜尋機.md](一台搜尋機.md)——兩者都是「枚舉離散空間→護欄剪枝→成本(prior＋history＋intent)排序→取最小→history 迭代多樣」，唯一差別是成本耦合度（gen_v0 全場預算耦合→全域 DFS；director 母題解耦→逐槽獨立），**同一台機器兩種耦合**。通往使用者洞見「八股/段落/句式/填充物/原子事實只是不同粒度、同一種節點」→「一個 search kernel 遞迴跑每層粒度」。**五路探索已齊＋綜合完成**（[uniform_probe/](uniform_probe/README.md)）：a1–a5 各自獨立 kernel、全綠；五個關鍵發現（種≠粒度／分支=普通節點且結構是DAG／事實vs句式=兩個域/真值?分域／填充物帶邏輯→句式界線溶解／層由refs湧現+懸空=佔位）。**綜合出最小統一 kernel** [synth/synth.lisp](uniform_probe/synth/synth.lisp)（102 行，核心~30：一表＋`node`/`fact` 兩 macro＋part 折疊 `字面/引用/(? env選)/(! 邏輯)`＋湧現粒度，六 demo 全綠；省掉 realizer_l 的四表四接縫）。**表示×演算法合一**：synth 的 `*env*` 就是搜尋維度，director 的 best-line＝搜 `*env*` 最小成本 →「一棵統一節點樹＋一個搜 env 的 search kernel，遞迴跑每層粒度」。**待使用者裁**：是否扶正 synth kernel 取代四表／抽共用 search kernel／域界線+靜態護欄進 kernel。

- **corpus 定點細固化實驗進行中**（2026-07-17 開）：[固化/定點_盛夏黃昏河鹿堂/](corpus/固化/定點_盛夏黃昏河鹿堂/README.md) 一個場景鑿到底，問題集在 [90](corpus/固化/定點_盛夏黃昏河鹿堂/90_問題與討論.md)。**使用者已定調終點形態**（純程式執行、品質優先延遲不設限、真源=規則表=快取、代價模型、LLM 移編譯期——落 [固化/README「終點形態」](corpus/固化/README.md)）。丙-1 第一輪已收（[定點_陽溜亭_初雪傍晚/](corpus/固化/定點_陽溜亭_初雪傍晚/01_驗證報告.md)：H1–H8 無一整條推翻、三條上抽一層後更通用，canon 抽驗通過）。**gen_v0 已落地且引擎升 v1**（[gen_v0/](gen_v0/README.md)：Lua、條件全 lua table；L1 座標搜尋＋L2 beat 產生式＋全場窮舉＋intent 層＋requires_pick 承接鏈，八示範全 assert 綠；驗證指令入 [testing](workflows/testing.md)）。**丁-4 機制回寫已完成**（H1–H8 定稿落河鹿堂 01/02/03/05＋上層 02/04/05）。open（下一棒，互不阻塞）：①**表從 corpus 定稿生成**（gen 的 tables.lua 目前是手抄快取）＋交集程序寫成真程式（丁-1 殘餘）；②丙-2 統計 pass（掃 34 劇本＋52 場景校準門檻/預算/定價）；③丙-1「他主場」第三定點；④句庫擴充的編譯期流程（LLM 炸候選→人審→入庫）。討論題見 [90](corpus/固化/定點_盛夏黃昏河鹿堂/90_問題與討論.md)（無★項）。

- **LLM 接口探索三線（try_1 s7／try_2 Lua／try_4 三線整合）已退出並移除工作樹**（先封存於 commit `b89c042`、再於 2026-07-16 依使用者指示刪除；**git 歷史仍完整、要撈得回來**）。三線的 durable 細節（設計決策／踩坑／跨平台驗證）留在 [common/gotchas](workflows/common/gotchas.md) ＋ git log。原 try_1／try_4 的 open 待辦（`*llm-schema*` 生 `--flag` CLI 薄殼、三擴充接串流、腳本側表描述生 schema 等）一併擱置，不再列為活狀態。

- **try_3（純 C++ 線）已收斂成兩交付物、抽離成獨立 sub_proj [cllm](../cllm/README.md)**（2026-07-16，`git mv galtxt/try_3 → sub_projs/cllm`；lib 名 `libgaltxt`→`libcllm` 隨改，README／INDEX 同步）：對外 C ABI `libcllm.so`（唯一入口 `llm_ask`）＋`llm` unix filter CLI，離線 17/17 綠、Windows／Manjaro 雙機皆驗。舊 L0（`llm::Client` ask＋三擴充）封存於 `cllm/archived/`。**架構/決策/踩坑細節全在 [cllm/README](../cllm/README.md)＋[common/gotchas](workflows/common/gotchas.md)＋git log**。
  - **open 待辦（跟著 cllm 走）**：反射生成 CLI 目前是**參考版**，**使用者將自行重寫一遍（加深記憶）**——重寫定案後此條再更新。

> 註：舒適 CL 環境已另立為兄弟子專案 [`sub_projs/comfy/`](../comfy/README.md)（與原 s7 線並存、兩邊都推），其活狀態記在該處與頂層 [SESSION-LOG](../../SESSION-LOG.md)，不在 galtxt hub 裡。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
