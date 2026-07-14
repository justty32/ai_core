# try_2 — C++ ＋ 內嵌 Lua 的 LLM 接口實驗場

galtxt 底下的**玩具 sub-proj**（與 [try_1](../try_1/README.md) 並存），**刻意不套**九軸／workflow 那套框架組織。
跟 try_1 同一個目的——先跑通一個**能在 shell 用指令呼叫的統一 LLM 接口**，backend 隨便換
（LM Studio / DeepSeek / 任何 OpenAI 相容端點）——但換了**技術棧地基**：

> **try_1 ＝ s7 Scheme（純腳本 + `(system curl)`）；try_2 ＝ C++ 效能核心內嵌 Lua VM。**
> 對應 AGENTS.md 技術棧方向〔idea 礦脈 §9.2〕：主力 C++ ＋ 內嵌 Lua 當通用腳本層（未來 Fennel 編成 Lua、同 runtime）。
> 兩條線並跑、互為對照；哪套「函式參數 ⇄ CLI 旗標由 schema 生成」更順手，跑過才知道。

**try_2 起步就比 try_1 完整**：argv 從第一天就有（自寫 host），所以直接做到 try_1 還沒做的
**由 schema 生成的 `--flag` CLI**（[cli.lua](cli.lua)）——「函式參數 ⇄ CLI 旗標同源」洞見已落地。

## 架構（四塊 + 一個 host）

```
host.exe (C++)  ── 內嵌 Lua 5.5 VM、綁命令列成全域 arg 表、UTF-8、load 並跑 .lua 腳本
   │  跑
   ├─ cli.lua ── 由 llm.lua 的 schema「生成」--flag 介面：解析 arg → 呼叫 llm_entry
   ├─ llm.lua ── ★ 接口本體 + 參數 schema（唯一真相源）；組請求→curl→解析回應
   │     └─ dofile ─▶ json.lua ── vendored rxi/json.lua 0.1.2（MIT）；encode/decode
   └─ test.lua ── 全離線 E2E（curl file:// 灌假回應）
```

- **schema 單一真相源**（[llm.lua](llm.lua) 的 `M.schema`）：每列 `{名稱, JSON鍵或"ctrl", 值型別}`，
  同時驅動 ① 呼叫端參數驗證 ② JSON 欄位對映 ③ CLI 旗標名 ④ CLI 值型別解析。
  **加參數（`stop`／`logit_bias`…）＝schema 加一行**，llm.lua／cli.lua 別處零改動。
  （比 try_1 的 2 欄 schema 多一欄型別，把 CLI 那一哩也涵蓋進來。）
- **同像性精神**：請求用 Lua table 表達、`json.encode` 序列化；回應 `json.decode` 成 table 導航——零手工 escape。

## ★ Lua 格式輸出（`--lua` / `lua=true`）

try_2 的關鍵洞見：**runtime 就是 Lua，何必讓模型吐 JSON 再拿 parser 解？** 讓模型直接吐 **Lua table 字面量**，
host 一個 `load()` 就變**原生 table**——零 parser、同像性，而且 Lua 語法對 LLM 更寬容（尾逗號、註解），
多行中文台詞用 `[[…]]` 長字串免跳脫。這是「Lua 當基質」整條線的收束。

```lua
local ch = llm.llm_entry{ prompt = "生成傲嬌角色", lua = true }   -- 回來是 table，不是字串
print(ch.name, ch.affection + 1, ch.lines[1])                     -- 直接導航／運算
```

```sh
./host.exe cli.lua --prompt "生成角色" --lua      # CLI 也行；--lua 是布林旗標，dump 回乾淨 Lua 字面量
./host.exe demo_lua.lua                            # 離線示範（fake_lua 假回應）
```

- **安全（重要）**：`load()` LLM 輸出＝執行任意 Lua，所以走**沙箱**——`load(src, "=llm", "t", {})`：
  `"t"` 只吃文字不吃 bytecode、**空環境**沒有 `os`/`io`/`load`，只能建純資料 table。副作用型內容（`os.execute…`）直接失敗。
- **容錯**：自動剝除 ```` ```lua … ``` ```` markdown 圍欄、缺 `return` 會補上（見 `llm.parse_lua`）。
- 注意：這只換掉**內容層**；OpenAI API 外層信封（`choices[…].message.content`）仍是後端回的 JSON，照樣用 `json.lua` 解。

