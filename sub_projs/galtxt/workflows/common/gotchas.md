# 共通踩坑（跨工作流）

← [INDEX](../../INDEX.md)

不專屬任一工作流、任何人都可能撞到的坑，記/查這裡。某工作流專屬的坑記在該工作流自己的 `gotchas.md`（長出來後在下表加一列導流）。

## 哪類坑記哪裡

| 坑的性質 | 記/查這裡 |
|---------|----------|
| 不專屬任一工作流的共通坑 | **common/gotchas**（本檔）|

條目格式：一條一個坑，**粗體標題 + 一兩句現象與對策**，必要時連結細節。

---

- **Windows 中文：終端碼頁 vs 命令列參數，兩個獨立的坑**。① **輸出亂碼**＝終端碼頁非 UTF-8（繁中預設常 950），程式吐 UTF-8 被讀錯——對策：exe 啟動 `SetConsoleOutputCP(CP_UTF8)` 自救（try_2 host.exe 這樣做），或終端 `chcp 65001` / PowerShell `[Console]::OutputEncoding`。② **中文命令列參數壞**＝MinGW 標準 `main(argv)` 拿到的是系統碼頁（950）不是 UTF-8——對策：用 `wmain`＋`-municode`＋`WideCharToMultiByte` 轉 UTF-8（try_2 host.exe、try_1 s7host.exe 都這樣），或把中文走檔案（`--infile` 讀 UTF-8）而非 argv。標準 `lua.exe`（無 wmain）就中鏢②，所以它只適合 debug、餵中文靠 `--infile`。
- **Smart App Control（SAC）偶發擋掉剛編好的新 exe**：本機（Windows 11）SAC 為 On，會把**某顆新編、沒信譽的 exe 雜湊**判成 unknown→封鎖，跑時報「應用程式控制原則已封鎖此檔案」。**這不是程式錯誤**——同一份原始碼**重編一次**產生新雜湊即放行。已驗：純連 Lua、載外部腳本、C／C++ 全能跑，只是偶發雜湊信譽卡住；`Unblock-File`（MOTW）無關、無效。對策：重編；若反覆卡同一支，換輸出檔名或清掉重編。（try_2 host.exe 踩過，見 [try_2/README](../../try_2/README.md)。）
- **Git Bash 跑剛連結好的 exe 報 `Permission denied`（exit 126），但 PowerShell 跑同一支正常**：這**不是 SAC**（SAC 會兩個殼都擋），是 Git Bash 對「剛被 `ar`/`gcc` 寫出、exec 位元狀態未穩」的 exe 偶發拒跑。**別誤診成 SAC 去重編**——直接改用 PowerShell（或 cmd）跑，或 `chmod +x`。判別法：PowerShell 跑得動＝非 SAC＝Bash 的 exec-bit 坑；PowerShell 也被擋＝才是 SAC，重編。
- **glaze 反射 JSON 預設遇未知鍵報錯**：`glz::read_json(obj, src)` 用預設選項，只要 `src` 有 `obj` struct 沒有的欄位就整包解碼失敗。解真後端回應（OpenAI chat completion 這種欄位一大堆、你只挑 `choices[0].message.content` 幾個）必踩。對策：`glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)`——忽略多餘鍵、只填 struct 有的。（try_3 `main.cpp demo_http()` 踩過，見 [try_3/README](../../try_3/README.md)。）
- **匿名 namespace 的同名 struct 讓 glaze COMDAT section 跨 TU 撞名（MinGW/GCC）**：多個 `.cpp` 各自在**匿名 namespace** 定義同名 struct（如 `ReqBody`），GCC 把匿名 namespace **一律 mangle 成 `_GLOBAL__N_1`**（不保證每 TU 唯一），於是 glaze 為每個型別生成的 `quoted_key_v` 之類 `static constexpr` 走 COMDAT/weak section，section 名含 `_GLOBAL__N_1::ReqBody` → 跨 TU **撞名**。若兩個 `ReqBody` 欄位不同，連結器報 `duplicate section … has different size`，折疊後**可能給錯 JSON 鍵**（不只是警告，是潛在序列化 bug）。對策：**別用匿名 namespace，改各 TU 唯一命名的內部 namespace**（如 `tool_impl`/`media_impl`/`json_impl`）＋ `using namespace X;`。（try_3 `llm_*.cpp` 踩過，見 [try_3/README](../../try_3/README.md)。）
