# testing — 跑測試 / 建置驗證（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

> 本檔是「單檔工作流」：一個 `.md` 同時是入口與內容。膨脹了就照 [DEV-GUIDE](../DEV-GUIDE.md) 升級成資料夾型。建置／依賴／跨平台細節在 [README](../README.md)。

## 指令

首次要先 configure（讀 `CMakePresets.json`，vcpkg 自動裝 glaze）。Linux（Manjaro）：

```bash
cmake --preset linux-debug          # 首次 configure
cmake --build --preset linux-debug  # 建置 → build/libcllm.so ＋ build/llm
bash test/cli_smoke.sh              # 離線黑箱煙霧測試（驅動 build/llm 打 file:// fixtures）
```

- **快速驗證（Claude 自己跑、鐵律要求的那套）**：`cmake --build --preset linux-debug && bash test/cli_smoke.sh`（**17/17 綠**）。
- **完整驗證（乾淨重建）**：`rm -rf build && cmake --preset linux-debug && cmake --build --preset linux-debug && bash test/cli_smoke.sh`。
- **Windows**：把 `linux-` 換成 `mingw-`；產物為 `build/llm.exe`（`.so` 兩邊都叫 `libcllm.so`）。
- **Release**：把 `-debug` 換成 `-release`（`-O2`）。

## 測試分類

- **離線黑箱（`test/cli_smoke.sh`，Claude 可跑）**：全走 `--endpoint file://` 指 `test/fixtures/{fake,fake_stream,fake_tool,fake_json}/chat/completions`，不連網——驗輸出正確（含串流／結構化）、config 三層來源、退出碼 0/1/2 三段分流、繁中 UTF-8 無亂碼。fixture 路徑執行期用 `$ROOT` 組，不綁機器路徑。
- **真後端（使用者做）**：離線 fixture **只投影成功、從不投影失敗**——錯誤路徑／reasoning 模型 `max_tokens`／schema `required` 等只有打真的本機 LM Studio（或雲端 API）才驗得出。把 `llm` 的 `--endpoint` 指向真後端實跑；驗證需求記 [WAIT_USER](../WAIT_USER.md)，坑見 [common/gotchas/backend](common/gotchas/backend.md)。
