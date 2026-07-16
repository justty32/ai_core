# cllm — C LLM：對外 C ABI（`libcllm.so`）＋ `llm` CLI（unix filter）

← [sub_projs](../README.md)

**cllm** 把 LLM 收成**一支對外 C ABI 共享庫 `libcllm.so`**（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 **`llm` unix filter CLI**。源自 galtxt 的純 C++ 實驗線（原 `galtxt/try_3`），收斂成兩交付物後**抽離成獨立產物**。

**完全 C++、純原生**——不嵌任何腳本 VM。（早期曾用 C++20 modules 當骨架示範，**已回歸傳統 header**：C++ 有 struct＋glaze 編譯期反射，接口的唯一真相源是 struct 本身，modules 那層抽象畫蛇添足，拿掉。）建置走 **CMake + Ninja + vcpkg toolchain**（配 `CMakePresets.json` 釘死工具鏈）。

目前狀態：**可建置、可 VSCode/gdb 除錯；Windows／Linux（Manjaro）雙機皆已實測**。

> ★ **2026-07 收斂成兩個對外交付物**（早期那批可獨立呼叫的 L0 C++ 入口＋demo exe 已封存進 `archived/`，見下）：
> 1. **`libcllm.so` —— 對外 C ABI**（`src/cabi.*`，`extern "C"`）：唯一入口 `llm_ask` 統一吃 prompt＋
>    schema＋tools＋media＋modalities＋stream。純 C 客戶端 include `cabi.h`、連 `-lcllm` 即用；另附
>    header-only 的 C++ 薄鏡像 `cabi.hpp`（`llm::abi`）。
> 2. **`llm` —— unix filter CLI**（`src/cli.*`＋`src/main.cpp`）：走 `cabi.hpp` 消費 `libcllm`。

現行原始碼（`src/`）：`http.{hpp,cpp}`（native HTTP 傳輸，兩交付物共用的傳輸管子）、`cabi.h`＋
`cabi_{client,request,response,context}.h`（對外 C ABI 傘檔＋功能頭）、`cabi.cpp`／`cabi_request.cpp`／
`cabi_response.cpp`／`cabi_stream.cpp`（C ABI 實作，按關注點拆檔）、`cabi_internal.hpp`（實作共用內部頭）、
`cabi.hpp`＋`cabi_{context,request,response}.hpp`（C++ 薄鏡像）、`cli.{hpp,cpp}`＋`main.cpp`（`llm` CLI）。

> **`archived/`（git mv 保史、內容一字未改，之後有需要再刪）**：舊 L0 的可獨立呼叫入口——`llm.{hpp,cpp}`
> （`llm::Client`／`ask`／`from_env`）、`llm_tool`／`llm_media`／`llm_json`（工具呼叫／vision／結構化輸出三擴充）、
> `llm_schema.hpp`、舊反射 `cli.{hpp,cpp}`、`demo.{hpp,cpp}`、跑離線 demo 的舊 `main.cpp`。這些在重構時被
> **融進 C ABI 實作**（見「對外 C ABI」一節），不再對外提供獨立函數；`git mv` 保史留著、之後有需要再刪。

離線 fixture（`test/fixtures/{fake,fake_stream,fake_tool,fake_json}/`）＋ `test/cli_smoke.sh` 端到端驗
`llm` CLI（17/17 綠）——Windows 用 mingw preset、Linux 用 linux preset（詳見下方「跨平台」一節）。

## 需要的東西

**Windows**

- **MinGW-w64**（本機 `C:/dev/mingw64`）：g++ 16、gdb。
- **CMake** 3.28+（本機 4.3.3）。
- **Ninja** 1.11+（本機用 vcpkg 自帶的 **1.13.2**）。preset 用 Ninja（非模組建置亦適用；當初為 modules 掃描而選，回歸 header 後保留不換）。
  取得：`C:/dev/vcpkg/vcpkg.exe fetch ninja` 會下載並印出路徑（本機在 `C:/dev/vcpkg/downloads/tools/ninja-1.13.2-windows/ninja.exe`，已釘進 preset）。
- **vcpkg**（本機 `C:/dev/vcpkg`）：preset 已接 `scripts/buildsystems/vcpkg.cmake` toolchain，日後加依賴（`vcpkg.json` manifest）即自動安裝。
- **VSCode 擴充**：
  - **C/C++**（`ms-vscode.cpptools`）— 必裝，提供 `cppdbg` 除錯器。
  - **CMake Tools**（`ms-vscode.cmake-tools`）— 選裝，裝了能在側欄切 preset／建置。

