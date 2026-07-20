# 踩坑 · Windows 執行 / 中文

← [gotchas 索引](README.md)｜[common/README](../README.md)｜[INDEX](../../../INDEX.md)

Windows 側執行與中文相關的坑：終端碼頁、命令列參數、Smart App Control、Git Bash exec 位元。

- **Windows 中文：終端碼頁 vs 命令列參數，兩個獨立的坑**。① **輸出亂碼**＝終端碼頁非 UTF-8（繁中預設常 950）——對策：exe 啟動 `SetConsoleOutputCP(CP_UTF8)` 自救，或終端 `chcp 65001`。② **中文命令列參數壞**＝標準 `main(argv)` 拿到系統碼頁——對策：`wmain`＋`-municode`（本專案 `main.cpp` 這樣做），或中文走 stdin/檔案而非 argv。
- **Smart App Control（SAC）偶發擋掉剛編好的新 exe**：Windows 11 SAC On 時把**某顆新編、沒信譽的 exe 雜湊**判成 unknown→封鎖，報「應用程式控制原則已封鎖此檔案」。**這不是程式錯誤**。GCC 連結是**決定性的**——原始碼沒變，重連結會**重現同一顆被擋的雜湊**；`cp` 成新檔名也同雜湊照擋。真正可靠：**改變二進位內容**（`strip build/llm.exe` 去符號→新雜湊，一次放行；或真改一行原始碼再編）。`Unblock-File`（MOTW）無關無效。
- **Git Bash 跑剛連結好的 exe 報 `Permission denied`（exit 126），但 PowerShell 跑同一支正常**：這**不是 SAC**（SAC 兩個殼都擋），是 Git Bash 對「剛被 `gcc` 寫出、exec 位元狀態未穩」的 exe 偶發拒跑。**別誤診成 SAC 去重編**——改用 PowerShell/cmd 跑，或 `chmod +x`。判別法：PowerShell 跑得動＝非 SAC。
- **★ 離線 fixture 的 `file://` 在 Windows 要真磁碟路徑（`C:/…`），不是 MSYS 路徑（`/c/…`）**：`cli_smoke.sh` 用 `$(pwd)` 組 `file://` endpoint，Git Bash 的 `pwd` 給 `/c/code/…`（MSYS 式、磁碟無冒號），native `llm.exe` 的 `fopen` 開不了 → 所有「成功路徑」測試變 `exit=2 out=<>`、錯誤路徑照過（假象：像後端連不上）。**cllm 程式碼沒問題**——`llm.exe` 餵 `file://C:/…` 正常回 fixture。對策：smoke 在 MSYS 下用 `pwd -W` 取 Windows 路徑組 `$FX`（`uname` 判 `MINGW*/MSYS*/CYGWIN*`；已實作），Linux 不變。判別法：手動 `llm.exe 你好 --endpoint file://C:/…/test/fixtures/fake/chat/completions` 回台詞＝binary 沒事、是腳本路徑格式。
