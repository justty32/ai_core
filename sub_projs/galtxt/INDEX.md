# INDEX — galtxt 專案地圖

整個專案的頂層導航。galtxt = **galgame 台詞生成的動手實驗場——把台詞產出鍛成「笨模型＋鷹架＋護欄」可靠管線，ai_core 北極星第一目標問題的落地執行處；依賴 [llm_forge/](../llm_forge/AGENTS.md)，跑通的機制才搬去固化**。AGENTS.md 只放主工作流 + 指向本檔；細節從這裡分流出去。

> 🧭 **回家接手先看** → [導覽.html](導覽.html)（本輪 session 地圖＋能跑什麼＋待你決定；瀏覽器開）。

---

## Repo 佈局

工作流骨架 ＋ **語料庫**（`corpus/`，進行中）＋ **第一片程式碼**（`gen_v0/`，確定性生成器玩具版）。LLM 接口探索三線（`try_1` s7／`try_2` Lua／`try_4` 三線整合）已探索完、退出現役維護鏈並**從工作樹移除**（durable 細節留 git log ＋ [common/gotchas](workflows/common/gotchas.md)）；純 C++ 那條（原 `try_3`）已收斂成兩交付物、**抽離成獨立 sub_proj [cllm](../cllm/core/README.md)**。

| 路徑 | 內容 |
|------|------|
| `AGENTS.md` / `CLAUDE.md` / `WORKFLOWS.md` / `INDEX.md` / `DEV-GUIDE.md` | 頂層路由 / 轉址 / 派發 / 地圖 / 結構整理參考 |
| `SESSION-LOG.md` / `WAIT_USER.md` | 活狀態（open-only）：進度 hub、待使用者項 |
| `workflows/` | 開發工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）|
| [`gen_v0/`](gen_v0/README.md) | **確定性台詞生成器・玩具版**（Lua）：`G(條件)→台詞` 純函數雛形，條件全以 lua table 表述；範圍鎖死河鹿堂定點。跑法 `cd gen_v0 && lua main.lua`。 |
| [`gen_v1/`](gen_v1/README.md) | **事實層地基**：事實/敘事風格分離——facts（帶型別邊的分層 LOD 網）→ director → act → realizer。事實庫玩具版已落地（Lua，六動詞＋兩道寫入門，九示範全 assert）；director/realizer 未動工。跑法 `cd gen_v1 && lua main.lua`。 |
| [`gen_v1_f/`](gen_v1_f/README.md) | **gen_v1 的 Fennel 移植**（Fennel 定調翻案復試）：行為與九示範逐條對齊 Lua 版，拆檔一比一。跑法 `cd gen_v1_f && fennel main.fnl`。 |
| [`gen_v1_l/`](gen_v1_l/README.md) | **gen_v1 的 Common Lisp 移植**（三線並排第三線，SBCL）：行為與九示範逐條對齊。跑法 `cd gen_v1_l && sbcl --script main.lisp`。 |
| [`realizer_f/`](realizer_f/README.md) | **敘事風格層（realizer）玩具原型**（Fennel＋Lisp macro）：`tmpl` macro 把句子模板當 list、編譯期折成字串接合（同像性）；`G(母題,風格座標)→整場河堤台詞`，固定旁白框＋多句子模板＋兩條護欄，六 demo 全綠。跑法 `cd realizer_f && fennel main.fnl`。 |
| [`uni/`](uni/README.md) | **統一節點 ＋ 共用 search kernel**（表示×演算法合一）：`八股→段→句式→填充→原子事實` 全是同一 `(node…)`／`(fact…)`、粒度由 refs 湧現；`direct` 搜 `*env*`（風格軸）生台詞＝gen_v0引擎≅director 的統一實現。取代 realizer_l 四表。四檔各≤51 行、九 demo 全綠。跑 `cd uni && sbcl --script main.lisp`。 |
| [`uniform_probe/`](uniform_probe/README.md) | **統一節點五路並行探索＋綜合**（bench）：a1–a5 各獨立最小 kernel＋一粒度切片，[synth](uniform_probe/synth/synth.lisp) 綜合成最小統一 kernel（102 行）。五發現：種≠粒度／分支=普通節點(DAG)／事實vs句式=兩域／填充物帶邏輯→界線溶解／層湧現+懸空=佔位。 |
| [`realizer_l/`](realizer_l/README.md) | **realizer（CL 壓縮版）＋director 搜索層**（macro 母語，接 comfy/gen_v1_l）：同一套八股（核心 90 行，Fennel 六檔 224 的壓縮對照）；**director 升格**——不再顯式給風格座標，改窮舉模板×填充＋成本挑最優，`intent`（甜/靜）搜出不同風格、`direct-n` 自動生 N 種台詞流（閉環最初手寫「20 種」）。九 demo 全綠。跑法 `cd realizer_l && sbcl --script main.lisp`。 |
| [`corpus/`](corpus/README.md) | **語料庫**（日和町 galgame，~150 檔）：34 篇劇本（含 2 篇逐句深度分析）＋52 短場景＋多尺度世界設定（`世界/`）＋14 份角色 dossier（`人物/`）＋19 題評測 benchmark＋固化規則素材。入口與 canon 專名見 [corpus/README](corpus/README.md)。|
| ~~`try_3/`~~ → [`../cllm/`](../cllm/core/README.md) | 玩具實驗場③（**純 C++、傳統 header**）已收斂成兩交付物（對外 C ABI `libcllm.so`＋`llm` unix filter CLI）、**抽離成獨立 sub_proj `cllm`**。舊 L0（`llm::Client` ask＋三擴充）封存於 `cllm/archived/`。|

## 開發工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)**——依「你想做什麼」派發。每個工作流的 durable 知識歸在 `workflows/<該工作流>/`（入口＝該夾 README 或主檔，含 `archive/` 封存過時文檔），具體流程在各自入口檔。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**（結構整理原則 + 四級成長軌跡）——**只在要重構/整理結構時取用**。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)；碰原始碼的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[gotchas](workflows/common/gotchas.md) 踩坑 + [conventions](workflows/common/conventions.md) 程式碼慣例 |

## 內容源頭與依賴

- **idea 礦脈（唯一內容來源）**：[../../workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](../../workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
- **依賴目標**：[llm_forge/](../llm_forge/AGENTS.md)（跑通的機制搬去固化）。**port 地基**：[../ver_1/try_implement/core_handy/](../ver_1/try_implement/core_handy/)。

## 活狀態（只列還沒完成的）

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根）→ 各工作流 session-log（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證的入口（repo 根；膨脹後拆 `wait_todo/` 分類檔）|