**Linux（Manjaro，實測）**

- **系統 g++**（本機 `/usr/bin/g++`，16.1.1）：`pacman -S gcc`；gdb 同包路徑取得。
- **CMake** 3.28+（本機 4.3.4）、**Ninja** 1.11+（本機 `/usr/bin/ninja`，1.13.2，`pacman -S cmake ninja`）。
- **vcpkg**（本機 `/home/lorkhan/vcpkg`，設 `VCPKG_ROOT`）：preset 接同一份 `scripts/buildsystems/vcpkg.cmake` toolchain。
- **libcurl**：走**系統既有**（本機 `pacman` 裝的 8.21.0，`pkg-config --modversion libcurl` 可查）——`vcpkg.json` **不加** `curl`（那會從源碼編、很慢），`find_package(CURL REQUIRED)` 在 vcpkg toolchain 下一樣找得到系統庫（見下方「跨平台」一節）。
- neovim/lazyvim 除錯另建，`.vscode/` 是 Windows VSCode 專用不適用。

路徑（`C:/dev/mingw64`、`C:/dev/vcpkg`、ninja 版本／`/usr/bin/g++`、`/home/lorkhan/vcpkg`、`/usr/bin/ninja`）都在 `CMakePresets.json`（`mingw-*`／`linux-*` 兩組 preset，`.vscode/` 僅 Windows 側）釘死，換機改那幾處即可。

## 建置（命令列）

```bash
# Windows（mingw-debug／mingw-release）
cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
cmake --build --preset mingw-debug  # 建置 → build/libcllm.so ＋ build/llm.exe
./build/llm.exe 你好                # 跑 CLI（Windows 產物帶 .exe）

# Linux（linux-debug／linux-release，指令完全同構）
cmake --preset linux-debug
cmake --build --preset linux-debug  # 建置 → build/libcllm.so ＋ build/llm
./build/llm 你好                    # Linux 產物無副檔名（unix filter 慣例）
bash test/cli_smoke.sh              # 離線黑箱煙霧測試（驅動 build/llm 打 file:// fixtures）
```

Release：把 `-debug` 換成 `-release`（`mingw-release` 或 `linux-release`）。

> ⚠ **CLI 產物名跨平台不同**：`.so` 兩邊都叫 `libcllm.so`；`llm` 執行檔在 Windows 是 `llm.exe`、Linux 是
> `llm`（無副檔名）——CLI 是 unix filter，Linux 側刻意**不**加 `.exe` 後綴（對照舊 demo exe 曾為指令一致而
> 硬掛 `.exe`；那顆 exe 已封存，這個約定不再沿用）。

`cmake --list-presets` 只會列出當前平台的 preset——兩組 preset 各自掛了 `condition`（`hostSystemName` 判斷），Windows 只看得到 `mingw-*`、Linux 只看得到 `linux-*`，不用手動挑對的那組。

## 依賴（vcpkg manifest）

已接 **glaze**（反射式 JSON↔struct、header-only、超快）當第一個 vcpkg 庫，兼作整條鏈的驗證。用 **manifest 模式**（宣告式、隨專案版控）：

1. [`vcpkg.json`](vcpkg.json) 宣告依賴：
   ```json
   { "name": "cllm", "version-string": "0.0.1", "dependencies": ["glaze"] }
   ```
2. **★ MinGW triplet（最容易踩的雷）**：vcpkg 預設 triplet 是 `x64-windows`（MSVC 導向），在 MinGW 上會建置／連結失敗。`CMakePresets.json` 的 `cacheVariables` 已設（static，與本線 `-static` 獨立 exe 哲學一致）：
   ```json
   "VCPKG_TARGET_TRIPLET": "x64-mingw-static",
   "VCPKG_HOST_TRIPLET":   "x64-mingw-static"
   ```
   （要動態庫改 `x64-mingw-dynamic`，但那樣 exe 執行期要找 DLL，與本線 `-static` 相左。）
   **Linux 側 `linux-*` preset 沒設 `VCPKG_TARGET_TRIPLET`**：讓 vcpkg 用偵測到的預設 `x64-linux` 即可，不用比照 MinGW 特別指定。
