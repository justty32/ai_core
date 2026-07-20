# 共通踩坑（跨工作流）

← [common/README](../README.md)｜[INDEX](../../../INDEX.md)

不專屬任一工作流、任何人都可能撞到的坑，記/查這裡。條目格式：一條一個坑，**粗體標題 + 一兩句現象與對策**，必要時連結細節。膨脹就按主題分檔（本夾已分三檔）。

> 這批坑多數在 cllm 還是 `galtxt/try_3` 純 C++ 實驗線時踩到、抽離後帶過來的（含真後端首驗）；仍成立。

## 按主題分檔

| 檔 | 收哪些坑 |
|----|---------|
| [`build.md`](build.md) | **建置工具鏈**：glaze / C++ 建置、vcpkg / CMake、clangd / 編輯器 |
| [`windows.md`](windows.md) | **Windows 執行 / 中文**：終端碼頁、`wmain`、Smart App Control、Git Bash exec |
| [`backend.md`](backend.md) | **真後端 + 測試資料**：fixture 方法論、後端 error 護欄、reasoning `max_tokens`、schema `required` |

新增坑：歸到對應主題檔；某主題檔自己膨脹超 8192 bytes 再依 [DEV-GUIDE](../../../DEV-GUIDE.md) 往下拆。