## API 縫：`llm.ask`（table 進、table 出）

介面契約先行、實作可換：**對 Lua 而言，傳一個 table 出去、收一個 table 回來；中間 JSON/HTTP 誰做怎麼做，Lua 不管。**
`json.lua` 是這條縫**背後的實作細節**——現在由 Lua 做，將來可換成 C++ 原生函式（`lua_newtable`/`lua_setfield` 直接推 table），
而呼叫端 `llm.ask{…}` 一行都不用改。

```lua
local llm = dofile("llm.lua")
local ch = llm.ask{ prompt = "生成傲嬌角色" }   -- 不必寫 lua=true；ask 預設吐 table
print(ch.name, ch.lines[1])
```

- 非串流：回**模型答案的原生 table**（缺內容/解析失敗回 `nil`）。
- `llm.llm_entry{…}` 仍是底層入口（可回字串／table／串流）；`ask` 只是把 (A) 契約收乾淨的薄殼。

## 串流（streaming）

`streaming = true` 時**同時要帶 `callback` 函式**；呼叫**即時回 `{ ok = bool, err? }`**（內容不放回傳值），
成功的話 `callback` 隨後被**持續呼叫**，把生成內容一片片餵回。可用 `chunk_chars` 設「累積幾個 **UTF-8 字**才呼叫一次」。

```lua
llm.ask{
  prompt = "講個故事",
  streaming = true,
  chunk_chars = 4,                         -- 每 4 個 UTF-8 字餵一次（省略＝每片就緒即餵）
  callback = function(chunk)
    io.write(chunk); io.flush()            -- 逐塊即時顯示
  end,
}   -- → { ok = true }
```

```sh
./host.exe cli.lua --prompt "講個故事" --streaming --chunk-chars 4   # CLI 用內建 callback 即時印
./host.exe demo_stream.lua                                          # 離線示範（fake_stream SSE 假回應）
```

- **UTF-8 安全**：按 code point（字）計數、**絕不把多位元組中文切一半**（跨 SSE delta 的半個字先留緩衝，湊齊才吐）——用 Lua 5.5 `utf8` 庫。
- **傳輸**：`curl -N`（不緩衝）逐行讀 SSE（`data: {…}` / `data: [DONE]`），每片 `delta.content` 累積到門檻→callback。
- `callback` 是函式，**只能從 Lua 端傳**（CLI 無此旗標，`--streaming` 改用內建「印到 stdout」callback）。
- 串流與 `--lua`（吐 table）不相組：串流是**文字增量**；要結構化 table 用非串流的 `ask`。

## 需要什麼

- **MinGW g++/gcc/ar**（本機 `C:\dev\mingw64\bin`，實測 16.1.0）
- **curl**（Windows 10+ 內建）
- 一個 OpenAI 相容端點（本機 LM Studio，或雲端 DeepSeek 之類）
- Lua 5.5.0 原始碼**已 vendored 進 [`vendor/lua/`](vendor/lua/)**（官方 lua.org 下載，進版控）——不需另裝 Lua。

## 編譯

```sh
# Git Bash；build.sh 會把 MinGW 加進 PATH
./build.sh          # 建 liblua.a（vendored Lua 5.5）+ host.exe + lua.exe
./build.sh host     # 只重建 host.exe（liblua.a 已在時最常用）
./build.sh luaexe   # 只重建 lua.exe（給 VSCode debug 用）
./build.sh lua      # 只重建 liblua.a
```

- `liblua.a`：`vendor/lua/*.c`（除 `lua.c`／`luac.c` 兩個 main 外全部）編成的靜態庫。
- `host.exe` ＝ `g++ … host.cpp … -llua -municode`（`-municode`＋`wmain` 取寬字元命令列，**中文參數安全**）——**正式 CLI**。
- `lua.exe` ＝ `gcc vendor/lua/lua.c … -llua`（標準 Lua 直譯器，內建 `arg` 表，我們腳本原封不動能跑）——**給 VSCode 除錯用**。
  ⚠ 標準 `main` 在 Windows 拿到的 argv 是系統碼頁（950）不是 UTF-8，**中文命令列參數會亂碼**；要餵中文走 `--infile`（讀檔是原始 UTF-8）或改用 host.exe。

