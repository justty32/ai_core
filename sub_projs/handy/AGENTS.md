# handy — AI agent 專案備忘

handy = **路徑一（把 OS 當 AI agent）的落地試驗田——一組靈活的小腳本／小程式集，拿現成程式（尤其 [cllm](../cllm/AGENTS.md) 與其 tool）用腳本包裝、按慣例組合**。北極星＝**整個作業系統變成一個 AI agent**（工具集，非單體）。本檔是最頂層路由器：只指向下一層，durable 細節不寫這裡。

## 定位：一塊試驗田，陸續往裡丟小程式

**handy 本身就是那一塊試驗田**——一個資料夾，之後往裡面放**一大堆小程式／小腳本**。不預先分格、不搞子田；每個小東西各自成一攤，用同一套方法組合。

頭兩個要住進來的（只是起手，不是全部）：

```
handy/
├─ llme     cllm 之上的多 endpoint dispatcher
│           （資料夾＝callable：_exec ＋各 endpoint config，外面 llme.sh 轉發）
└─ daemon   常駐 server，client 注入命令
            （開新 agent＝claude -p；操縱活 agent＝讓它自己 poll 檔案讀命令）
```

共享**同一個方法論**：拿現成程式、腳本包裝、資料夾＝callable、按慣例組合。這是 [0708 一切皆檔案](../../workflows/notes/20260708-1040-一切皆檔案-list-統一描述子-複合路徑.md)「folder-as-callable」落到地上的第一個真身。

## 先讀哪裡

- **想懂本專案的來龍去脈／設計決定** → [設計 note：daemon 與 llme 兩塊試驗田](../../workflows/notes/20260718-0924-路徑一-daemon與llme兩塊試驗田.md)（本專案的收斂記錄；接同日 [岔路盤點](../../workflows/notes/20260718-0727-三條路岔路盤點-os語言-cllm函數-lisp蒸餾.md)）。
- **要用 cllm／cllm tool** → [cllm/AGENTS.md](../cllm/AGENTS.md)。重點現成件：`libcllm.so`（唯一入口 `llm_ask`）＋`llm` CLI、`tools/llm-login`（登入/重登：`llm-login token` 給腳本 `$(...)`）、`tools/anthropic-proxy`。

## 不重造（本專案吃現成、別憑記憶重建）

| 要的 | 現成實體 | 位置 |
|---|---|---|
| daemon 骨架（常駐＋socket＋佇列＋單例計量） | `llm_entry.cpp` 的 `--serve <sock>` | `../ver_1/try_implement/core_handy/examples/llm_entry.cpp` |
| 登入／key 失效重登／patch config | `llm-login` | `../cllm/tools/llm-login/` |
| 命令 log／memoize／可靠度原料 | NDJSON trace 構想 | note 0718 §6.3 |

## 鐵律（always-on）

1. **這是試驗田，不是基礎建設**——低風險、隨時可推倒；守住「這是玩、不是一次蓋對」，才不落回 note 0718 §四.1「可以永遠做下去卻沒消費者」的 yak-shave。
2. **拿現成用腳本包**——優先 port／wrap 上表現成件，別憑記憶重造（避免「一直重來」）。
3. **先執行再說，邊做邊生慣例**——`llme` config 慣例、poll「已讀」形狀等，實作時遇到再定，不開工前立法。
4. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
5. **改動使任一層文檔失準時同步更新**——本檔、上層 [sub_projs/README.md](../README.md)、[ai_core INDEX](../../INDEX.md)。
6. **所有回覆、註解、留檔用繁體中文**；程式碼識別子、shell 指令、技術名詞保留原文。

## 開發環境

