# janet-handy —— handy 工具集的 Janet 移植

把 `~/repo/ai_core/sub_projs/handy` 的四支工具（**llme / zhtw / wf / mail**）以
**shell-out 方式**用 Janet（1.41.2）重寫，行為 1:1 對齊 Fennel 原型，並照 cllm-apps
房規整合 cllm 周邊工具（anthropic-proxy / llm-login）。

只重寫「dispatcher／膠水邏輯」，真正的「手」仍轉呼外部命令（`llm` cllm CLI、`claude`、
以及**同語言 sibling 工具**）。不 link libcllm、不用任何 cllm binding——**純 Janet 內建**
（`os/getenv`／`os/realpath`／`os/execute`／`os/spawn`／`slurp`／`spit`／`os/dir`）。
共用膠水碼抽到 `src/util.janet`，各工具頂層 `dofile` 併入自身環境。

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
llme  zhtw  wf  mail          四支可執行工具（#!/usr/bin/env janet，chmod +x）
src/
  util.janet                  四工具共用膠水碼（shquote／script-dir 解 symlink／run-shell／
                              capture-shell／檔案 I/O／stdin 守衛／list-stems／env-stem／
                              utf8-head／trim），各工具頂層 dofile 併入自身環境
configs/
  deepseek.json               DeepSeek endpoint（keyless；timeout_ms=120000）
  anthropic.json              指向本機 anthropic-proxy（127.0.0.1:8787）的 endpoint
  _template.json              新 endpoint 範本（`_` 開頭＝llme 不列入可用 endpoint）
inbox/                        mail 的信箱（runtime 信件 *.md 不進版控）
  .gitignore（*.md + !.gitkeep）  .gitkeep   done/.gitkeep
scripts/
  vendor.sh                   把 cllm tools 的 build 產物 vendored 進 vendor/cllm-tools/
  up.sh                       起 proxy sidecar + 備憑證，然後 exec 本 app 的 `llme anthropic`
vendor/cllm-tools/            vendored 二進位（vendor.sh 產出，gitignored）
```

## 執行方式（shebang / 相依）

四支工具都用 `#!/usr/bin/env janet` 直跑——不需編譯、不需 jpm，只要 PATH 有
`janet`（本專案驗於 Janet 1.41.2）。每支開頭先用 `(os/realpath (get (dyn :args) 0))`
解出本檔真實所在目錄（**解 symlink**），再 `dofile src/util.janet` 把共用碼併進自身環境。
runtime 另需 PATH 有 `llm`（cllm CLI）與 `claude`，但離線 dry-run 可用 env 把它們換成
`echo`／`false`（見下）。

> **執行模型**：與 Fennel 原型一致，各工具組出「單一 shell 命令字串」（每個參數以
> `shquote` 單引號轉義），交給 `/bin/sh -c` 跑（`run-shell`／`capture-shell`），並透傳
> 子命令 exit code。`shquote` 因此是真・load-bearing——`wf -i` 的 `printf … | mail send`
> 管線也是靠這條 shell 字串路徑天然表達。
>
> **UTF-8**：Janet 字串是位元組串，argv／檔名／stdout／stderr／信件內容全程原樣保留
> 位元組、與 locale 無關——已驗於 UTF-8 locale 與 `LC_ALL=C` 兩種環境皆不亂碼、不切半字。

## 怎麼跑

### 1. 離線 dry-run（不觸網，把外部命令換成 echo）

```sh
# llme：轉發形狀（DEEPSEEK_API_KEY 若已在 shell env 會被自動注入，測乾淨形狀先 unset）
env -u DEEPSEEK_API_KEY LLME_LLM=echo ./llme deepseek --stream hi
#   → --config …/configs/deepseek.json --stream hi
DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./llme deepseek hi
#   → --config …/configs/deepseek.json --api-key FAKE hi
LLME_LLM=echo ./llme nope        # 找不到 endpoint → exit 2、列可用 endpoint

# wf：路由決策（啟發式免 LLM）
WF_LLME=echo WF_CLAUDE=echo ./wf 幫我修改 foo.py </dev/null   # [wf] 路由：agent（啟發式・免 LLM）
WF_LLME=echo WF_CLAUDE=echo ./wf 解釋什麼是 mmap  </dev/null   # [wf] 路由：brain（啟發式・免 LLM）
WF_LLME=echo WF_CLAUDE=echo ./wf -i 幫我改 foo.py </dev/null   # 投遞 inbox（-i）

# mail：投遞 / 列出 / 處理歸檔
./mail send 測試任務
./mail list
WF_CLAUDE=echo  ./mail run          # 成功 → mv 到 done/、印 ✓
WF_CLAUDE=false ./mail run <slug>   # 失敗 → 保留 inbox、印 ✗、exit 1

# zhtw：薄包裝轉呼 sibling llme
ZHTW_LLME=echo ./zhtw 測試          # → deepseek --temperature 0.2 … --system … -- 翻譯以下內容：\n測試
```