3. `CMakeLists.txt`：`find_package(glaze CONFIG REQUIRED)` ＋ `target_link_libraries(cllm PRIVATE glaze::glaze)`（`llm` CLI 另連 `cllm` 拿 `llm_ask`＋`glaze` 做反射）。glaze 需 **C++23**（已把 `CMAKE_CXX_STANDARD` 提到 23，g++16 支援）。
4. `cmake --preset mingw-debug`（或 `linux-debug`）配置時 vcpkg 自動裝依賴（glaze header-only，約數秒；Linux 實測 7.4.0，約 1.4 秒裝完）。vcpkg 裝到 `build/vcpkg_installed/`（gitignored）。

**要再加庫**：`vcpkg.json` 的 `dependencies` 加一個名字 → `find_package`＋`target_link_libraries` → 重配置。實測 glaze 7.8.4：`glz::write_json(struct)` / `glz::read_json(struct, src)` 反射自動映射、中文原樣 UTF-8——`cabi_request.cpp` 的 `build_body` 靠它把請求 struct 寫成 JSON、`cabi_response.cpp` 靠它把回應 JSON 解回 struct。

> ℹ glaze 這種現代 C++ 庫用**編譯期反射**：定義 `struct` 就自動 JSON↔struct，不用寫映射巨集（這正是 C++26 `std::meta` 反射要標準化的東西，glaze 現在就有）。

> ⚠ **glaze 預設遇未知鍵報錯**：真後端回應（OpenAI chat completion）欄位遠多於你 struct 挑的那幾個。用 `glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)` 才會忽略多餘鍵、只填 struct 有的。`cabi_response.cpp`（＋`cabi_stream.cpp`）就是這樣用 `kLenient` opts 把 `choices[0].message.content` 台詞挑出來。

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
- **平台分流**：Windows＝**WinHTTP**（系統內建、Schannel TLS，零安裝；連結 `winhttp`）；Linux/Mac＝**libcurl**（`find_package(CURL)`＋`CURL::libcurl`，需在 `vcpkg.json` 加 `curl`——目前僅 Windows 實測）。
- **`file://` 特例**：兩平台共用，直接讀檔當 200 回應——保住**離線 fixture 測試 harness**（WinHTTP 不支援 `file://`，非在這層處理不可）。fixture 在 `test/fixtures/{fake,fake_stream,fake_tool,fake_json}/chat/completions`（假 chat completion／SSE／工具／結構化回應，版控當測試資料）。
- **fixture 路徑不寫死**：`llm` CLI 的 `--endpoint file://<絕對路徑>` 就是反射欄位，`test/cli_smoke.sh` 在**執行期**用 `$ROOT` 組出 `file://` 路徑餵進去——不綁機器路徑，比舊 demo exe 靠 `TRY3_SOURCE_DIR` 編譯期注入更活。
- 產物仍**全靜態獨立**：`objdump -p` 只剩 KERNEL32＋系統 CRT＋WINHTTP.dll（Windows 系統內建、必在），無 mingw runtime。

> 這是本線「native HTTP 傳輸先」的落地：先把傳輸下沉成純原生 `.cpp`，JSON 已有 glaze，**真後端 round-trip＋SSE 串流的縫都在**——對外 C ABI（下一節，`llm_ask` 統一入口）與其上的 `llm` CLI 都已長出來，接真後端見後面「接真後端」一節。
>
> **★ 上層接口方向定調（不搬 schema 表）**：動態語言缺靜態反射時，schema 表是**補償拐杖**；C++ 有 struct＋glaze 編譯期反射，**struct 本身就是唯一真相源**。往上長 ask 接口時，JSON 對映／CLI 旗標／型別解析／驗證**全從欄位反射生成**（驗證還能移到編譯期），不搬 schema 表。

## 對外 C ABI（`libcllm.so`，`src/cabi.*`）

在 http＋glaze 兩塊地基上，把 LLM 接口收成**一個對外 C ABI**：`extern "C"` 的扁平結構＋唯一入口 `llm_ask`。內部實作仍守「**struct＝唯一真相源**」（請求/回應都是 struct、glaze 反射自動 JSON↔struct、零 schema 表），只是這些 struct 是**實作細節、不外洩**——對外只露 `cabi.h` 那套 C 結構。

**唯一入口 `llm_ask`**：一次吃 prompt＋schema＋tools＋media＋modalities＋stream，組成**一個** OpenAI 請求。

