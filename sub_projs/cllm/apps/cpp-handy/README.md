# cpp-handy —— handy 工具集的 C++ 移植

把 `~/repo/ai_core/sub_projs/handy` 的四支工具（**llme / zhtw / wf / mail**）以
**shell-out 方式**用 C++（C++20/23、只用標準庫＋POSIX）重寫，行為 1:1 對齊 Fennel 原型，
並照 cllm-apps 房規整合 cllm 周邊工具（anthropic-proxy / llm-login）。

只重寫「dispatcher／膠水邏輯」，真正的「手」仍以 `std::system` / `popen` 轉呼外部命令
（`llm` cllm CLI、`claude`、以及**同語言 sibling 工具**）。不 link libcllm、不需 cllm 開發環境、
不裝任何第三方套件——只要一支 C++20/23 編譯器（本機驗過 g++ 16 / clang++ 22）。

## 架構

```
  zhtw ─┐                                            ┌─▶ llm (cllm CLI) ──▶ endpoint
        ├─▶ llme <endpoint> ──(--config <cfg>.json)──┤
  wf ───┤        （多 endpoint dispatcher）           └─▶（configs/anthropic.json）
        │                                                    │ OpenAI+Bearer
  wf -a ┼─▶ claude -p（headless agent，當場動手）              ▼
  wf -i ┘                                            anthropic-proxy(127.0.0.1:8787)
        └─▶ mail send ──▶ inbox/<slug>.md                     │ x-api-key
                  mail run ──▶ claude -p ──▶ done/             ▼
                                                        api.anthropic.com
```

- **llme** 是核心 dispatcher：把 `<endpoint>` 翻成 `configs/<endpoint>.json`，轉呼 `llm --config …`。
- **zhtw** 站在 llme 上，把 endpoint／取樣參數／人格烤死＝薄包裝範例。
- **wf** 是兩層任務派發器：啟發式關鍵詞（免 LLM）＞ LLM 分類，判 brain（llme 直接答）
  或 agent（claude 動手）；`-i` 走非同步 inbox。
- **mail** 是 inbox 協議：`send` 投信、`list` 列信、`run` 逐封 spawn claude 處理後歸檔 `done/`。

## 結構

```
src/
  util.hpp                    四工具共用膠水：shquote / script_dir / app_root /
                              run_system / capture / trim / utf8_head / stdin_piped …
  llme.cpp  zhtw.cpp          四支工具本體（每支獨立 translation unit，各自 include util.hpp）
  wf.cpp    mail.cpp
build.sh                      編四支 → bin/{llme,zhtw,wf,mail}（CXX / STD 可覆寫）
bin/                          編出的執行檔（gitignored）
configs/
  deepseek.json               DeepSeek endpoint（keyless；timeout_ms=120000）
  anthropic.json              指向本機 anthropic-proxy（127.0.0.1:8787）的 endpoint
  _template.json              新 endpoint 範本（`_` 開頭＝llme 不列入可用 endpoint）
inbox/                        mail 的信箱（runtime 信件 *.md 不進版控）
  .gitignore（*.md + !.gitkeep）  .gitkeep   done/.gitkeep
scripts/
  vendor.sh                   把 cllm tools 的 build 產物 vendored 進 vendor/cllm-tools/
  up.sh                       起 proxy sidecar + 備憑證，然後 exec 本 app 的 bin/llme anthropic
vendor/cllm-tools/            vendored 二進位（vendor.sh 產出，gitignored）
```

## 怎麼跑

### 0. 編譯

```sh
./build.sh                 # → bin/llme bin/zhtw bin/wf bin/mail
CXX=clang++ ./build.sh     # 換編譯器（g++ 16 / clang++ 22 皆驗過）
STD=c++20 ./build.sh       # 換標準（預設 c++23）
```

`build.sh` 對每支工具跑 `$CXX -std=$STD -O2 -Isrc src/<tool>.cpp -o bin/<tool>`。純 shell-out，
不需 pkg-config／libcllm。

