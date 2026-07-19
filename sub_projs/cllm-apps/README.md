# cllm-apps —— handy 多語言移植 + cllm 周邊整合

> 一頁看懂這個子專案是什麼、有哪些 app、彼此什麼關係、怎麼跑。
> 視覺速覽版見同目錄 [`overview.html`](overview.html)（瀏覽器開，回家掌握用）。
> 上層路由見 [../README.md](../README.md)。

---

## 一句話

**cllm-apps ＝把 [handy](../handy/AGENTS.md) 的四支工具（`llme`/`zhtw`/`wf`/`mail`）用各種語言重寫一遍**，並照本房房規整合 [cllm](../cllm/AGENTS.md) 的周邊工具（anthropic-proxy / llm-login）。真相源＝handy 的 **Fennel 原型**，移植版行為 **1:1 逐字對齊**。

## 兩群 app（別搞混）

這裡有兩批性質不同的東西，分別回答兩個問題：

### A. `*-handy/` —— 四工具的 shell-out 移植（主線）

「**同一套 handy 工具，換個語言長怎樣？**」把 `llme`/`zhtw`/`wf`/`mail` 以 **shell-out 方式**重寫：各語言只重寫 dispatcher／膠水邏輯，真正的「手」仍**轉呼外部命令**（`llm` cllm CLI、`claude`、同語言 sibling 工具）——**不用 cllm binding**。行為契約權威＝[`HANDY-PORT-SPEC.md`](HANDY-PORT-SPEC.md)。

| app | 語言 | 執行方式 | 狀態 |
|---|---|---|---|
| [python-handy/](python-handy/README.md) | Python（純 stdlib）| `#!/usr/bin/env python3` | ✅ 離線 dry-run 全綠 |
| [cpp-handy/](cpp-handy/README.md) | C++20/23（標準庫＋POSIX）| `build.sh` 編執行檔 | ✅ 離線 dry-run 全綠 |
| [lisp-handy/](lisp-handy/README.md) | Common Lisp（SBCL 2.6）| `#!/usr/bin/sbcl --script` | ✅ 離線 dry-run 全綠 |
| [janet-handy/](janet-handy/README.md) | Janet 1.41.2 | `#!/usr/bin/env janet` | ✅ 離線 dry-run 全綠 |

> 四支工具長什麼樣、各 env override、測試怎麼跑——每個 app 的 README 都有；共通契約全在 SPEC。

### B. `*-try-1/` —— 用 cllm binding 打 Anthropic 的最小應用（早期探索）

「**怎麼用 cllm binding ＋ 它的周邊工具打通一條真後端？**」比 `*-handy` 更早（探索期），各寫一支最小應用：透過 **cllm binding** 打 **Anthropic API**，整合 **anthropic-proxy**（轉發代理）＋ **llm-login**（帳號登入）。**本房的 `scripts/{up.sh,vendor.sh}` 房規就源自這裡**（尤其 `janet-try-1`）。

| app | 語言 | 備註 |
|---|---|---|
| [cpp-try-1/](cpp-try-1/README.md) | C++23 | 走 `<cllm/llm.hpp>` binding |
| [janet-try-1/](janet-try-1/README.md) | Janet | jpm 專案骨架；房規 `up.sh`/`vendor.sh` 藍本 |
| [lisp-try-1/](lisp-try-1/README.md) | Common Lisp（SBCL）| janet-try-1 的 CL 版 |
| [lua-try-1/](lua-try-1/README.md) | Lua | janet-try-1 的 Lua 版 |

## 四工具速記（契約細節見 [SPEC](HANDY-PORT-SPEC.md)）

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

- **llme** — 多 endpoint dispatcher：把 `<endpoint>` 翻成 `configs/<endpoint>.json`，轉呼 `llm --config …`；沒帶 `--api-key` 時依序找 `LLME_KEY_<EP>`／`<EP>_API_KEY` 自動注入。
- **zhtw** — 繁中翻譯薄包裝：烤死 deepseek＋翻譯人格＋取樣參數，轉呼 sibling `llme`。
- **wf** — 兩層任務派發器：啟發式關鍵詞（免 LLM）＞ LLM 分類，判 brain（llme 直接答）或 agent（claude 動手）；`-i` 走非同步 inbox。
- **mail** — inbox 協議：`send` 投信／`list` 列信／`run` 逐封 spawn claude 處理後歸檔 `done/`。