```c
#include "cabi.h"   // 純 C 客戶端；連 -lcllm
llm_client_t  c   = { .endpoint = "https://…/chat/completions", .api_key = "sk-…" };
llm_client_set_field(&c, LLM_FIELD_TEMPERATURE, 1); c.temperature = 0.7f;  // 取樣欄位靠 field_mask 記帳
llm_request_t r   = { .prompt = "你好", .stream = 1 };
llm_handlers_t h  = { .on_text = my_on_text };   // 文字（串流逐段／非串流整段）、on_tool/on_media/on_error
llm_status_t  st  = llm_ask(NULL, &c, &r, &h);   // ctx 給非 NULL 才能從別 thread cancel()/phase()
```

- **為什麼是「一個入口」而非四個**：舊 L0 的 `ask`/`ask_tools`/`ask_vision`/`ask_as` 是**彼此不可組合**的四條路（要「串流的結構化輸出＋帶圖」就沒轍）。C ABI 把整合搬進**唯一一處** `build_body`——schema／tools／media／modalities 全是同一個請求 body 的正交欄位，愛怎麼組就怎麼組。
- **取樣欄位（`model`/`temperature`/`top_p`/`max_tokens`/`presence_penalty`/`frequency_penalty`/`seed`）全選填**：C 沒有 `std::optional`，改用 `field_mask` 位元記帳（`llm_client_set_field`／`llm_client_has_field`）——沒設的欄位不寫進 JSON，交後端默認（⚠ `model` 例外：本地伺服器可省、OpenAI 雲端必填）。
- **輸入媒體**（`llm_media_in_t`）：`url`（https:// 或 data URI）或原始 `data`/`len` bytes 二選一；音訊→`input_audio`+base64、其餘→`image_url`（url 或本地 bytes 轉 data URI）。**想要的輸出模態**（`llm_modality_t`）另走 `modalities` 陣列＋每模態的 config JSON。
- **對稱四 handler**（`llm_handlers_t`）：`on_text`／`on_tool`（拼完整才交）／`on_media`（產出媒體）／`on_error`；前三個回非 0＝中止。每個帶 `void* user` 讓 C 客戶端夾帶狀態。
- **非同步控制**（`llm_context_t`＝兩個 int：cancel／phase，呼叫端自持、無 heap）：`llm_cancel` 從別 thread 請求取消、`llm_phase` 原子讀階段（IDLE→WAIT→STREAM→DONE/ERROR/CANCELLED，原子性靠 `std::atomic_ref`）。⚠ 邊界：非串流的 `http::request` 是阻塞的，cancel 只在派送**前**檢查得到；串流每塊都檢查、能乾淨中止。

**實作按關注點拆檔**（C++23＋glaze，見 [`cabi_internal.hpp`](src/cabi_internal.hpp) 的分工表）：

| 檔 | 職責 |
|---|---|
| `cabi.cpp` | 出口 `llm_ask`/`llm_cancel`/`llm_phase` ＋ `make_request`（連線設定→`http::Request`）|
| `cabi_request.cpp` | `build_body`：組 OpenAI 請求 JSON（**唯一請求真相源**；content 字串或異質陣列、response_format、tools、modalities 字串插接）|
| `cabi_response.cpp` | `dispatch_nonstream`（解 choices[0].message 派 handler）＋ `guard_backend_error`（後端錯誤護欄）|
| `cabi_stream.cpp` | `do_stream`：SSE 逐行解、`on_text` 逐段、tool_calls 按 index 拼、`[DONE]` 收工 |

