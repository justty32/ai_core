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

檔案：[`llm.scm`](llm.scm)（接口本體＋schema）、[`json.scm`](json.scm)（s7⇄JSON，llm.scm `(load)` 它）、
[`test.scm`](test.scm)（離線測試）、[`s7host.c`](s7host.c)＋[`shim_include/`](shim_include/)（argv-aware host，見文末）。
s7 runtime 用 `C:\code\mine\pas\derived\s7-playground\s7.exe`（不需 FFI；純 Scheme ＋ `(system "curl…" #t)`）。

**開發循環**（playground 哲學，REPL 一直開）：

```scheme
(load "llm.scm")
(set! *llm-base-url* "http://localhost:1234/v1")     ; 後端用全域變數切（s7 無 getenv）
(llm-entry :prompt "你是一隻貓娘，請回答我問題" :temp 0.1)
(llm-entry :in "q.txt" :out "a.txt" :sys "你是傲嬌貓娘" :temp 0.8 :top-p 0.9 :max-tokens 256)
```

keyword 參數 ＝ schema 的直接投影：`:prompt :in :out :sys :model :temp :top-p :top-k
:max-tokens :n :seed :presence-penalty :frequency-penalty :base-url :api-key`。無 streaming。
這串**不是手寫**的——`llm.scm` 裡的 `*llm-schema*` 是**唯一真相源**，`llm-entry` 的 `define*`
簽章由它生成、取樣參數也由它 runtime 驅動塞入。要加參數（`stop`／`logit_bias`…）＝schema 加一行。

**離線跑測**（curl `file://` 灌假回應，不需真後端）：`cd try_1` 後
`…\s7.exe test.scm`（s7 只吃「剛好一個檔名」的參數）。

**設計要點 / 學到的**：
- 請求用 s7 `inlet` 表達、`s7->json` 序列化；回應 `json->s7` 成 inlet 導航——同像性，零手工 escape。
- **schema 驅動**：`*llm-schema*` 每列 `(scheme名 json鍵-或-ctrl)`。`make-llm-entry!` 用 `eval` 生成一個
  薄殼 `define*`（只把 keyword 值收成 inlet），真正邏輯在 `llm-entry-impl`；取樣參數在 impl 裡
  `for-each` 迭代 schema 塞入——參數列與塞入邏輯都不再手寫。
- `json.scm`（抠自 playground `lib/json.scm`）補上 boolean/null（原版沒有，`stream:false` 會炸）；
  此 s7 版**不准 `(reverse let)`**，已改直接迭代。
- **s7 拿不到 argv**（`main()` 只 `s7_load` 一個檔、不綁 argv）→ 已做 `s7host.exe`（見下）補上
  `*argv*`；剩下的 `--flag` CLI 薄殼還沒生成，正是由同一張 `*llm-schema*` 產出的下一步。

## argv-aware s7 host（`s7host.exe`）

s7 內建 `main()` 只吃「剛好一個檔名」、不把 argv 傳進 Scheme。[`s7host.c`](s7host.c) 補上：把
`argv[2..]` 綁成 Scheme 變數 `*argv*`（字串 list、順序保持），再 `s7_load` 腳本（`argv[1]`）。
Windows 上用 `wmain`＋`-municode`＋`WideCharToMultiByte` 轉 UTF-8，中文參數不亂碼。

**編**（Git Bash，先 `export PATH=/c/dev/mingw64/bin:$PATH`）：

```sh
cd try_1
gcc s7host.c "C:/code/mine/pas/derived/s7-playground/s7.c" -o s7host.exe \
    -I "C:/code/mine/pas/derived/s7-playground" -I "./shim_include" \
    -O2 -lm -municode
```

- 把 `s7.c` 當庫一起編、**不定義 `WITH_MAIN`**（否則撞 s7 自己的 main）。
- MinGW 上 `WITH_C_LOADER` 自動變 0（不需 FFI），所以只缺 `<sys/utsname.h>` 一個標頭 →
  由 [`shim_include/sys/utsname.h`](shim_include/sys/utsname.h) 補最小版（`dlfcn`/`realpath` 那兩個
  只在 C-loader 開時才需要，此路用不到）。

**用**：`s7host.exe <script.scm> [arg1 arg2 …]`；腳本裡 `(for-each display *argv*)` 取參數。
無參數印用法回傳 1、腳本載入失敗回傳 2。範例見 [`argv_test.scm`](argv_test.scm)。

**下一步（等細規劃）**：由 `*llm-schema*` 生成 `--flag` CLI 薄殼（解析 `*argv*` → 呼叫 `llm-entry`），
讓 `s7host.exe cli.scm --prompt "hi" --temp 0.7` 成真；接真後端實跑（見 WAIT_USER）。