### 2. 真後端 DeepSeek

```sh
export DEEPSEEK_API_KEY=sk-...     # llme 自動注入 --api-key
./llme deepseek --stream 你好
./zhtw This is a pen              # 走 deepseek 翻成繁中
```

### 3. 經 proxy 打 Anthropic（照房規）

```sh
# 前置（一次）：build cllm 三支 tool，並 vendored（可選）
cd ~/repo/ai_core/sub_projs/cllm && cmake --preset linux-debug && cmake --build --preset linux-debug
cd ~/repo/ai_core/sub_projs/cllm/apps/janet-handy && ./scripts/vendor.sh   # 複製 anthropic-proxy/llm-login/liblogin.so

# 起 proxy sidecar + 備憑證 + 用本 app 的 llme anthropic 跑
export ANTHROPIC_API_KEY=sk-ant-...
./scripts/up.sh 用一句話介紹你自己
./scripts/up.sh --stream 寫一首短詩
```

`up.sh` 做三件事：① 確保 anthropic-proxy sidecar 在跑（TCP health-check）② 備好憑證
（`ANTHROPIC_API_KEY` ＞ `llm-login token`）③ `exec` 本 app 的 `llme anthropic "$@"`——
把 proxy endpoint（`configs/anthropic.json`）當一個 llme endpoint 用，憑證則透過 llme 的
auto-inject 慣例（設 `ANTHROPIC_API_KEY` → llme 自動補 `--api-key`）帶進去。`scripts/`
與 lisp-handy／python-handy 版本是語言無關的（都 `exec "$HERE/llme"`），逐字共用。

> ⚠ `llm-login` 目前沒有 anthropic OAuth provider，`MODE=anthropic` 的憑證是「貼 sk-ant key」，
> 非 OAuth 帳號登入。要真 OAuth 且觸及 Claude 走 `MODE=openrouter`。

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
  `WF_MAIL`(sibling mail；`-i` 模式轉呼)、`WF_PERMISSION`(acceptEdits)、`WF_MODEL`、
  `WF_EFFORT`、`WF_SYSTEM`(預設繁中溝通指示，設空字串關掉)。
- **mail**：`INBOX_DIR`（信箱）、`WF_CLAUDE`／`WF_PERMISSION`（與 wf 共用）。

## sibling 解析

`zhtw→llme`、`wf→llme/mail` 預設解到**同語言同層 sibling**（`<script-dir>/llme` 等），
env 可覆寫（`ZHTW_LLME`／`WF_LLME`／`WF_MAIL`）。`<script-dir>` 由 `os/realpath` 解 symlink
取得，因此即使工具被 symlink 進 PATH，仍找得到真實所在目錄的 sibling 與 `configs/`。

## 與 Fennel 原型的差異

1. **sibling 解析**：原型 zhtw／wf 對 llme 寫死裸名 `"llme"`（靠 PATH）；本移植版**預設解到
   同層 sibling**（`<script-dir>/llme`），並讓對應 env（`ZHTW_LLME`／`WF_LLME`／`WF_MAIL`）可覆寫。
   `WF_MAIL` 是本移植新增（照 sibling 可覆寫原則），原型 wf→mail 本就是同層 sibling。
   （與 python-handy／lisp-handy 一致。）
2. **script-dir 解 symlink**：原型 Fennel 的 `script-dir` 只做 abspath＋dirname、**不**解
   symlink；本移植版用 `os/realpath` 解 symlink（與 python-handy `Path.resolve()`／
   lisp-handy `*load-truename*` 一致），symlink 進 PATH 也能定位 configs／sibling。
3. **共用碼載入**：原型每檔各自定義 shquote/trim 等；本移植版抽進 `src/util.janet`，各工具
   頂層 `(dofile … :env (curenv))` 併入——靠 realpath 定位，symlink 亦可。