- ★ **各 impl .cpp 用唯一具名子 namespace**（`req_impl`／`resp_impl`／`stream_impl`），**不可用匿名 namespace**：GCC 把匿名 namespace 一律 mangle 成 `_GLOBAL__N_1`，同名 struct（`ReqBody`…）的 glaze `quoted_key_v` COMDAT section 會跨 TU 撞名、大小不同→折疊後給錯 JSON 鍵（沿自舊 L0 的坑，見 [common/gotchas](workflows/common/gotchas.md)）。
- ★ **後端錯誤會 throw、不靜默回空**（護欄，2026-07-15 真後端驗揪出）：OpenAI 相容後端出錯（LM Studio 模型載入失敗、雲端 401/429…）回 `{"error":{"message":…}}`＋HTTP 4xx、**無 `choices`**。`guard_backend_error(status, raw)`（解 error 物件 throw、或 HTTP≥400 帶狀態碼＋body 片段 throw）在非串流派送前攔一次；串流路徑另接 `http::stream` 的 status＋累積 raw 再攔一次。`llm_ask` 把例外收斂成 `on_error`＋回 `LLM_ERROR`。**離線 `file://` 回 status=200 且無 error 物件，不受影響**（fixture 不回歸）。⚠ 這種「離線 fixture 從不回錯、只投影成功」的缺陷離線驗不出（見 [common/gotchas](workflows/common/gotchas.md) 方法論條 ③）——得靠真後端驗（見下「接真後端」一節）。
- ⚠ **schema 的 `required` 坑（真後端 2026-07-14 血淚，寫 `--schema` 檔的人要知道）**：JSON Schema 下**沒列進 `required` 的欄位就是選填**，後端的約束解碼只保證「不多給鍵」、不保證「每個欄位都給」。實測 LM Studio + gemma 對 `{name,affection,lines}` 的 schema **若沒標 required 只吐 `{"name":…}`**。C ABI 收的是**原始 schema JSON 字串**（`llm_schema_t.json`／`llm_tool_def_t.parameters`，caller 自備），故 `required` 要不要標、標哪些，是寫 schema 檔那方的責任。（舊 L0 曾有 `schema_of<T>()` 把「開 `error_on_missing_keys` ⇒ 非 nullable 成員自動進 required」包起來的便利層——已隨三擴充封存進 `archived/llm_schema.hpp`；反射生成 schema 的便利層留待日後在 `cabi.hpp` 上層重長，見 `cabi_request.hpp` 註。）

**C++ 薄鏡像 `cabi.hpp`（`llm::abi`，header-only）**：C ABI 的 C++ 面，一比一對應、捨去 C 的麻煩——`const char*`→`std::string`、指標+count→`std::vector`、函數指標+`void*`→`std::function`（閉包自帶狀態）、`field_mask`+值→`std::optional`。

```cpp
#include "cabi.hpp"     // 連 libcllm
llm::abi::Client c;
c.endpoint = "https://…/chat/completions"; c.temperature = 0.7f;  // optional，未設就不送
c.ask({.prompt = "你好"}, {.on_text = [](std::string_view sv){ std::fputs(…); return false; }});
// 另 thread 取消：llm::abi::Context ctx; std::thread t([&]{ c.ask({.prompt="很長的活"}, h, &ctx); }); … ctx.cancel();
```

- ★ **反射糖刻意不放這層**（`ask<T>`、`make_tool<Args>`、`modality<Config>`）：那是「方便使用」的便利層，`cabi.hpp` 只依賴 `cabi.h`、不碰 glaze，留給使用者自己在薄鏡像上包。`llm` CLI 就是這薄鏡像的第一個消費端（下一節）。
- 全部離線可測：`--endpoint file://` 指 `test/fixtures/{fake,fake_stream,fake_tool,fake_json}/chat/completions`，`test/cli_smoke.sh` 端到端跑過（17/17）。

## `llm` CLI（unix filter，`src/cli.{hpp,cpp}` + `src/main.cpp`）

> ★ 第二個交付物，建在上一節「對外 C ABI」之上：走 **C++ 鏡像 `cabi.hpp`（`llm::abi::Client`）** 消費
> `libcllm`，唯一入口 `llm_ask` 統一吃 prompt＋schema＋media。它也是那薄鏡像的第一個真實消費端——
> 反射生成旗標這一手，正是把「不搬 schema 表、全靠 struct 反射」的主張延伸到 CLI 那一哩（反射 `llm::abi::Client`）。

`build/llm` 是 unix 風格過濾器：**prompt 走位置參數，沒給就從 stdin 整段讀**；答案吐 stdout、診斷吐 stderr。

```bash
llm 用一句話介紹你自己                      # 位置參數當 prompt
echo "數到五" | llm --stream               # 沒位置參數 → 讀 stdin；--stream 逐段即時印
llm 這張圖是什麼 --image ./cat.jpg          # --image 可重複，mime 由副檔名推
llm 生成一個傲嬌角色 --schema ./char.json   # --schema：JSON Schema 檔 → 結構化輸出
llm --help                                 # 用法（含反射旗標）也是從 struct 生出來的
# --endpoint 本身就是反射欄位 → 指向 file:// fixture 即可離線自測 CLI（免真後端）
llm 你好 --endpoint "file:///<abs>/test/fixtures/fake/chat/completions"
```