### 1. 離線 dry-run（不觸網，把外部命令換成 echo）

```sh
# llme：轉發形狀
LLME_LLM=echo ./bin/llme deepseek --stream hi
#   → --config …/configs/deepseek.json --stream hi
DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./bin/llme deepseek hi
#   → --config …/configs/deepseek.json --api-key FAKE hi
LLME_LLM=echo ./bin/llme nope        # 找不到 endpoint → exit 2、列可用 endpoint

# wf：路由決策（啟發式免 LLM）
WF_LLME=echo WF_CLAUDE=echo ./bin/wf 幫我修改 foo.py   # [wf] 路由：agent（啟發式・免 LLM）
WF_LLME=echo WF_CLAUDE=echo ./bin/wf 解釋什麼是 mmap    # [wf] 路由：brain（啟發式・免 LLM）
printf 'x\n' | WF_CLAUDE=echo ./bin/wf -i 幫我改 foo.py # 投遞 inbox（-i）

# mail：投遞 / 列出 / 處理歸檔
./bin/mail send 測試任務
./bin/mail list
WF_CLAUDE=echo ./bin/mail run          # 成功 → mv 到 done/、印 ✓
WF_CLAUDE=false ./bin/mail run <slug>  # 失敗 → 保留 inbox、印 ✗、exit 1

# zhtw：薄包裝轉呼 sibling llme（ZHTW_LLME=echo 看轉出形狀）
ZHTW_LLME=echo ./bin/zhtw 測試         # → deepseek --temperature 0.2 … --system … -- 翻譯以下內容：\n測試
```

### 2. 真後端 DeepSeek

```sh
export DEEPSEEK_API_KEY=sk-...     # llme 自動注入 --api-key
./bin/llme deepseek --stream 你好
./bin/zhtw This is a pen           # 走 deepseek 翻成繁中
```

### 3. 經 proxy 打 Anthropic（照房規）

```sh
# 前置（一次）：build cllm 三支 tool，並 vendored（可選）
cd ~/repo/ai_core/sub_projs/cllm && cmake --preset linux-debug && cmake --build --preset linux-debug
cd ~/code/cllm-apps/cpp-handy && ./scripts/vendor.sh   # 複製 anthropic-proxy/llm-login/liblogin.so

# 起 proxy sidecar + 備憑證 + 用本 app 的 bin/llme anthropic 跑
export ANTHROPIC_API_KEY=sk-ant-...
./scripts/up.sh 用一句話介紹你自己
./scripts/up.sh --stream 寫一首短詩
```

`up.sh` 做三件事：① 確保 anthropic-proxy sidecar 在跑（TCP health-check；沒編過會先 `build.sh`）
② 備好憑證（`ANTHROPIC_API_KEY` ＞ `llm-login token` ＞ `configs/anthropic.json`）
③ `exec` 本 app 的 `bin/llme anthropic "$@"`——把 proxy endpoint（`configs/anthropic.json`）當一個
llme endpoint 用，憑證則透過 llme 的 auto-inject 慣例（設 `ANTHROPIC_API_KEY` → llme 自動補 `--api-key`）帶進去。

> ⚠ `llm-login` 目前沒有 anthropic OAuth provider，`MODE=anthropic` 的憑證是「貼 sk-ant key」，
> 非 OAuth 帳號登入。要真 OAuth 且觸及 Claude 走 `MODE=openrouter`（需 `configs/openrouter.json`）。

## 四工具契約摘要

| 工具 | 用法 | 行為 |
|--|--|--|
| `llme` | `llme <endpoint> [llm 參數...]` | 翻 endpoint→`configs/<ep>.json`，轉呼 `llm --config …`；未帶 `--api-key` 時依序找 `LLME_KEY_<EP>`／`<EP>_API_KEY` 自動注入 |
| `zhtw` | `zhtw <文字...>` 或 `… \| zhtw` | 烤死 deepseek＋翻譯人格＋取樣參數，轉呼 sibling `llme` |
| `wf` | `wf [-b\|-a\|-i] <任務...>` | 啟發式關鍵詞（免 LLM）＞ llme 分類，判 brain/agent；`-i` 轉 `mail send` |
| `mail` | `mail <send\|list\|run>` | inbox 協議：投信／列信／逐封 spawn claude 處理後歸檔 `done/` |

