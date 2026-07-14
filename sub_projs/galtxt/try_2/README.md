# try_2 — C++ ＋ 內嵌 Lua 的 LLM 接口實驗場

galtxt 底下的**玩具 sub-proj**（與 [try_1](../try_1/README.md) 並存），**刻意不套**九軸／workflow 那套框架組織。
跟 try_1 同一個目的——先跑通一個**能在 shell 用指令呼叫的統一 LLM 接口**，backend 隨便換
（LM Studio / DeepSeek / 任何 OpenAI 相容端點）——但換了**技術棧地基**：

> **try_1 ＝ s7 Scheme（純腳本 + `(system curl)`）；try_2 ＝ C++ 效能核心內嵌 Lua VM。**
> 對應 AGENTS.md 技術棧方向〔idea 礦脈 §9.2〕：主力 C++ ＋ 內嵌 Lua 當通用腳本層。**try_2 就是純 Lua**——想玩 Lisp 走 try_1（s7 線），不上 Fennel。
> 兩條線並跑、互為對照；哪套「函式參數 ⇄ CLI 旗標由 schema 生成」更順手，跑過才知道。

**try_2 起步就比 try_1 完整**：argv 從第一天就有（自寫 host），所以直接做到 try_1 還沒做的
**由 schema 生成的 `--flag` CLI**（[cli.lua](src/cli.lua)）——「函式參數 ⇄ CLI 旗標同源」洞見已落地。

## 目錄結構

```
try_2/
├─ host.cpp  build.sh  README.md  .gitignore
├─ vendor/lua/          vendored Lua 5.5.0 原始碼（linit.c 一處增修：註冊 cjson 進 preload）
├─ native/              ★ native（C）模組，編進 liblua.a → 內建 require
│   ├─ cjson.c          JSON codec（encode/decode）
│   └─ http.c           HTTP 傳輸（WinHTTP／libcurl／file://）；request/stream
├─ .vscode/             除錯 launch.json + 任務 tasks.json + IntelliSense settings.json
├─ src/                 ★ 函式庫本體
│   ├─ llm.lua          接口本體 + 參數 schema（唯一真相源）；ask 縫 / Lua 格式輸出 / 串流
│   ├─ cli.lua          由 schema 生成 --flag CLI 薄殼
│   └─ _path.lua        路徑工具（定位專案根、組 fixture 的 file:// URL）
├─ examples/            demo_lua.lua（Lua 格式輸出）、demo_stream.lua（串流）
├─ test/                test.lua（離線 E2E）、sample_prompt.txt、fixtures/{fake,fake_lua,fake_stream}/
└─ build/              （gitignored）host.exe / lua.exe / liblua.a
```

## 資料流（一個 host + 函式庫）

```
build/host.exe (C++)  ── 內嵌 Lua 5.5 VM、綁命令列成全域 arg 表、UTF-8、load 並跑 .lua 腳本
   │  跑                （liblua.a 內含 native/{cjson,http}.c；stock lua.exe 亦同源、同樣內建兩者）
   ├─ src/cli.lua ── 由 llm.lua 的 schema「生成」--flag 介面：解析 arg → 呼叫 llm_entry
   ├─ src/llm.lua ── ★ 接口本體 + 參數 schema（唯一真相源）；組請求→http→解析回應
   │     ├─ require ─▶ cjson（native C）；encode/decode
   │     └─ require ─▶ http （native C，Windows=WinHTTP／Linux=libcurl，file:// 自理）；request/stream
   └─ test/test.lua ── 全離線 E2E（base_url 指 file://，http 讀 test/fixtures/ 假回應）
```

> 模組間用 `debug.getinfo` **自解腳本位址**載入 sibling（location-independent，不綁 CWD）；
> fixture 路徑由 `src/_path.lua` 從專案根算出（不寫死機器路徑）；請求 body 直接以字串進 C，**不再寫暫存檔**。