> **⚠ Smart App Control（SAC）踩坑**：本機 SAC 為 On，偶爾會把**剛編好的某顆新 exe 雜湊**判成 unknown→擋
> （`應用程式控制原則已封鎖此檔案`）。這**不是**程式問題——**重編一次**產生新雜湊即放行（同源重編就好）。
> 已驗：純連 Lua、載外部腳本、C／C++ 全都能跑，只是偶發雜湊信譽卡住。詳見 [common/gotchas](../workflows/common/gotchas.md)。

## 用法

### CLI（[cli.lua](cli.lua)）

```sh
# prompt 當旗標（有空白用引號）
./host.exe cli.lua --prompt "你是一隻貓娘，請回答我問題"

# 讀檔 + 寫檔 + system role + 取樣參數
./host.exe cli.lua --infile q.txt --outfile a.txt --sys "傲嬌貓娘" --temp 0.8 --top-p 0.9 --max-tokens 256

# 看所有旗標（由 schema 生成）
./host.exe cli.lua --help
```

completion 印到 **stdout**（或 `--outfile` 檔）；token 用量／錯誤印到 **stderr**（`[meter] total_tokens=…`）。
旗標名＝schema 名稱把底線換連字號：`max_tokens`→`--max-tokens`、`base_url`→`--base-url`。

| 旗標 | 值 | 說明 |
|---|---|---|
| `--prompt` | 文字 | prompt 本體 |
| `--infile` | 檔路徑 | 讀檔內容，接在 `--prompt` 後（檔存 UTF-8）|
| `--outfile` | 檔路徑 | completion 寫檔（預設寫 stdout）|
| `--sys` | 文字 | system role |
| `--model` | model id | 覆蓋 env 的 model |
| `--temp` `--top-p` `--top-k` `--max-tokens` `--n` `--seed` `--presence-penalty` `--frequency-penalty` | 數值 | 取樣參數，**有給才塞**進請求；沒給讓 backend 用預設 |
| `--base-url` `--api-key` | 文字 | 覆蓋後端設定 |

### Lua 端直接呼叫（playground）

```lua
-- host.exe 跑的腳本裡，或改 llm.lua 後重跑
local llm = dofile("llm.lua")
llm.base_url = "http://localhost:1234/v1"    -- 或設 env AI_CORE_LLM_BASE_URL
llm.llm_entry{ prompt = "你是一隻貓娘，請回答我問題", temp = 0.1 }
llm.llm_entry{ infile = "q.txt", outfile = "a.txt", sys = "傲嬌貓娘", temp = 0.8, top_p = 0.9, max_tokens = 256 }
```

**REPL（一直開著玩，＝try_1 s7 那種 playground 循環）**：stock `lua.exe` 的互動模式就是 Lua 5.5 REPL，不需第三方工具：

```sh
./lua.exe -i          # 進 5.5 REPL；> local llm=dofile("llm.lua"); llm.ask{prompt="嗨"}
```

（stock REPL 在 Windows 無行歷史/編輯；若哪天嫌煩，正解是給 lua.exe 編進 linenoise，不接停在 5.4 的第三方 REPL。）

## backend 用 env 切換（Lua 有 `os.getenv`，比 s7 順）

| 環境變數 | 預設 | 說明 |
|---|---|---|
| `AI_CORE_LLM_BASE_URL` | `http://localhost:1234/v1` | 到 `/v1` 為止；程式自接 `/chat/completions` |
| `AI_CORE_LLM_MODEL` | `local-model` | LM Studio 用當前載入模型；雲端填真實 id（或用 `--model`）|
| `AI_CORE_LLM_API_KEY` | （空） | 有值才加 `Authorization: Bearer` |

env 名沿用 core_handy 慣例。也可在 Lua 端 `llm.base_url = …` 直接覆蓋。

## 離線跑測

```sh
./host.exe test.lua
```

`test.lua` 把 `base_url` 指到 `file://…/fake`，curl 讀 [`fake/chat/completions`](fake/) 假回應，
不需真後端就驗完「請求組裝＋回應解析＋未知參數擋＋缺 prompt 擋」。實測全綠。

## 中文亂碼？（終端碼頁問題）

