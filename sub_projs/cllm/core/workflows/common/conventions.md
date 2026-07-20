# 程式碼慣例 + 導航 index 維護鏈（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（feature-dev / refactor…）共用這套規矩。結構整理原則（被動、按需取用）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../AGENTS.md)。

## 程式碼慣例

- **struct＝唯一真相源**：請求／回應都是 struct，glaze 編譯期反射自動 JSON↔struct，**零 schema 表**。往上長接口（JSON 對映／CLI 旗標／型別解析／驗證）**全從欄位反射生成**，不搬動態語言那套 schema 表（那是缺靜態反射時的補償拐杖）。
- **對外只露 C ABI**：內部 struct 是實作細節、**不外洩**；對外只有 `src/cabi.h` 那套 `extern "C"` 扁平結構＋唯一入口 `llm_ask`。C++ 面走薄鏡像 `cabi.hpp`（`llm::abi`）一比一對應、header-only。**反射糖（`ask<T>`／`make_tool<Args>`）刻意不放鏡像層**，留給使用者自己在上面包。
- **實作按關注點拆檔**：`cabi.cpp`（出口＋`make_request`）／`cabi_request.cpp`（`build_body`＝唯一請求真相源）／`cabi_response.cpp`（解回應＋錯誤護欄）／`cabi_stream.cpp`（SSE）／`http.cpp`（傳輸管子）。分工表在 `src/cabi_internal.hpp`。
- **★ 各 impl `.cpp` 用唯一具名子 namespace**（`req_impl`／`resp_impl`／`stream_impl`），**絕不用匿名 namespace**——否則 glaze 的 `quoted_key_v` COMDAT section 會跨 TU 撞名給錯 JSON 鍵（見 [gotchas/build](gotchas/build.md)）。
- **介面/實作分離**：系統標頭（`windows.h`／`winhttp.h`／`curl.h`）**只在 `.cpp` include，不外洩到 header**——取用端不被 `windows.h` 汙染。
- **跨平台碼以 `#ifdef _WIN32` 包乾淨**；Windows 走 WinHTTP＋`wmain`＋`-municode`，POSIX 走 libcurl＋標準 `main`。
- **單檔行數門檻 300 行**（與 [DEV-GUIDE](../../DEV-GUIDE.md) 觸發 A 一致）：超標觸發檢視、按職責拆。
- **breaking change 前先全域 grep 受影響處、同 commit 更新**；尤其**動 `cabi.h` 的 C ABI**（扁平結構／`llm_ask` 簽章／enum）會震到所有 C 客戶端與 `llm` CLI，改前先想相容性（鐵律 3）。
- **加 vcpkg 依賴**：`vcpkg.json` 的 `dependencies` 加名字 → `find_package` ＋ `target_link_libraries` → 重配置。glaze 需 C++23。

## 導航 index（code map）維護鏈

**code map ＝ [code-map/CODE_MAP.md](code-map/CODE_MAP.md)**：描述 `src/` 每個檔負責什麼領域、關鍵符號、常見任務先讀哪些檔、測試對應。修改原始碼前先讀它。

三個面向構成維護鏈：**程式碼 → code map → 文檔**。

**優先級（衝突或時間不夠時，依序保持一致）：** 程式碼 > code map > 文檔。
**code map 與程式碼衝突時：以程式碼為準，立即修正 [CODE_MAP.md](code-map/CODE_MAP.md)。**

**日常規則：**
1. **修改前**：先讀 [CODE_MAP.md](code-map/CODE_MAP.md)，找到相關領域，只讀清單中列出的檔——不要漫讀無關領域。
2. **修改後**：若新增或刪除了原始碼檔案，或某檔案的職責／關鍵符號有顯著改變，必須同步更新 [CODE_MAP.md](code-map/CODE_MAP.md)（含分層總圖與領域地圖）。動對外 C ABI 時連 README 對應段一起更新。
3. 原始碼檔案本身**不加**「對應 code map」的註釋（維護成本過高）；反向查找直接 grep CODE_MAP。