- **schema 單一真相源**（[llm.lua](src/llm.lua) 的 `M.schema`）：每列 `{名稱, JSON鍵或"ctrl", 值型別}`，
  同時驅動 ① 呼叫端參數驗證 ② JSON 欄位對映 ③ CLI 旗標名 ④ CLI 值型別解析。
  **加參數（`stop`／`logit_bias`…）＝schema 加一行**，llm.lua／cli.lua 別處零改動。
  （比 try_1 的 2 欄 schema 多一欄型別，把 CLI 那一哩也涵蓋進來。）
- **同像性精神**：請求用 Lua table 表達、`cjson.encode`（native C）序列化；回應 `cjson.decode` 成 table 導航——零手工 escape。

## ★ Lua 格式輸出（`--lua` / `lua=true`）

try_2 的關鍵洞見：**runtime 就是 Lua，何必讓模型吐 JSON 再拿 parser 解？** 讓模型直接吐 **Lua table 字面量**，
host 一個 `load()` 就變**原生 table**——零 parser、同像性，而且 Lua 語法對 LLM 更寬容（尾逗號、註解），
多行中文台詞用 `[[…]]` 長字串免跳脫。這是「Lua 當基質」整條線的收束。

```lua
local ch = llm.llm_entry{ prompt = "生成傲嬌角色", lua = true }   -- 回來是 table，不是字串
print(ch.name, ch.affection + 1, ch.lines[1])                     -- 直接導航／運算
```

```sh
./build/host.exe src/cli.lua --prompt "生成角色" --lua      # CLI 也行；--lua 是布林旗標，dump 回乾淨 Lua 字面量
./build/host.exe examples/demo_lua.lua                            # 離線示範（fake_lua 假回應）
```

- **安全（重要）**：`load()` LLM 輸出＝執行任意 Lua，所以走**沙箱**——`load(src, "=llm", "t", {})`：
  `"t"` 只吃文字不吃 bytecode、**空環境**沒有 `os`/`io`/`load`，只能建純資料 table。副作用型內容（`os.execute…`）直接失敗。
- **容錯**：自動剝除 ```` ```lua … ``` ```` markdown 圍欄、缺 `return` 會補上（見 `llm.parse_lua`）。
- 注意：這只換掉**內容層**；OpenAI API 外層信封（`choices[…].message.content`）仍是後端回的 JSON，照樣用 `cjson` 解。

## API 縫：`llm.ask`（table 進、table 出）

介面契約先行、實作可換：**對 Lua 而言，傳一個 table 出去、收一個 table 回來；中間 JSON/HTTP 誰做怎麼做，Lua 不管。**
這條縫**背後的實作已下沉到 C**：JSON 組解＝native `cjson`（[native/cjson.c](native/cjson.c)，取代原 rxi/json.lua）；
HTTP 傳輸＝native `http`（[native/http.c](native/http.c)，取代原 `io.popen("curl …")`＋暫存檔）。
C 是「笨管子」只管 TLS＋round-trip／逐塊吐 bytes；SSE 拆框、UTF-8 分批、`ask` 縫都留 Lua。
呼叫端 `llm.ask{…}` 全程一行沒改——這正是「縫的價值」：實作換血、契約不動。

```lua
local llm = dofile("src/llm.lua")
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
./build/host.exe src/cli.lua --prompt "講個故事" --streaming --chunk-chars 4   # CLI 用內建 callback 即時印
./build/host.exe examples/demo_stream.lua                                          # 離線示範（fake_stream SSE 假回應）
```

- **UTF-8 安全**：按 code point（字）計數、**絕不把多位元組中文切一半**（跨 SSE delta 的半個字先留緩衝，湊齊才吐）——用 Lua 5.5 `utf8` 庫。
- **傳輸**：`http.stream` 把 raw bytes 逐塊經 `on_data` 餵回，Lua 端自己拆 SSE 行（`data: {…}` / `data: [DONE]`），每片 `delta.content` 累積到門檻→callback。
- `callback` 是函式，**只能從 Lua 端傳**（CLI 無此旗標，`--streaming` 改用內建「印到 stdout」callback）。
- 串流與 `--lua`（吐 table）不相組：串流是**文字增量**；要結構化 table 用非串流的 `ask`。

## 需要什麼