程式吐的是 **UTF-8**。若終端顯示亂碼，是**終端碼頁不是 UTF-8**（繁中 Windows 的 cmd/PowerShell 預設常是 950/Big5，把 UTF-8 讀成亂碼）。

- **host.exe 已自救**：啟動時 `SetConsoleOutputCP(CP_UTF8)`，中文不依賴終端預設，正常情況直接對。
- 若仍亂碼（或用 lua.exe），把終端切 UTF-8：cmd `chcp 65001`；PowerShell `[Console]::OutputEncoding=[Text.Encoding]::UTF8`。
- **中文命令列參數**：走 host.exe（`wmain` UTF-8 安全）。lua.exe 的 `--prompt 中文` 會壞，改 `--infile 檔`。

## VSCode 除錯 Lua

1. 裝擴充套件 **Local Lua Debugger**（發布者 `tomblind`，ID `tomblind.local-lua-debugger-vscode`）——純 Lua 除錯器、不需原生模組。（另可裝 `sumneko.lua` 做 IntelliSense。）
2. `./build.sh luaexe` 產生 `lua.exe`（除錯器要標準直譯器；**不能用 host.exe**——它不吃除錯器注入的 `-e` 引導）。
3. VSCode「File > Open Folder」開 **try_2** 這個資料夾（讓 `${workspaceFolder}` 指到這裡）。
4. 在 `cli.lua`／`llm.lua`／`json.lua` 想停的行按左側下中斷點 → F5，選設定檔（[.vscode/launch.json](.vscode/launch.json) 已備好離線／真後端／目前檔三種）→ 單步、看變數、call stack。

離線設定用 `--infile sample_prompt.txt` 餵中文（避開 lua.exe 的 argv 碼頁坑），debug 時看到的資料就是正確中文。

## 檔案

| 檔 | 是什麼 | 版控 |
|---|---|---|
| `host.cpp` | C++ host：內嵌 Lua、綁 `arg`、UTF-8（含主控台碼頁）、跑腳本 | ✓ |
| `json.lua` | vendored rxi/json.lua 0.1.2（MIT）；encode/decode | ✓ |
| `llm.lua` | ★ 接口本體 + 參數 schema（唯一真相源）+ `ask` 縫 + 串流 | ✓ |
| `cli.lua` | 由 schema 生成的 `--flag` CLI 薄殼（含 `--lua`／`--streaming`）| ✓ |
| `test.lua` | 離線 E2E（含 Lua 格式輸出 + 沙箱 + 串流案例）| ✓ |
| `demo_lua.lua` | ★ Lua 格式輸出示範（LLM 吐 table→原生 table）| ✓ |
| `demo_stream.lua` | ★ 串流示範（streaming + callback + chunk_chars）| ✓ |
| `sample_prompt.txt` | 中文 prompt fixture（給 lua.exe `--infile` debug 用）| ✓ |
| `build.sh` | 重建 liblua.a + host.exe + lua.exe | ✓ |
| `.vscode/` | 除錯 launch.json + 任務 tasks.json + IntelliSense settings.json | ✓ |
| `vendor/lua/*.c *.h` | vendored Lua 5.5.0 原始碼 | ✓ |
| `vendor/lua/liblua.a` | 編出的靜態庫（build 產物）| ✗ |
| `host.exe` / `lua.exe` | build 產物 | ✗ |
| `galtxt_llm_req.json` / `fake/` / `fake_lua/` / `fake_stream/` | 執行期暫存 / 測試 fixture | ✗ |

## 刻意還沒做（等細規劃 / 待使用者）

- **接真後端實跑**：離線已綠，真回應需備環境（LM Studio 開 server / DeepSeek key）——見 [WAIT_USER](../WAIT_USER.md)。
- **daemon / 多輪對話 / `--metadata` 自述 / trace log**：跟 try_1 一樣先只做 one-shot，留給往 llm_forge 抽時再說。
- **Fennel**：技術棧終點是「手寫用 Fennel、編成 Lua」；目前先純 Lua 5.5 跑通，Fennel 之後再疊。
- **把 C++ 核心做厚**：現在 host 只做「內嵌＋綁 argv＋跑腳本」，效能核心（HTTP／JSON 下沉到 C++）等瓶頸出現再說。