- **重度依賴 cllm**：本專案的「肉」多是薄腳本／薄程式，把 cllm 及其 tool 串起來。建 cllm 見 [cllm/AGENTS.md](../cllm/AGENTS.md)（CMake＋Ninja＋vcpkg）。
- **技術棧**〔決定・2026-07-18〕：**腳本層＝LuaJIT ＋ Fennel**（Fennel 編到 LuaJIT，手寫用 Lisp 手感、同 runtime；少 Python——太重、每腳本冷啟 ~50–150ms）。三層分工看性質選：
  - **純膠水**（挑檔／轉發／exec）→ 一般選 **編譯 C++／shell**（零 interpreter 啟動）；**但若膠水緊接一次慢呼叫**（如 `llme` 接 LLM，啟動稅埋在 token 延遲裡＝雜訊）→ 免建置的 **Fennel** 更順手。llme 就是後者（Fennel 一個檔、shebang 直接跑）。
  - **有邏輯的小工具** → **Fennel/LuaJIT**（冷啟 ~1–5ms，甜蜜區；預編成 `.lua` 免重複編譯）。
  - **要免重載重狀態** → 掛 **daemon** 當瘦 client（見下）。
  - 呼叫 cllm：cllm 已有 Lua＋Fennel 綁定；LuaJIT 還能 **FFI 直接 call `libcllm.so`**（連綁定都省）。
- **daemon（`--serve`）暫緩，但要記對觸發條件**〔洞見・2026-07-18〕：**不是**為了省啟動——LLM 呼叫的載入環境時間 vs token 吐出延遲差幾個數量級，那幾 ms 是雜訊，為此做 daemon 不划算。daemon 的**真正觸發＝需要跨呼叫的共享狀態**（`llm_entry` 的 `RateMeter` 跨請求累計、LLM 當單一資源循序佇列化、活 agent 保 context）。→ Q1「操縱活 agent」就是這類，那才是 daemon 正主。
- **本地測試後端**〔決定・2026-07-18〕：一律用 **gemma**，且**只用 `google/gemma-4-e4b`（小・快，不要 31b）**——`llme local`（`llme/configs/local.json` 的 model）已釘成它。要跑真後端測試就 `llme local …`／`./zhtw …`（需 LM Studio 在 `localhost:1234`）。
- **驗證**：目前**尚無**程式碼、無驗證指令（開田期）；有第一片可跑物後再補。

## 主工作流（進度與待測）

事情告一段落或臨時中止時 → 把**還沒完成**的活狀態記到進度；需**使用者親自做／驗證**的 → 記到待使用者。兩者**只列 open**，完成即移除。

### 進度（open）
- 〔可用・已驗真後端〕`llme`（第一個住戶，**Fennel**）：薄轉發器已成。`llme <endpoint>` → `llm --config configs/<endpoint>.json <其餘>`。**真後端跑通**（LM Studio）：`llme local` → gemma、`llme qwen` → qwen3.5-9b，**換 endpoint 名＝換模型**（dispatcher 核心價值當場證明）；串流 `--stream` 逐段印、非串流、經 `llme.sh` 外殼皆綠、exit 0。也驗過離線 `file://` fixture 全鏈。`llme`／`llm` 已上 PATH（`~/.local/bin` symlink，本機；別台複製法見 README「上 PATH」）。config 慣例〔提案〕`<endpoint>.json`、可改。入口 [llme/README.md](llme/README.md)。
- 〔可用〕`zhtw`（薄包裝範例，單檔 Fennel）：固定取樣參數＋system＋prompt 前置，轉呼 `llme local`（繁中翻譯人格）。改最上面幾個常數即換人格。真後端驗過。**stdin 慣例〔2026-07-18〕**：有位置參數＝拿參數；無參數＝讀 stdin（`cat file | zhtw`、`zhtw <<< …`），兩者皆空才印用法 exit 2——**薄包裝的通用管線公民形狀，之後住戶照抄**。四路徑（argv／pipe／redirect／空）本地 gemma 驗綠。**發現**：①〔已解〕`llm` CLI 原本**沒有 `--system`**——2026-07-18 已在 cllm 補上真 system-role（`--system <文字>` → cabi 在 user 訊息前插 `{"role":"system"}`；`LLM_DUMP_BODY=1` 可觀測 body，cli_smoke 35/35）；zhtw 已改吃 `--system`，不再折進 prompt 開頭。②reasoning 模型（qwen3.5/gemma）思考鏈與答案共用 `--max-tokens`，給小值會偶爾空輸出→翻譯設 4096。入口見檔頭註解。
- 〔待動手〕daemon（下一個住戶）〔note 0718 §六提案〕：常駐小程式，client 寫命令進一個檔／socket，daemon 讀到就 `claude -p` 起 headless run；命令通道複用 append-log。

### 待使用者（open）
- （暫無）