- **旗標分兩類**：
  - **固定旗標**（手寫）：`--stream`／`--image <檔>`（可重複）／`--schema <檔>`／`--config <檔>`／`--help`。
  - **連線／取樣旗標**：從 `llm::abi::Client` 的欄位**反射生成**（`--endpoint`／`--model`／`--temperature`／
    `--top-p`／`--max-tokens`／`--seed`…）。要多一個旋鈕＝在 `Client` 加一個欄位，旗標與 `--help` 自動長
    出來、零改動。欄位名底線換連字號（`api_key`→`--api-key`、`max_tokens`→`--max-tokens`）。
- **反射怎麼把「欄位名」對到值**：`glz::for_each_field(client, cb)` 只餵欄位參照、不給名，但它是
  fold-over-逗號運算子、**求值順序有保證**，配一個可變 `idx` 就能對到 `glz::reflect<Client>::keys[idx]`。
  跑兩次：一次走空 probe 建「合法旗標表＋`--help` 用法」，一次走真 `client` 把命令列字串 coerce 進欄位
  （`if constexpr` 型別分派，`std::optional<T>` 用 trait 拆 value_type 再包回）。只用 `for_each_field`＋`keys`
  兩個公開面，不碰 glaze 內部符號。
- **設定來源（後者覆寫前者）**：`Client{}` 內建預設 → **config 檔**（glaze 反射整份灌入）→ 命令列旗標。
  config 檔是**扁平 JSON**、鍵＝上面那些欄位名：
  ```json
  { "endpoint": "http://localhost:1234/v1/chat/completions", "model": "local-model", "temperature": 0.7 }
  ```
  config 檔路徑解析：`--config <檔>` ＞ 環境變數 `LLM_CLI_CONFIG` ＞ `~/.config/llm/config.json`。
  前二者是**明指**（讀不到就報用法錯）；家目錄那條是**探測**（不存在就靜默用預設，存在卻壞才報錯）。
  ⚠ **無 env 存值**：`LLM_CLI_CONFIG` 只「指定 config 檔路徑」，不存任何設定值本身。
- **退出碼（三段分流＋取消）**：`0`＝成功；`1`＝用法錯（未知旗標／缺 prompt／檔案讀不到／config JSON 壞）；
  `2`＝請求失敗（傳輸／後端錯）；`130`＝收到 SIGINT 取消（128+SIGINT，POSIX；handler 只做 atomic store
  戳 `Context::cancel()`，串流可乾淨中止）。
- **輸出走 handlers**：文字（串流逐段／非串流整段）吐 stdout、收尾補一個換行；錯誤走 `on_error` 吐 stderr
  帶「請求失敗：」前綴。CLI 不開 tools／modalities，故 `on_tool`／`on_media` 兩路不觸發。

## 接真後端（本機 LM Studio）

離線 fixture 是**回歸測試**（`test/cli_smoke.sh` 全走 `file://`、不連網）。要打**真的**本機 LM Studio，直接把 `llm` CLI 的 `--endpoint` 指過去（或寫進 config 檔）：

```bash
# 旗標直給
llm 用一句話介紹你自己 --endpoint http://localhost:1234/v1/chat/completions --model local-model

# 或寫進 config 檔（~/.config/llm/config.json 或 --config／env LLM_CLI_CONFIG 指定），之後裸跑
#   { "endpoint": "http://localhost:1234/v1/chat/completions", "model": "local-model", "timeout_ms": 120000 }
llm 用一句話介紹你自己
```

- **`endpoint`**：到 `/v1/chat/completions` 為止的完整 URL（不像舊 `from_env()` 幫你補尾巴——那個工廠函式已隨 L0 封存）。
- **`model`**：LM Studio 接受 `local-model`，會自動用**目前已載入**的模型；雲端 API（如 OpenAI）需填真實 model id、且 `api_key` 必給。
- **`timeout_ms`**：真後端（尤其 reasoning 模型）單次可能等數十秒，config 設個 `120000`（2 分鐘）寬裕些（`0`＝不設逾時；`file://` 離線讀檔不吃這欄）。
- ⚠ **真後端的行為（含錯誤路徑）要靠真後端驗**（離線 `file://` fixture 從不回錯）：上面「對外 C ABI」的後端錯誤護欄就是真後端驗揪出來、修在 `guard_backend_error` 的。`llm` CLI 走的是同一套 `http`＋請求／回應解析，行為一致。

