# 踩坑 · 建置工具鏈（glaze / vcpkg / clangd）

← [gotchas 索引](README.md)｜[common/README](../README.md)｜[INDEX](../../../INDEX.md)

建置與工具鏈類的坑：glaze 反射、vcpkg / CMake、clangd / 編輯器整合。

## glaze / C++ 建置

- **glaze 反射 JSON 預設遇未知鍵報錯**：`glz::read_json(obj, src)` 用預設選項，只要 `src` 有 `obj` struct 沒有的欄位就整包解碼失敗。解真後端回應（OpenAI chat completion 這種欄位一大堆、你只挑 `choices[0].message.content` 幾個）必踩。對策：`glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)`——忽略多餘鍵、只填 struct 有的。`cabi_response.cpp`（＋`cabi_stream.cpp`）就是這樣用 `kLenient` opts。
- **★ 匿名 namespace 的同名 struct 讓 glaze COMDAT section 跨 TU 撞名（MinGW/GCC）**：多個 `.cpp` 各自在**匿名 namespace** 定義同名 struct（如 `ReqBody`），GCC 把匿名 namespace **一律 mangle 成 `_GLOBAL__N_1`**（不保證每 TU 唯一），於是 glaze 為每個型別生成的 `quoted_key_v` 之類 `static constexpr` 走 COMDAT/weak section，section 名含 `_GLOBAL__N_1::ReqBody` → 跨 TU **撞名**。若兩個 `ReqBody` 欄位不同，連結器報 `duplicate section … has different size`，折疊後**可能給錯 JSON 鍵**（不只是警告，是潛在序列化 bug）。對策：**別用匿名 namespace，改各 TU 唯一命名的內部 namespace**（如 `req_impl`／`resp_impl`／`stream_impl`）＋ `using namespace X;`。

## vcpkg / CMake

- **vcpkg toolchain 不會攔截系統的 `find_package(CURL)`**：怕 vcpkg 搶走而特地在 `vcpkg.json` 加 `curl`（→ 從源碼編、很慢）是多餘的。vcpkg toolchain 只是在標準 CMake 模組搜尋**之前**插入自己的搜尋路徑，沒裝就自然落回系統庫。Manjaro 上 `find_package(CURL REQUIRED)` 直接命中 `/usr/lib/libcurl.so`。
- **vcpkg manifest 沒釘 `builtin-baseline`，兩台機器會解出不同版本**：同一份 `vcpkg.json` 因各機 registry 快照不同而版本漂移（實例：glaze，Windows 7.8.4 / Linux 7.4.0）。API 沒變、行為一致，但**跨機潛在地雷**——要鎖版就加 `builtin-baseline` 或 `version>=`。
- **★ MinGW triplet**：vcpkg 預設 triplet `x64-windows`（MSVC 導向）在 MinGW 上會建置／連結失敗，`CMakePresets.json` 的 `mingw-*` 已設 `x64-mingw-static`。**Linux 側 `linux-*` 不設 `VCPKG_TARGET_TRIPLET`**，用偵測到的預設 `x64-linux` 即可。

## clangd / 編輯器

- **CMake 3.28+ 配 Ninja 會塞 C++20 modules 的掃描旗標，clangd 看不懂**：即使專案完全不用 modules，Ninja generator 仍會給每個編譯單元加 `-fmodules-ts`／`-fmodule-mapper=…`／`-fdeps-format=p1689r5`（P1689 依賴掃描）。這些旗標原樣進 `compile_commands.json`，clang 驅動器不認得 → 每個檔都噴 `Unknown argument` 診斷雜訊。對策：`.clangd` 的 `CompileFlags.Remove` 過濾掉（本專案 `.clangd` 有實例）。
- **clangd 不會自己下探 `build/` 找 `compile_commands.json`**：它只從當前檔案往**上層**找；CMake 產在 `build/` 底下，所以要在專案根放 `.clangd` 寫 `CompilationDatabase: build`。這做法**編輯器中立**（nvim 與 VSCode 的 clangd 都讀），比在編輯器設定裡塞 `--compile-commands-dir` 好。
- **nvim-dap：codelldb 只監聽 `127.0.0.1`（純 IPv4）——adapter 寫 `host = "localhost"` 會在 IPv6 優先的機器上 ECONNREFUSED**：`getaddrinfo("localhost")` 先回 `::1` 時連過去就被拒。**LazyVim 自己的 clangd extra 就是寫 `host = "localhost"`**（多數機器碰巧沒事），照抄就踩。對策：**別給 `host`**（預設 `127.0.0.1`）或明寫 `127.0.0.1`。
- **註冊某個 DAP adapter 名字，會招來 mason 自動下載對應套件**：`mason-nvim-dap` 的 `automatic_installation` 掃 `dap.adapters` 的 **key**，看到 `cppdbg` 就去解析成 `cpptools`（微軟 ~90MB 的 VSCode 擴充）開始下載——即使你只想把 `cppdbg` 接到別的除錯器。對策：`automatic_installation = { exclude = { "cppdbg" } }`。