- **C++/C 編譯器**：Windows 用 MinGW g++/gcc/ar（本機 `C:\dev\mingw64\bin`，實測 16.1.0）；Linux（如 Manjaro）用原生 g++/gcc。
- **HTTP 庫**：Windows 用系統內建 **WinHTTP**（`-lwinhttp`，Schannel TLS，零安裝）；Linux 用 **libcurl**（`-lcurl`，`pacman -S curl`）。不再依賴外部 `curl` 執行檔。
- 一個 OpenAI 相容端點（本機 LM Studio，或雲端 DeepSeek 之類）
- Lua 5.5.0 原始碼**已 vendored 進 [`vendor/lua/`](vendor/lua/)**（官方 lua.org 下載，進版控）——不需另裝 Lua。

> **跨平台**：`build.sh` 以 `uname` 偵測平台，產物名一律帶 `.exe`（Linux 不在意副檔名，指令跨平台一致）。三處分流：① Windows 補 MinGW PATH＋`-municode`（wmain 寬字元）、連 `-lwinhttp`；Linux 原生、連 `-lcurl`。② **Lua 的平台開關 `-DLUA_USE_LINUX`（`LUAPLAT`）——Linux 必給**：`luaconf.h` 只在 `_WIN32` 時自動開 `LUA_USE_WINDOWS`，POSIX 這邊不給就沒有 `LUA_USE_POSIX`，`io.popen` 會是「`'popen' not supported`」的錯誤樁、`os.tmpname` 也退回 `tmpnam`（編譯還會噴警告）。連帶開的 `LUA_USE_DLOPEN` 只需再補 `-ldl`（readline 是 `lua.c` 執行期 dlopen，**不需** `-lreadline`）。③ `host.cpp` 的 Windows 專屬碼用 `#ifdef _WIN32` 包起、Linux 走純 `main`＋原生 UTF-8 argv。
>
> Lua 端則用 `package.config` 偵測 OS（`src/_path.lua` 的 cwd 探測、`llm.lua` 的暫存目錄都已跨平台）。**`.vscode/` 是 Windows VSCode 用的**；Linux 上另用編輯器（neovim 等）自行接除錯。
>
> **Linux 實測（2026-07-14，Manjaro，gcc 16.1.1＋libcurl 8.21）：全綠**——編譯零警告、離線測試全過（請求組裝／參數擋／Lua 格式輸出／沙箱／串流 UTF-8 分批）、`demo_lua`／`demo_stream` 正常、`cli.lua --help` 與中文參數無亂碼、`lua.exe` 的 `require("cjson")`／`require("http")` 皆內建可用、`file://` fixture 讀取正常、換 cwd 跑仍找得到 fixture（`_path.lua` 自解位址有效）。

## 編譯

```sh
# Git Bash；build.sh 會把 MinGW 加進 PATH。產物全落 build/（gitignored）
./build.sh          # 建 build/{liblua.a, host.exe, lua.exe}
./build.sh host     # 只重建 build/host.exe（liblua.a 已在時最常用）
./build.sh luaexe   # 只重建 build/lua.exe（給 VSCode debug 用）
./build.sh lua      # 只重建 build/liblua.a
```

- `build/liblua.a`：`vendor/lua/*.c`（除 `lua.c`／`luac.c` 兩個 main 外全部）編成的靜態庫。
- `build/host.exe` ＝ `g++ … host.cpp … -llua -municode`（`-municode`＋`wmain` 取寬字元命令列，**中文參數安全**）——**正式 CLI**。
- `build/lua.exe` ＝ `gcc vendor/lua/lua.c … -llua`（標準 Lua 直譯器，內建 `arg` 表，我們腳本原封不動能跑）——**給 VSCode 除錯用**。
  ⚠ 標準 `main` 在 Windows 拿到的 argv 是系統碼頁（950）不是 UTF-8，**中文命令列參數會亂碼**；要餵中文走 `--infile`（讀檔是原始 UTF-8）或改用 host.exe。