**⚠ reasoning 模型的 `max_tokens` 陷阱（2026-07-14 真後端實測血淚，仍成立）**：本機掛載的是 **reasoning 模型**（`google/gemma-4-e4b`）——它把思考鏈放在 `message.reasoning_content`、答案才在 `message.content`，**兩者共用同一份 `max_tokens` 預算**。實測設一個看似夠用的小值（如 600）會讓 reasoning 把預算吃光，`content` 變**空字串**（`finish_reason` 也不會是乾淨的 `stop`）。**因此打 reasoning 模型時刻意不設 `--max-tokens`／config 不放 `max_tokens`**，交後端用 context 上限——實測這樣 `finish_reason: stop`、`content` 完整。若日後真要限長度，得先扣掉 reasoning 可能吃掉的份額，抓夠大的值（別學 600 那樣小）。

另一個連帶的坑：真後端回應**很慢**（reasoning 要想，單次可能 5～30 秒）——這也是 config 建議把 `timeout_ms` 設到 120 秒的原因，避免真等待被誤判成當掉。

## VSCode 除錯

以「**File > Open Folder**」開 `cllm` 這個資料夾（讓 `${workspaceFolder}=cllm`），然後：

- **Ctrl+Shift+B** → 跑預設任務「cmake: build」建置。
- **F5** → gdb/MinGW 除錯：F5 前會自動建置（`preLaunchTask`），在 `cli.cpp` 的 `cli::run`、`main.cpp` 的入口或 `cabi_request.cpp` 的 `build_body` 下中斷點試單步、看變數。傳統 header 下符號名無模組修飾，`break cli::run` 直接命中（不像具名模組要 `run@cli`）。⚠ `.vscode/launch.json` 的除錯目標若還指著舊 `try3` exe，換機時記得改指 `llm`（舊 demo exe 已封存）。

`.vscode/` 四個檔：`tasks.json`（cmake configure/build/run，走 `--preset`）、`launch.json`（cppdbg + MinGW gdb）、`settings.json`（CMake/UTF-8）、`c_cpp_properties.json`（IntelliSense 讀 `build/compile_commands.json`）。

## 編輯器整合（clangd／nvim／VSCode 共用一份設定）

**LSP／IntelliSense 的真相源是 `build/compile_commands.json`**：`CMakeLists.txt` 開了 `CMAKE_EXPORT_COMPILE_COMMANDS ON`，所以只要 `cmake --preset <xxx>` configure 過一次，這份檔案就在 `build/` 底下（`build/` 整包 gitignored，換機/清乾淨重建都會自動再生，不用手動維護）。VSCode 端 `c_cpp_properties.json` 本來就指到這份檔；nvim 端（LazyVim + clangd）另外靠下面這個檔接上。

- **[`.clangd`](.clangd)**（版控、根目錄）：clangd 預設只會往「當前檔案所在目錄」往上層找 `compile_commands.json`，不會自動探進 `build/`，所以明講 `CompilationDatabase: build`。這是 clangd 原生設定格式，nvim 的 clangd client 和 VSCode 的 clangd 擴充都吃——**一份設定兩邊共用、也進版控**，比在各編輯器設定裡各塞一次 `--compile-commands-dir` 乾淨。
  - vcpkg 的 include 路徑（`build/vcpkg_installed/x64-linux/include`）**不用額外處理**：`compile_commands.json` 裡每條指令本來就帶 `-isystem .../vcpkg_installed/x64-linux/include`，clangd 讀資料庫時原樣套用，實測過（`src/cabi_request.cpp` 這種吃 glaze 的檔案沒有 `glaze/glaze.hpp` 找不到的診斷）。
  - `.clangd` 還設了 `Remove`，濾掉 `-fmodules-ts`／`-fmodule-mapper=*`／`-fdeps-format=*` 三個旗標——這是 CMake＋Ninja 為了 C++20 modules 依賴掃描（P1689）自動塞給每個編譯單元的（即使本專案早就回歸傳統 header、不用 modules，Ninja generator 仍會加），clang 驅動器不認得會冒出 `Unknown argument` 診斷雜訊（不影響實際程式碼分析，純視覺雜訊，過濾掉即可）。
  - **VSCode 也吃這份 `.clangd`**：若改用 clangd 擴充（`llvm-vs-code-extensions.vscode-clangd`）取代 cpptools 的 IntelliSense，會自動套用同一份設定；目前 VSCode 側仍走 cpptools＋`c_cpp_properties.json`（見上「VSCode 除錯」一節），`.clangd` 是為 nvim 準備的，兩邊不衝突。
