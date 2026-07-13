# try_1 — 隨便玩的 LLM 接口實驗場

galtxt 底下的**玩具 sub-proj**，**刻意不套**九軸／workflow 那套框架組織。目的只有一個：先跑通一個
**能在 shell 用指令呼叫的統一 LLM 接口**，backend 隨便換（LM Studio / DeepSeek / 任何 OpenAI 相容端點）。
晚點要細規劃時再從這裡的實跑經驗往上抽。

> **⚠ 方向已轉（2026-07-13 同日）**：C++ 版**放下**（收進「效能瓶頸再說」抽屜），現行主線是
> **s7 Scheme 版 [`llm.scm`](llm.scm)**——REPL 函式 `(llm-entry :prompt … :temp …)`，跑通離線 E2E。
> 轉向緣由：悟到「函式參數 ⇄ CLI 旗標」的對應**該由 schema meta-programming 生成、不該手寫**，
> 而 Lisp 同像性正是那台機器。下面 C++ 段落留作已擱置版本的參考。s7 版用法見文末「s7 版」。

## 需要什麼

- **mingw64 g++**（本機 `C:\dev\mingw64\bin`，實測 g++ 16.1.0）
- **curl**（Windows 10+ 內建 `C:\Windows\system32\curl.exe`）
- 一個 OpenAI 相容端點（本機跑 LM Studio，或雲端 DeepSeek 之類）

## 編譯

```powershell
$env:Path = 'C:\dev\mingw64\bin;' + $env:Path
g++ -std=c++20 -O2 -static -o llm_entry.exe llm_entry.cpp simdjson.cpp -lshell32
```

- `-static`：免帶 mingw 執行期 DLL，任何 shell 都能直接跑。
- `-lshell32`：Windows 取寬字元命令列（`CommandLineToArgvW`）用，讓中文參數不亂碼。

## 用法

```powershell
# prompt 直接當參數（有空白請用引號括住）
.\llm_entry.exe --prompt "你是一隻貓娘，請回答我問題"

# prompt 讀檔、結果寫檔
.\llm_entry.exe --in question.txt --out answer.txt

# 兩者共用：--in 內容接在 --prompt 後面（中間補一個換行）
.\llm_entry.exe --prompt "你是一隻貓娘，用可愛語氣回答：" --in question.txt

# 都沒給 → 讀 stdin
echo 你好 | .\llm_entry.exe

# 取樣參數
.\llm_entry.exe --prompt "寫首短詩" --temp 0.1 --topp 0.4 --topk 40 --max-tokens 256
```

completion 印到 **stdout**（或 `--out` 檔）；token 用量與錯誤印到 **stderr**（`[meter] total_tokens=...`）。

### 旗標

| 旗標 | 值 | 說明 |
|---|---|---|
| `--prompt` | 文字 | prompt 本體 |
| `--in` | 檔路徑 | 讀檔內容，接在 `--prompt` 後面；**檔請存 UTF-8** |
| `--out` | 檔路徑 | completion 寫到檔（預設寫 stdout；寫檔不加尾換行） |
| `--model` | model id | 覆蓋 env 的 model |
| `--temp` | 浮點 | `temperature` ┐ |
| `--topp` | 浮點 | `top_p`     ├ **有給才塞進請求**；沒給就不送，讓 backend 用自己的預設 |
| `--topk` | 整數 | `top_k`     │ ⚠ 非 OpenAI 標準欄位，LM Studio/本地多半支援、DeepSeek 之類雲端可能不吃 |
| `--max-tokens` | 整數 | `max_tokens` ┘ |

## backend 用 env 切換

| 環境變數 | 預設 | 說明 |
|---|---|---|
| `AI_CORE_LLM_BASE_URL` | `http://localhost:1234/v1` | 到 `/v1` 為止；程式會自己接 `/chat/completions` |
| `AI_CORE_LLM_MODEL` | `local-model` | LM Studio 用當前載入的模型；雲端要填真實 model id（或用 `--model`）|
| `AI_CORE_LLM_API_KEY` | （空） | 有值才加 `Authorization: Bearer`；本機通常留空 |

env 名沿用 core_handy 慣例，方便日後跟 C++ 主線對齊。範例：

```powershell
# LM Studio（本機，已載入模型、開好 server）— 用預設即可
.\llm_entry.exe --prompt "你好"

# DeepSeek（雲端，超便宜）
$env:AI_CORE_LLM_BASE_URL = "https://api.deepseek.com/v1"
$env:AI_CORE_LLM_API_KEY  = "sk-你的key"
.\llm_entry.exe --model deepseek-chat --prompt "你好" --temp 0.7
```

## 檔案

| 檔 | 是什麼 |
|---|---|
| `llm_entry.cpp` | 全部邏輯（收參數 → 組 prompt → 組請求 → curl → simdjson 抽 content） |
| `simdjson.h` / `simdjson.cpp` | vendored simdjson 4.6.1 單頭 amalgamation（最快的 JSON 解析，尻下來直接用） |
| `llm_entry.exe` | 編出來的執行檔（build 產物，不進版控） |

## 刻意還沒做（等細規劃）

- **daemon / `--serve`**：core_handy 有長駐 socket 版（LLM 單資源循序佇列）；try_1 先只做 one-shot。
- **rate meter 跨呼叫累計、`--metadata` 自述、trace log（NDJSON 礦堆）**：留給之後往 llm_forge 抽時再說。
- **多輪對話 / system role 分離 / 更多參數（seed / stop…）**：現在把 prompt 全塞單輪 user message。

---

## s7 版（現行主線）

檔案：[`llm.scm`](llm.scm)（工具）、[`test.scm`](test.scm)（離線測試）。s7 runtime 用
`C:\code\mine\pas\derived\s7-playground\s7.exe`（不需 FFI；純 Scheme ＋ `(system "curl…" #t)`）。

**開發循環**（playground 哲學，REPL 一直開）：

```scheme
(load "llm.scm")
(set! *llm-base-url* "http://localhost:1234/v1")     ; 後端用全域變數切（s7 無 getenv）
(llm-entry :prompt "你是一隻貓娘，請回答我問題" :temp 0.1)
(llm-entry :in "q.txt" :out "a.txt" :sys "你是傲嬌貓娘" :temp 0.8 :top-p 0.9 :max-tokens 256)
```

keyword 參數 ＝ schema 的直接投影：`:prompt :in :out :sys :model :temp :top-p :top-k
:max-tokens :n :seed :presence-penalty :frequency-penalty :base-url :api-key`。無 streaming。

**離線跑測**（curl `file://` 灌假回應，不需真後端）：`cd try_1` 後
`…\s7.exe test.scm`（s7 只吃「剛好一個檔名」的參數）。

**設計要點 / 學到的**：
- 請求用 s7 `inlet` 表達、`s7->json` 序列化；回應 `json->s7` 成 inlet 導航——同像性，零手工 escape。
- `json.scm` 兩個 helper 抠自 playground `lib/json.scm`，並補上 boolean/null（原版沒有，`stream:false` 會炸）；此 s7 版**不准 `(reverse let)`**，已改直接迭代。
- **s7 拿不到 argv**（`main()` 只 `s7_load` 一個檔、不綁 argv）→ shell 的 `--flag` CLI 要靠之後的「argv-aware s7 host（讓 shebang 成真）」，那層正是由 schema 生成的薄殼。

**下一步（等細規劃）**：把 `define*` 簽章改由一張 **schema 表生成**（消滅手寫參數列）；抽出獨立 `json.scm`；argv-aware host。