> **⚠ Smart App Control（SAC）踩坑**：本機 SAC 為 On，偶爾會把**剛編好的某顆新 exe 雜湊**判成 unknown→擋
> （`應用程式控制原則已封鎖此檔案`）。這**不是**程式問題——**重編一次**產生新雜湊即放行（同源重編就好）。
> 已驗：純連 Lua、載外部腳本、C／C++ 全都能跑，只是偶發雜湊信譽卡住。詳見 [common/gotchas](../workflows/common/gotchas.md)。

## 用法

### CLI（[cli.lua](src/cli.lua)）

```sh
# prompt 當旗標（有空白用引號）
./build/host.exe src/cli.lua --prompt "你是一隻貓娘，請回答我問題"

# 讀檔 + 寫檔 + system role + 取樣參數
./build/host.exe src/cli.lua --infile q.txt --outfile a.txt --sys "傲嬌貓娘" --temp 0.8 --top-p 0.9 --max-tokens 256

# 看所有旗標（由 schema 生成）
./build/host.exe src/cli.lua --help
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
local llm = dofile("src/llm.lua")
llm.base_url = "http://localhost:1234/v1"    -- 或設 env AI_CORE_LLM_BASE_URL
llm.llm_entry{ prompt = "你是一隻貓娘，請回答我問題", temp = 0.1 }
llm.llm_entry{ infile = "q.txt", outfile = "a.txt", sys = "傲嬌貓娘", temp = 0.8, top_p = 0.9, max_tokens = 256 }
```

**REPL（一直開著玩，＝try_1 s7 那種 playground 循環）**：stock `lua.exe` 的互動模式就是 Lua 5.5 REPL，不需第三方工具：

```sh
./build/lua.exe -i          # 進 5.5 REPL；> local llm=dofile("src/llm.lua"); llm.ask{prompt="嗨"}
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
./build/host.exe test/test.lua
```

`test.lua` 把 `base_url` 指到 `file://…/test/fixtures/fake`，`http` 模組的 `file://` 特例讀 [`test/fixtures/fake/chat/completions`](test/fixtures/fake/) 假回應，
不需真後端就驗完「請求組裝＋回應解析＋未知參數擋＋缺 prompt 擋＋串流」。實測全綠。
（WinHTTP 不支援 `file://`，故此特例由 `http.c` 自理——offline harness 兩平台一致。）

## 中文亂碼？（終端碼頁問題）

程式吐的是 **UTF-8**。若終端顯示亂碼，是**終端碼頁不是 UTF-8**（繁中 Windows 的 cmd/PowerShell 預設常是 950/Big5，把 UTF-8 讀成亂碼）。

- **host.exe 已自救**：啟動時 `SetConsoleOutputCP(CP_UTF8)`，中文不依賴終端預設，正常情況直接對。
- 若仍亂碼（或用 lua.exe），把終端切 UTF-8：cmd `chcp 65001`；PowerShell `[Console]::OutputEncoding=[Text.Encoding]::UTF8`。
- **中文命令列參數**：走 host.exe（`wmain` UTF-8 安全）。lua.exe 的 `--prompt 中文` 會壞，改 `--infile 檔`。

## VSCode 除錯 Lua

1. 裝擴充套件 **Local Lua Debugger**（發布者 `tomblind`，ID `tomblind.local-lua-debugger-vscode`）——純 Lua 除錯器、不需原生模組。（另可裝 `sumneko.lua` 做 IntelliSense。）
2. `./build.sh luaexe` 產生 `build/lua.exe`（除錯器要標準直譯器；**不能用 host.exe**——它不吃除錯器注入的 `-e` 引導）。此 `lua.exe` 與 host.exe 同源、一樣內建 `cjson`（`require("cjson")` 在除錯環境也可得）。
3. VSCode「File > Open Folder」開 **try_2** 這個資料夾（讓 `${workspaceFolder}` 指到這裡）。
4. 在 `src/cli.lua`／`src/llm.lua`／`native/cjson.c`（C 需另配原生除錯器；Lua 檔用本除錯器）想停的行按左側下中斷點 → F5，選設定檔（[.vscode/launch.json](.vscode/launch.json) 已備好離線／真後端／目前檔三種）→ 單步、看變數、call stack。

