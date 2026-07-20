# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。
> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**。完成即整條刪除。

## 最新進度

- **〔真後端矩陣〕OpenRouter 已驗綠、其餘 OpenAI 相容雲端待驗**（2026-07-20，Windows `build/llm.exe`）：OpenRouter 非串流＋`--stream` 兩路 exit 0（`tencent/hy3:free`），順帶驗到真後端錯誤路徑（`:free` slug 帳號 gated→HTTP 404 被 `guard_backend_error` 正確攔成結構化錯誤）；durable 細節＋config 已固化進 [README「接真後端」](README.md#接真後端本機-lm-studio雲端-openrouter)。**open 尾巴**：OpenAI／DeepSeek 直連、Gemini（OpenAI 相容層）、Anthropic（走 proxy）皆**協定通、未對真後端打過**——每接一家新的都該真打一次才算數（[gotchas/backend](workflows/common/gotchas/backend.md)）。
- **〔open bug〕Windows Python binding：`media`＋`modalities` 同傳→`llm_ask` 存取違規崩潰**（2026-07-20，mingw64 g++16.1.0＋Python 3.14）：各自單獨呼叫皆 OK、合起來才崩（`access violation reading 0x9`）；ask／stream／schema／tool／media-out 全綠。已排除 `c_long`/LLP64（counts 皆 `c_size_t`），真根因待 gdb。同輪已治另兩坑（DLL 改 `-static` 自足免 libwinpthread、`LIBCLLM` 覆寫 `.so`→`.dll`），詳見 [gotchas/windows](workflows/common/gotchas/windows.md)。cpp／python 兩綁定的 Windows 庫路徑已驗（cpp 六能力綠、python 五綠一崩）。

- **新增 Janet 綁定（第十語言）**（2026-07-18）：`bindings/janet/`（`llm.c`＋`build.sh`＋`example.janet`＋`smoke.sh`＋`project.janet`）。**採 native C 模組、非純 FFI**——Janet 的 `ffi/trampoline` 把回呼簽名寫死成 `void(void*,void*)`，表達不了 cllm 的 `int on_text(const char*,size_t,void*)`（三參數＋int 回傳），故跟 lua/s7 同型態走 C 橋接：C 端實作 on_text/on_tool/on_media/on_error，內部 `janet_pcall` 回呼 JanetFunction（回真值＝中止；回呼 janet error 已被 pcall 收住，安全），文字用 C buffer 累積、opts 全程 gcroot 護住借出的 Janet 字串。API `(llm/ask prompt [endpoint] :key val …)`、hyphen keyword（`:on-delta`／`:api-key`／`:max-tokens`），對齊其它語言能力面（ask／串流／schema／tools／media／modalities）。`build.sh` 直接 gcc（`JANET_INC` 預設 `~/.local/include/janet`，不必 jpm）。**install-dev.sh 已加 Janet 段**（`[4c]` 編 `llm.so`→`$PREFIX/lib/janet/`；env.sh 新增 `JANET_PATH="$DEV/lib/janet:<janet syspath>"`——`:` 分隔多根，前者放模組、後者留給 example 的 `spork/json`／`spork/sh`），重跑 install 冪等、從乾淨 shell 任意 cwd `(import llm)` 過。**驗證全綠**：`bindings_smoke.sh` 從九→**十語言 PASS=10 FAIL=0**；lab `~/code/cllm-lab/janet/`（`play.janet`＋`run.sh`＋`project.janet`）從乾淨 shell 跑 7/7 標記對齊其它語言（`[janet] json => name=星野 affection=42 lines=2` …）。文檔已同步：lab README/GUIDE、bindings/README、INDEX。**open 尾巴**：未在 Windows 實測（同其它綁定）。
- **兩支周邊工具（anthropic-proxy＋llm-login）C++ 重寫成 cllm 模組**（2026-07-17，git log 025065f/187ab65）：從 Python 重寫成純 C++23、進主建置（`-DCLLM_BUILD_TOOLS`，預設 ON）、重用 `src/http` 出站＋自寫 [`common/httpd`](tools/common/httpd.hpp) 微型 server 入站＋glaze json_t，**零新依賴**（server／PKCE SHA-256 都 vendored）。**anthropic-proxy**（執行檔）＝OpenAI⇄Anthropic 轉發代理，讓 cllm 直連 Anthropic（cllm 說 Bearer＋OpenAI、Anthropic 收 x-api-key＋Messages，中間翻譯；無狀態、金鑰轉手不落地）；端對端假上游驗過（非串流／串流／[DONE]／缺 key→401／x-api-key＋version 帶對）。**llm-login**（`liblogin.so` C-ABI [`login_abi.h`](tools/llm-login/login_abi.h)＋`llm-login` CLI，就跟 cllm 一樣的模組形態）＝OAuth 授權碼＋PKCE 登入 patch cllm config；**C-ABI 為與 cllm 聯動而生**——cllm 401→harness 呼 `llm_login_login`→自動開瀏覽器→patch config→重試，**cllm 零改動**；離線測 24 條＋免瀏覽器整合測（接 callback→換 token→存檔→patch）全綠。Python 原版封存各 `tools/*/reference/`（保 git 史、對照參考）。**cllm 自身 smoke 仍 31/31**。供應商矩陣（能接：openrouter[免註冊 client、最省事]／gemini／azure／github；接不了：deepseek/openai 直填 key、anthropic 走本 proxy）詳見 [tools/README](tools/README.md)。**install 尾巴已收（2026-07-17）**：`cmake --install` 現裝 `liblogin.so`＋`login_abi.h`（→ `include/cllm`）＋`llm-login.pc`＋`llm-login` CLI，沿用 cllm 同一套 configure_file→.pc 機制（帶 INSTALL_RPATH）；下游 `pkg-config --cflags --libs llm-login` → `-I/-L/-llogin` 即可 link。實測：裝到臨時 prefix→最小下游 C 消費端純靠 pkg-config 編譯連結、從乾淨 cwd 跑起、`ldd` 命中裝好的 `.so`（rpath，免 `LD_LIBRARY_PATH`）＋三個 C-ABI 符號 `nm -D` 皆匯出。用法段見 [tools/llm-login/README「下游如何 link」](tools/llm-login/README.md)。**聯動 harness demo 尾巴已收（2026-07-17）**：[`tools/llm-login/demo_harness.cpp`](tools/llm-login/demo_harness.cpp)＋target `llm-login-demo-harness`，用假上游（假 LLM 後端回 401／假 OAuth token 端換 `GOOD-TOKEN`）＋假瀏覽器（另 thread 打本機 callback）把整條「cllm 401→攔→`llm_login_login`→patch config→重試成功」跑通，**cllm 一行不動**（子行程 exec 出貨 `llm`、只看 exit/stderr）；實跑 7 條全綠、exit 0、重跑 3× 穩定。回歸：llm-login 離線測全綠、cli_smoke 仍 31/31。**open 尾巴（1 項）**：**Windows 實測**（socket/bcrypt 已 `#ifdef` 包好、POSIX 已測、未在 Windows 建）。真 OAuth／真 Anthropic 往返 → [WAIT_USER](WAIT_USER.md)。
- **八語言綁定全路收齊＋一鍵 smoke**（2026-07-16）：全語言（C／C++／Lua／Fennel／s7／Python／CL／Go＋Shell）都綁了 `tools`／`media`／`modalities`＋`on_tool`／`on_media`；每語言各有 `smoke.sh`、[`test/bindings_smoke.sh`](test/bindings_smoke.sh) 一鍵輪跑（9/9 綠）。C++ 另有便利層 `llm.hpp`（聚合 ask／`std::expected`／串流糖／media helpers）＋`llm_reflect.hpp`（`ask_as<T>`／`make_tool<Args>`／`args_as<Args>`／`modality<Config>`）。常駐前綴改 **`~/dev`**（共用目錄、install 冪等覆蓋、勿整個 rm）；s7 原始碼 vendor 進 repo（`bindings/s7/vendor/`），不再依賴 pas。**真後端已驗**（2026-07-16，LM Studio）：`ask_as<T>` required 全吐／tools（C++＋Python）／vision／錯誤路徑／SSE 串流全過；modalities 被靜默忽略（記 [gotchas/backend](workflows/common/gotchas/backend.md)）。**open 尾巴**：① 未在 Windows 實測；② 未接主 CMake（刻意獨立）。詳見 [bindings/README](bindings/README.md)。
- **CLI 打磨第一輪已落地**（2026-07-16，詳見 git log）：管線體驗（stdin×位置參數合體、「-」插入點）＋tools/modalities/media-out 旗標（四路 handler 全開，新 TU `cli_output.*`）＋錯誤訊息打磨，cli_smoke 31/31、真後端驗過（stdin 合體＋`--tool` 打 LM Studio）。**open**：下一輪打磨方向待使用者再點（便利層方向已評估為不值得，撤）。CLI 用法見 [docs/cli-manual.md](docs/cli-manual.md)、檔對應見 [code-map ⑤](workflows/common/code-map/CODE_MAP.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|
| （尚無工作流長出自己的 session-log）| | |

## 不屬任何工作流的進度

- （無）