### 環境變數

- **llme**：`LLME_LLM`（llm 執行檔，測試設 echo）、`LLME_CONFIG_DIR`（config 目錄）。
- **zhtw**：`ZHTW_LLME`（llme 執行檔；預設同層 sibling）。
- **wf**：`WF_ENDPOINT`(deepseek)、`WF_LLME`(sibling llme)、`WF_CLAUDE`(claude)、
  `WF_PERMISSION`(acceptEdits)、`WF_MODEL`、`WF_EFFORT`、`WF_SYSTEM`(預設繁中溝通指示，設空字串關掉)。
- **mail**：`INBOX_DIR`（信箱）、`WF_CLAUDE`／`WF_PERMISSION`（與 wf 共用）。

> 註：`bin/` 佈局下，configs/ 與 inbox/ 在 app 根、執行檔在 `bin/`。llme／mail 用 `app_root()`
> （偵測到自己在 `bin/` 就退一層）找 configs/inbox；sibling 互呼（zhtw→llme、wf→llme/mail）
> 則用 `script_dir()`（同在 `bin/` 內同層）。

## 與 Fennel 原型的差異

1. **sibling 解析**：原型 zhtw／wf 對 llme 寫死裸名 `"llme"`（靠 PATH）；本移植版**預設解到同層 sibling**
   （`<script-dir>/llme`），並讓對應 env（`ZHTW_LLME`／`WF_LLME`）可覆寫。wf→mail 原型本就是同層 sibling，維持不變。
2. **stdin non-tty 守衛**：原型 zhtw 無位置參數時無條件讀 stdin；本移植版照 SPEC 通用約定，
   **只在 stdin 非 tty（被 pipe／重導）時才讀**，避免互動情境卡等 EOF（wf／mail 原型本就有此守衛）。
3. **shell 轉義**：`util::shquote` 手寫（`'` → `'\''`、整體單引號包住），與原型 Lua `shquote` 逐字等價。
4. **script-dir 解 symlink**：走 `/proc/self/exe`（`readlink`）取真實執行檔路徑，取代原型的
   `PWD`＋`arg[0]` 拼接；更穩健（symlink 進 PATH 也定位得到）。
5. **slug UTF-8 安全取前 16 字**：`util::utf8_head` 依 UTF-8 續接位元組（`0x80` 高位）數 code point、
   取滿 16 個字元就停，不切半個字；對齊原型的 `utf8.offset` 語義。
6. **lower/upper**：`ascii_lower`／`ascii_upper` 只動 A–Z、非 ASCII 位元組原樣，比照 Lua 於 C locale
   的 `string:lower`／`:upper`（啟發式關鍵詞比對、classify 輸出正規化皆賴此語義）。

行為（`AGENT-PATS`／`BRAIN-PATS` 關鍵詞、`CLASSIFY-SYS`、zhtw 的 `SYSTEM`／`PREFIX`／`FLAGS`、
mail 信件模板、各用法字串、exit code 透傳、api-key 自動注入規則）皆與 Fennel 原型逐字對齊。

## 測試

以上「離線 dry-run」章節的每一條都驗過綠（不觸網、不 spawn 真 claude，g++ 16 / clang++ 22 編過）：
llme 轉發形狀與 api-key 注入、找不到 endpoint exit 2＋列 endpoint、wf agent/brain 啟發式路由、
wf 模稜兩可 fallback 到 llme 分類、wf -i 投遞 inbox（含 pipe 上下文分欄）、mail
send/list/run(--dry)/run 歸檔 done/、失敗保留＋exit 1、撞名補 `-2`、slug 中文 16 字不切半、
zhtw 站在 sibling llme 上的轉發形狀＋非 tty 空輸入 exit 2。