離線設定用 `--infile test/sample_prompt.txt` 餵中文（避開 lua.exe 的 argv 碼頁坑），debug 時看到的資料就是正確中文。

## 編輯器整合（clangd + lua_ls，nvim／VSCode 通用）

跟上一節「VSCode 除錯 Lua」（斷點單步）不同層——這節是**補全／跳定義／診斷**（IntelliSense），設定檔全放
專案根、**不綁定特定編輯器**，Neovim（LazyVim，`clangd` + `lua-language-server` 兩個 LSP client）跟
Windows 上的 VSCode 都吃同一份。

- **`compile_commands.json`**（clangd 用；[.gitignore](.gitignore) 排除，**不進版控**）
  由 [build.sh](build.sh) 每次執行（任何子指令：`all`／`host`／`lua`／`luaexe`）尾端自動重寫到
  **專案根**（clangd 從當前檔案往上找這個檔，放 `build/` 它找不到）。純 shell 手寫 JSON，逐一列出
  `host.cpp`、`native/*.c`、`vendor/lua/*.c`（含 `lua.c`／`luac.c`——這兩支平常不落進
  `liblua.a`，但列進去 clangd 才看得懂 Lua 原始碼、開檔不噴 include 找不到）三類原始檔各一筆
  `{directory, file, command}`。`command` 用「`-c` 編譯單元」形式，而非 `build.sh` 實際下的
  連結指令——差在連結旗標（`-L`/`-l`/`$MUNICODE`/`$HTTPLIBS`/`$PLATLIBS`），那些只影響連結、
  不影響單一檔案的語法／巨集分析；唯一會動到編譯語意的是 **`$LUAPLAT`**（`-DLUA_USE_LINUX`
  那個巨集），這條照抄 `build.sh` 偵測到的當下平台值，所以這檔案內容**隨你在哪台機器編譯自動
  對——Linux 上重編一次，Windows 上重編一次，兩邊各自的 `compile_commands.json` 都對**。
- **`.luarc.json`**（lua_ls 用；進版控）——lua_ls 的專案設定，Neovim（nvim-lspconfig）跟 VSCode 的
  Lua 擴充套件都直接讀這份格式（比只有 VSCode 讀得到的 `.vscode/settings.json` 更中立）：
  - `runtime.version` 設 `"Lua 5.4"`——**版本落差**：vendored 的是 Lua 5.5.0，但 lua_ls 目前最高
    只認到 5.4（尚無 5.5 選項），5.4 是最接近的近似值，不代表專案真的是 5.4。
  - `diagnostics.globals` 列 `arg`——host.exe/lua.exe 綁的命令列全域表（`host.cpp` 的
    `bind_arg_table`），純腳本看不到定義來源，不設會被誤報 undefined-global。
  - `workspace.library` 掛 `meta/`（見下）；`checkThirdParty` 關掉。
  - **沒設 `runtime.path`**：`src/` 底下模組互相載入走 `debug.getinfo` 自解位址 + `dofile`（見
    `src/cli.lua`／`src/_path.lua`），不是 `require`——`dofile` 的路徑是執行期字串組出來的，
    lua_ls 本來就無法靜態解析，`runtime.path` 對這個模式幫不上忙。
- **`meta/cjson.lua`、`meta/http.lua`**（型別 stub；進版控）——`require("cjson")`／`require("http")`
  是編進 `liblua.a` 的 native C 模組（`linit.c` 塞進 `package.preload`），lua_ls 找不到對應的
  `.lua` 原始碼，會報「module not found」。這裡選的做法是寫**型別 stub**（`---@meta` 標記，只有
  函式簽名沒有實作）而不是用 `diagnostics.disable` 消音：stub 讓 `require` 解析到定義之餘，
  `json.encode`／`http.request` 等呼叫還有真正的補全與型別提示；`diagnostics.disable` 只會讓
  診斷閉嘴，拿不到任何提示，而且是對整個診斷代碼**全域**關掉，不夠精準。

**驗證方式**（headless nvim，免開真的視窗）：

