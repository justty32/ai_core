# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾按類別拆檔（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。
> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**。完成即整條刪除。

## 最新進度

- **兩支周邊工具（anthropic-proxy＋llm-login）C++ 重寫成 cllm 模組**（2026-07-17，git log 025065f/187ab65）：從 Python 重寫成純 C++23、進主建置（`-DCLLM_BUILD_TOOLS`，預設 ON）、重用 `src/http` 出站＋自寫 [`common/httpd`](tools/common/httpd.hpp) 微型 server 入站＋glaze json_t，**零新依賴**（server／PKCE SHA-256 都 vendored）。**anthropic-proxy**（執行檔）＝OpenAI⇄Anthropic 轉發代理，讓 cllm 直連 Anthropic（cllm 說 Bearer＋OpenAI、Anthropic 收 x-api-key＋Messages，中間翻譯；無狀態、金鑰轉手不落地）；端對端假上游驗過（非串流／串流／[DONE]／缺 key→401／x-api-key＋version 帶對）。**llm-login**（`liblogin.so` C-ABI [`login_abi.h`](tools/llm-login/login_abi.h)＋`llm-login` CLI，就跟 cllm 一樣的模組形態）＝OAuth 授權碼＋PKCE 登入 patch cllm config；**C-ABI 為與 cllm 聯動而生**——cllm 401→harness 呼 `llm_login_login`→自動開瀏覽器→patch config→重試，**cllm 零改動**；離線測 24 條＋免瀏覽器整合測（接 callback→換 token→存檔→patch）全綠。Python 原版封存各 `tools/*/reference/`（保 git 史、對照參考）。**cllm 自身 smoke 仍 31/31**。供應商矩陣（能接：openrouter[免註冊 client、最省事]／gemini／azure／github；接不了：deepseek/openai 直填 key、anthropic 走本 proxy）詳見 [tools/README](tools/README.md)。**install 尾巴已收（2026-07-17）**：`cmake --install` 現裝 `liblogin.so`＋`login_abi.h`（→ `include/cllm`）＋`llm-login.pc`＋`llm-login` CLI，沿用 cllm 同一套 configure_file→.pc 機制（帶 INSTALL_RPATH）；下游 `pkg-config --cflags --libs llm-login` → `-I/-L/-llogin` 即可 link。實測：裝到臨時 prefix→最小下游 C 消費端純靠 pkg-config 編譯連結、從乾淨 cwd 跑起、`ldd` 命中裝好的 `.so`（rpath，免 `LD_LIBRARY_PATH`）＋三個 C-ABI 符號 `nm -D` 皆匯出；llm-login 離線測全綠、cli_smoke 仍 31/31。用法段見 [tools/llm-login/README「下游如何 link」](tools/llm-login/README.md)。**open 尾巴（2 項，使用者說「等一下再說」）**：① **聯動 harness demo**（抓 cllm `LLM_ERROR`＋401→呼 `llm_login_login`→重試）② **Windows 實測**（socket/bcrypt 已 `#ifdef` 包好、POSIX 已測、未在 Windows 建）。真 OAuth／真 Anthropic 往返 → [WAIT_USER](WAIT_USER.md)。
- **八語言綁定全路收齊＋一鍵 smoke**（2026-07-16）：全語言（C／C++／Lua／Fennel／s7／Python／CL／Go＋Shell）都綁了 `tools`／`media`／`modalities`＋`on_tool`／`on_media`；每語言各有 `smoke.sh`、[`test/bindings_smoke.sh`](test/bindings_smoke.sh) 一鍵輪跑（9/9 綠）。C++ 另有便利層 `llm.hpp`（聚合 ask／`std::expected`／串流糖／media helpers）＋`llm_reflect.hpp`（`ask_as<T>`／`make_tool<Args>`／`args_as<Args>`／`modality<Config>`）。常駐前綴改 **`~/dev`**（共用目錄、install 冪等覆蓋、勿整個 rm）；s7 原始碼 vendor 進 repo（`bindings/s7/vendor/`），不再依賴 pas。**真後端已驗**（2026-07-16，LM Studio）：`ask_as<T>` required 全吐／tools（C++＋Python）／vision／錯誤路徑／SSE 串流全過；modalities 被靜默忽略（記 [gotchas/backend](workflows/common/gotchas/backend.md)）。**open 尾巴**：① 未在 Windows 實測；② 未接主 CMake（刻意獨立）。詳見 [bindings/README](bindings/README.md)。
- **CLI 打磨第一輪已落地**（2026-07-16，詳見 git log）：管線體驗（stdin×位置參數合體、「-」插入點）＋tools/modalities/media-out 旗標（四路 handler 全開，新 TU `cli_output.*`）＋錯誤訊息打磨，cli_smoke 31/31、真後端驗過（stdin 合體＋`--tool` 打 LM Studio）。**open**：下一輪打磨方向待使用者再點（便利層方向已評估為不值得，撤）。CLI 用法見 [docs/cli-manual.md](docs/cli-manual.md)、檔對應見 [code-map ⑤](workflows/common/code-map/CODE_MAP.md)。

## 各工作流 session-log

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|
| （尚無工作流長出自己的 session-log）| | |

## 不屬任何工作流的進度

- （無）