## 怎麼跑

### 離線 dry-run（不觸網，把外部命令換成 echo）—— 每個 app 通用

```sh
cd python-handy   # 或 cpp-handy / lisp-handy / janet-handy（cpp 需先 ./build.sh）
LLME_LLM=echo ./llme deepseek --stream hi           # → --config …/configs/deepseek.json --stream hi
DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./llme deepseek hi   # → 追加 --api-key FAKE
WF_LLME=echo WF_CLAUDE=echo ./wf 幫我修改 foo.py </dev/null   # [wf] 路由：agent（啟發式・免 LLM）
ZHTW_LLME=echo ./zhtw 測試                           # 轉呼 sibling llme 的完整形狀
./mail send 測試任務 ; ./mail list ; WF_CLAUDE=echo ./mail run   # 投遞→列出→歸檔 done/
```

### 真後端 DeepSeek

```sh
export DEEPSEEK_API_KEY=sk-...     # llme 自動注入 --api-key
./llme deepseek --stream 你好
./zhtw This is a pen
```

### 經 proxy 打 Anthropic（照房規：`scripts/{vendor.sh,up.sh}`）

```sh
# 前置（一次）：build cllm 三支 tool 並 vendored（可選）
cd ../cllm && cmake --preset linux-debug && cmake --build --preset linux-debug
cd ../cllm-apps/python-handy && ./scripts/vendor.sh   # 複製 anthropic-proxy/llm-login/liblogin.so
export ANTHROPIC_API_KEY=sk-ant-...
./scripts/up.sh 用一句話介紹你自己                    # 起 proxy sidecar + 備憑證 + exec llme anthropic
```

## 房規（每個 `<lang>-handy/` 都遵守）

- **shell-out，不 link libcllm**：真正的「手」轉呼 `llm`/`claude`/sibling，各語言只寫膠水。
- **sibling 解析**：工具互呼（zhtw→llme、wf→llme/mail）預設解到**同語言同層 sibling**，對應 env（`ZHTW_LLME`/`WF_LLME`/`WF_MAIL`）可覆寫。
- **configs/**：`deepseek.json`（keyless）＋`anthropic.json`（指本機 anthropic-proxy）＋`_template.json`；只放 Client 欄位、無註解鍵（glaze 嚴格）。真憑證 `config.json` 不進版控。
- **scripts/**：`vendor.sh`（vendored cllm tools 進 `vendor/cllm-tools/`）＋`up.sh`（起 proxy sidecar＋備憑證＋`exec` 本 app 的 `llme anthropic`）。
- **build/vendor/.run 一律 gitignore、可 regen**。

## 與 handy Fennel 原型的差異（共通）

- **sibling 解析**：原型 zhtw／wf 對 llme 寫死裸名（靠 PATH）；移植版預設解同層 sibling、env 可覆寫。
- 其餘（`AGENT-PATS`/`BRAIN-PATS` 關鍵詞、`CLASSIFY-SYS`、zhtw 的 `SYSTEM`/`PREFIX`/`FLAGS`、mail 信件模板、各用法字串、exit code 透傳、api-key 注入規則、stdin 非-tty 才讀）皆與 Fennel 原型逐字對齊。各語言的細微偏差記在該 app 的 README。

## 結構

```
HANDY-PORT-SPEC.md   四工具行為契約 + 房規（權威，移植前先讀）
README.md            本檔（總覽入口）
overview.html        視覺速覽（回家掌握用）
python-handy/        ┐
cpp-handy/           │ 四工具 shell-out 移植（主線，A 群）
lisp-handy/          │
janet-handy/         ┘
cpp-try-1/           ┐
janet-try-1/         │ cllm binding 打 Anthropic 最小應用（早期探索，B 群）
lisp-try-1/          │
lua-try-1/           ┘
```

## 設計脈絡

- **四工具真相源**＝[handy](../handy/AGENTS.md) 的 Fennel 原型（`../handy/{llme,zhtw,wf,mail}`）。
- **cllm 周邊**（anthropic-proxy / llm-login / `llm` CLI / binding）＝[cllm](../cllm/AGENTS.md)。
- 本專案 2026-07-19 由 `~/code/cllm-apps`（版控外工作區）納入 repo 統一管理。