4. **endpoint／信件列出排序**：原型借 `ls -1`（locale 排序）；本移植版用 `os/dir` + `sort`
   （穩定字典序，結果更 deterministic，內容一致）。
5. **zhtw 的 stdin**：與 Fennel 原型一致，無位置參數時**無條件讀 stdin**（不做 tty 檢查）——
   與 lisp-handy 相同（照原型）；python-handy 則加了 tty 守衛。wf／mail 兩者皆照原型做
   `test -t 0` 非-tty 才讀。

行為（`AGENT-PATS`／`BRAIN-PATS` 關鍵詞、`CLASSIFY-SYS`、zhtw 的 `SYSTEM`／`PREFIX`／`FLAGS`、
mail 信件模板、各用法字串、exit code 透傳、api-key 自動注入規則、slugify、stdin 守衛）
皆與 Fennel 原型逐字對齊。

## 測試

以下「離線 dry-run」全驗過綠（不觸網、不 spawn 真 claude；Janet 1.41.2）。實測關鍵輸出：

**llme**
```
$ env -u DEEPSEEK_API_KEY LLME_LLM=echo ./llme deepseek --stream hi
--config …/configs/deepseek.json --stream hi
$ DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./llme deepseek hi
--config …/configs/deepseek.json --api-key FAKE hi
$ DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./llme deepseek --api-key USERKEY hi   # 使用者自帶不覆寫
--config …/configs/deepseek.json --api-key USERKEY hi
$ DEEPSEEK_API_KEY=LOW LLME_KEY_DEEPSEEK=HIGH LLME_LLM=echo ./llme deepseek hi   # LLME_KEY_ 優先
--config …/configs/deepseek.json --api-key HIGH hi
$ LLME_LLM=echo ./llme nope   # exit 2、列可用 endpoint：anthropic deepseek
```

**wf**（stderr 一行路由 + stdout 轉發形狀）
```
$ WF_LLME=echo WF_CLAUDE=echo ./wf 幫我修改 foo.py </dev/null
[wf] 路由：agent（啟發式・免 LLM）
-p --permission-mode acceptEdits --append-system-prompt 與使用者溝通時用繁體中文。 -- 幫我修改 foo.py
$ WF_LLME=echo WF_CLAUDE=echo ./wf 解釋什麼是 mmap </dev/null
[wf] 路由：brain（啟發式・免 LLM）
deepseek --stream --system 與使用者溝通時用繁體中文。 -- 解釋什麼是 mmap
$ WF_LLME=echo WF_CLAUDE=echo ./wf -i 幫我改 foo.py </dev/null
[wf] 路由：inbox（強制）
已投遞：…/inbox/幫我改-foo.py.md
```

**mail**
```
$ ./mail send "幫我把這個很長的中文任務名稱截斷測試看看有沒有切半個字"
已投遞：…/inbox/幫我把這個很長的中文任務名稱截斷.md    # UTF-8 取前 16 字、不切半字
$ ./mail send "幫我把這個很長的中文任務名稱截斷測試看看有沒有切半個字"
已投遞：…/inbox/幫我把這個很長的中文任務名稱截斷-2.md   # 撞名補 -2
$ echo 這是上下文 | ./mail send "處理這個/檔案*測試"      # slug 去 / *，body 保留原文
已投遞：…/inbox/處理這個檔案測試.md
$ WF_CLAUDE=echo  ./mail run   # ✓ 完成，歸檔 → done/…；exit 0
$ WF_CLAUDE=false ./mail run   # ✗ claude 退出碼 1，保留在 inbox；exit 1
$ ./mail run --dry             # [--dry] 略過不跑
```

**zhtw**
```
$ ZHTW_LLME=echo ./zhtw 測試
deepseek --temperature 0.2 --top-p 0.9 --max-tokens 4096 --system 你是專業翻譯引擎。… -- 翻譯以下內容：
測試
```

**UTF-8**：以上 argv／檔名／輸出／信件內容於 UTF-8 locale 與 `LC_ALL=C` 皆驗證正確
（Janet 位元組串與 locale 無關）。**symlink**：把 `llme`／`zhtw` symlink 進臨時目錄後執行，
仍正確解到真實 `configs/` 與 sibling `llme`。
