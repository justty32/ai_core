# 平台分流與傳輸（native HTTP + 跨平台雙機）

← [README](../README.md)｜[docs 索引](README.md)｜[建置環境](setup.md)｜[除錯](debugging.md)

傳輸層怎麼分平台（WinHTTP／libcurl／`file://`），以及 Windows ⇄ Manjaro 雙機的 preset 設計。

## native HTTP 傳輸（`src/http.{hpp,cpp}`）

`http`＝**純 C++ 的 HTTP 傳輸層**，介面乾淨、系統標頭只藏在 `.cpp`：

```cpp
#include "http.hpp"
http::Request  req{ .url = "...", .method = "POST", .headers = {"Content-Type: application/json"}, .body = "..." };
http::Response r = http::request(req);                    // 非串流：傳輸失敗 throw std::runtime_error
int status = http::stream(req, [](std::string_view chunk){ /* 逐塊 raw bytes */ return false; });  // 回 true 中止
```

- **分層**：C++ 是**笨管子**——只管 TLS＋HTTP round-trip、串流時逐塊把 raw bytes 交回呼；**SSE 拆框／UTF-8 分批／JSON 編解全留上層**（傳輸層語言中立、改起來便宜）。
- **介面/實作分離**：`http.hpp` 只放乾淨介面（`Request`/`Response`/`OnData`/`request`/`stream`）；**系統標頭（`windows.h`／`winhttp.h`／`curl.h`）只在 `http.cpp` include，不外洩到 header**——傳統 header 的好處，取用端 `main.cpp` 不被 `windows.h` 汙染。
- **平台分流**：Windows＝**WinHTTP**（系統內建、Schannel TLS，零安裝；連結 `winhttp`）；Linux/Mac＝**libcurl**（`find_package(CURL)`＋`CURL::libcurl`）。
- **`file://` 特例**：兩平台共用，直接讀檔當 200 回應——保住**離線 fixture 測試 harness**（WinHTTP 不支援 `file://`，非在這層處理不可）。fixture 在 `test/fixtures/{fake,fake_stream,fake_tool,fake_json}/chat/completions`（假 chat completion／SSE／工具／結構化回應，版控當測試資料）。
- **fixture 路徑不寫死**：`llm` CLI 的 `--endpoint file://<絕對路徑>` 就是反射欄位，`test/cli_smoke.sh` 在**執行期**用 `$ROOT` 組出 `file://` 路徑餵進去——不綁機器路徑，比舊 demo exe 靠 `TRY3_SOURCE_DIR` 編譯期注入更活。
- 產物仍**全靜態獨立**：`objdump -p` 只剩 KERNEL32＋系統 CRT＋WINHTTP.dll（Windows 系統內建、必在），無 mingw runtime。

> **★ 上層接口方向定調（不搬 schema 表）**：動態語言缺靜態反射時，schema 表是**補償拐杖**；C++ 有 struct＋glaze 編譯期反射，**struct 本身就是唯一真相源**。往上長 ask 接口時，JSON 對映／CLI 旗標／型別解析／驗證**全從欄位反射生成**（驗證還能移到編譯期），不搬 schema 表。

## 跨平台（Windows ⇄ Manjaro 雙機）

**已實測雙機皆綠**。`CMakePresets.json` 用同一份檔案裝兩組 preset（`mingw-*` / `linux-*`），各自掛 `condition`（`{ "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }` / `"Linux"`），`cmake --list-presets` 在哪台只列哪台的——**不用手動挑、`binaryDir` 共用同一個 `${sourceDir}/build`**（gitignored、兩機不會同時跑，故不衝突）。`mingw-*` 原封不動、`.vscode/` 也不動（Windows VSCode 專用；Manjaro 用 neovim，除錯設定另建，不在本骨架範圍）。

**新增的 `linux-*` preset 只覆寫平台相關的三樣**：

```json
"toolchainFile": "/home/lorkhan/vcpkg/scripts/buildsystems/vcpkg.cmake",
"cacheVariables": {
  "CMAKE_CXX_COMPILER": "/usr/bin/g++",
  "CMAKE_MAKE_PROGRAM": "/usr/bin/ninja"
}
```

刻意**不設** `VCPKG_TARGET_TRIPLET`：MinGW 側因為系統預設 triplet 是 MSVC 導向的 `x64-windows` 才要覆寫成 `x64-mingw-static`；Linux 上 vcpkg 偵測到的預設 triplet 就是 `x64-linux`，直接堪用，不用像 MinGW 那樣特別指定。

**libcurl 怎麼來（實測結論：直接用系統庫，免加 vcpkg 依賴）**：`CMakeLists.txt` 非 Windows 分支本來就是 `find_package(CURL REQUIRED)` + `CURL::libcurl`，**不用改一行**——vcpkg toolchain 不會攔截 `find_package(CURL)`（那是給「vcpkg 裝過的套件」用的 overlay，系統裝的 libcurl 一樣能被標準 CMake `FindCURL` 模組找到）。實測 configure 直接印出 `-- Found CURL: /usr/lib/libcurl.so (found version "8.21.0")`，`vcpkg.json` 全程沒加 `curl`（若加了會從源碼編、慢很多）。

**實測結果**（Manjaro，g++ 16.1.1／cmake 4.3.4／ninja 1.13.2／vcpkg 2026-04-08）：

```bash
cmake --preset linux-debug          # configure：glaze:x64-linux vcpkg 自動裝、約 1.4 秒；系統 CURL 直接命中
cmake --build --preset linux-debug  # 7 個編譯單元全過（.so 5：cabi.cpp/cabi_request/response/stream＋http；
                                     #   CLI 2：cli.cpp＋main.cpp），link 出 build/libcllm.so ＋ build/llm
bash test/cli_smoke.sh              # 離線黑箱煙霧測試 31/31 綠：輸出正確（含串流／結構化）、config 三層來源、
                                     #   退出碼 0/1/2 三段分流；中文輸出（繁中台詞）原樣 UTF-8、無亂碼
```

從乾淨 `rm -rf build` 重跑一次 configure＋build＋煙霧測試，結果一致（`.so`＋`llm` 都在、31/31 綠）。`linux-release`（`-O2`）也建置成功。

**產物名**：`.so` 兩邊都叫 `libcllm.so`。`llm` 執行檔在 Linux **不加副檔名**（`build/llm`）、Windows 是 `build/llm.exe`——CLI 是 unix filter，Linux 側刻意讓它就叫 `llm`。（對照：舊 demo exe 曾為「指令跨平台一致」硬掛 `set_target_properties(... SUFFIX ".exe")` 讓 Linux 也叫 `try3.exe`；那顆 exe 連同該約定已封存，新的 `llm` target 不再沿用。）

**Linux 特有踩坑**：目前實測下**沒踩到需要繞路的坑**——`find_package(CURL)` 直接吃到系統庫、vcpkg 預設 triplet 免設、glaze header-only 秒裝。唯一值得注意的是 vcpkg 這次解出的 glaze 版本是 **7.4.0**（Windows 側原記錄 7.8.4，manifest 沒釘版號，baseline 對到的版本會隨 vcpkg registry 更新而變動）；API 沒動過，行為未受影響，若要鎖版可在 `vcpkg.json` 加 `builtin-baseline` 或 `version>=`（見 [gotchas/build](../workflows/common/gotchas/build.md)）。