```sh
nvim --headless "+e src/llm.lua" "+sleep 5" \
  "+lua for _,c in ipairs(vim.lsp.get_clients({bufnr=0})) do print('LSP: '..c.name) end" +qa
nvim --headless "+e host.cpp" "+sleep 5" \
  "+lua for _,c in ipairs(vim.lsp.get_clients({bufnr=0})) do print('LSP: '..c.name) end" +qa
```

應分別看到 `LSP: lua_ls` 與 `LSP: clangd`；`vim.diagnostic.get(0)` 在兩個檔案上都應是空表。

## 檔案

| 路徑 | 是什麼 | 版控 |
|---|---|---|
| `host.cpp` | C++ host：內嵌 Lua、綁 `arg`、UTF-8（含主控台碼頁）、跑腳本 | ✓ |
| `build.sh` | 建 build/{liblua.a, host.exe, lua.exe} | ✓ |
| `src/llm.lua` | ★ 接口本體 + 參數 schema（唯一真相源）+ `ask` 縫 + 串流 | ✓ |
| `src/cli.lua` | 由 schema 生成的 `--flag` CLI 薄殼（含 `--lua`／`--streaming`）| ✓ |
| `native/cjson.c` | ★ native（C）JSON codec；編進 liblua.a→內建 `require("cjson")`（取代原 rxi/json.lua）| ✓ |
| `native/http.c` | ★ native（C）HTTP 傳輸（WinHTTP／libcurl／file://）；內建 `require("http")`（取代 curl＋暫存檔）| ✓ |
| `src/_path.lua` | 路徑工具：定位專案根、組 fixture file:// URL（跨 CWD）| ✓ |
| `examples/demo_lua.lua` | ★ Lua 格式輸出示範（LLM 吐 table→原生 table）| ✓ |
| `examples/demo_stream.lua` | ★ 串流示範（streaming + callback + chunk_chars）| ✓ |
| `test/test.lua` | 離線 E2E（含 Lua 格式輸出 + 沙箱 + 串流案例）| ✓ |
| `test/sample_prompt.txt` | 中文 prompt fixture（給 lua.exe `--infile` debug 用）| ✓ |
| `test/fixtures/{fake,fake_lua,fake_stream}/` | 離線假回應（測試資料）| ✓ |
| `.vscode/` | 除錯 launch.json + 任務 tasks.json + IntelliSense settings.json | ✓ |
| `vendor/lua/*.c *.h` | vendored Lua 5.5.0 原始碼（`linit.c` 一處增修：把 cjson 註冊進 preload）| ✓ |
| `.luarc.json` | lua_ls 專案設定（nvim／VSCode 通用，見「編輯器整合」一節）| ✓ |
| `meta/cjson.lua`、`meta/http.lua` | cjson／http 兩個 native 模組的型別 stub（給 lua_ls 看）| ✓ |
| `compile_commands.json` | clangd 用的 compilation database，`build.sh` 每次執行自動重寫 | ✗ |
| `build/` | 產物：`liblua.a` / `host.exe` / `lua.exe` | ✗ |

## 刻意還沒做（等細規劃 / 待使用者）

- **接真後端實跑**：離線已綠，真回應需備環境（LM Studio 開 server / DeepSeek key）——見 [WAIT_USER](../WAIT_USER.md)。
- **daemon / 多輪對話 / `--metadata` 自述 / trace log**：跟 try_1 一樣先只做 one-shot，留給往 llm_forge 抽時再說。
- **~~Fennel~~（已放棄）**：try_2 就是純 Lua；想玩 Lisp 走 try_1（s7 線）。不上 Fennel。
- **把 C++ 核心再做厚**：JSON（`cjson.c`）＋HTTP 傳輸（`http.c`）已下沉 C。更進一步可讓 C 直接推原生 table（`lua_newtable`/`lua_setfield`）、retire 掉 Lua 端的 `cjson.decode` 那一跳——等瓶頸或整合需求出現再說。
- **三線整合**：日後把 try_1（s7）／try_2（Lua）／try_3（純 C++）整合（方向待定）。