- **nvim（LazyVim）**：clangd 由 nvim-lspconfig 依附 C/C++ 檔案自動啟動，讀到根目錄的 `.clangd` 就會用 `build/compile_commands.json`；`CMakeLists.txt` 會被 `neocmakelsp` 接手（獨立 LSP，不需要額外設定）。實測：`nvim --headless src/cabi_request.cpp` 等 clangd 索引完，`vim.lsp.get_clients()` 看得到 `clangd`、`vim.diagnostic.get(0)` 乾淨（無 include 錯誤、無旗標雜訊）。
- **除錯要先自己 build**：VSCode 的 `launch.json` 用 `preLaunchTask` 在 F5 前自動跑 `cmake --build --preset mingw-debug`；**nvim 的 dap 不會跑這個 preLaunchTask**（那是 VSCode 專屬機制），用 nvim 除錯前要自己先手動 `cmake --build --preset linux-debug`（或對應平台的 preset）建一次，之後改了程式碼記得重建，不然中斷點會對不上舊的執行檔。
- `.vscode/launch.json` 的 `"type": "cppdbg"` 兩邊都不用改：nvim 端把 `cppdbg` 這個 adapter 名稱另外別名到 codelldb（設定在使用者的 nvim config repo，不在本專案範圍內）。`miDebuggerPath`（寫死 `C:/dev/mingw64/bin/gdb.exe`）、`MIMode`、`setupCommands` 幾欄是 cppdbg（gdb/MI）專屬，codelldb 會直接忽略，不影響 nvim 側除錯；檔案內已加註解說明。

## 設計要點

- **`-static` 靜態連結** libstdc++/libgcc/winpthread → 產出獨立 exe，**不依賴 `C:/dev/mingw64/bin` 在 PATH 上**。否則 gdb 啟動 exe 時會找不到 mingw runtime DLL 而失敗。（實測 `objdump -p` 只剩 KERNEL32＋系統 CRT。）
- **中文不亂碼**：`wmain` + `-municode` 取寬字元命令列（中文 argv 不壞）＋ `SetConsoleOutputCP(CP_UTF8)`。跨平台：Windows 專屬碼以 `#ifdef _WIN32` 包起，Linux 走標準 `main`＋原生 UTF-8 argv。
- **⚠ SAC（Smart App Control）偶發封鎖**：新編 exe 首次執行可能被「應用程式控制原則」擋（`Permission denied` / 「應用程式控制原則已封鎖此檔案」）。重新連結（產生新 hash）即放行，詳見 [common/gotchas](workflows/common/gotchas.md)。
- **⚠ 命令列 gdb 引號坑**：從 Git Bash 直接跑原生 `gdb.exe` 時，多字詞 `-ex "break foo"` 的引號分組會遺失。要在 shell 手動驅動 gdb 改用命令檔 `gdb --batch -x cmds.gdb`。**VSCode 的 cppdbg 走 MI 協定、不經 shell，不受此影響。**

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
bash test/cli_smoke.sh              # 離線黑箱煙霧測試 17/17 綠：輸出正確（含串流／結構化）、config 三層來源、
                                     #   退出碼 0/1/2 三段分流；中文輸出（繁中台詞）原樣 UTF-8、無亂碼
```

從乾淨 `rm -rf build` 重跑一次 configure＋build＋煙霧測試，結果一致（`.so`＋`llm` 都在、17/17 綠）。`linux-release`（`-O2`）也建置成功。

**產物名**：`.so` 兩邊都叫 `libcllm.so`。`llm` 執行檔在 Linux **不加副檔名**（`build/llm`）、Windows 是 `build/llm.exe`——CLI 是 unix filter，Linux 側刻意讓它就叫 `llm`。（對照：舊 demo exe 曾為「指令跨平台一致」硬掛 `set_target_properties(... SUFFIX ".exe")` 讓 Linux 也叫 `try3.exe`；那顆 exe 連同該約定已封存，新的 `llm` target 不再沿用。）

**Linux 特有踩坑**：目前實測下**沒踩到需要繞路的坑**——`find_package(CURL)` 直接吃到系統庫、vcpkg 預設 triplet 免設、glaze header-only 秒裝。唯一值得注意的是 vcpkg 這次解出的 glaze 版本是 **7.4.0**（Windows 側原記錄 7.8.4，manifest 沒釘版號，baseline 對到的版本會隨 vcpkg registry 更新而變動）；API 沒動過，行為未受影響，若要鎖版可在 `vcpkg.json` 加 `builtin-baseline` 或 `version>=`。
