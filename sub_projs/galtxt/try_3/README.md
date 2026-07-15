# try_3 — galtxt 玩具實驗場③：純 C++（傳統 header）

← [galtxt INDEX](../INDEX.md)

galtxt 第三條實驗線，與 [try_1](../try_1/README.md)（s7 Scheme）、[try_2](../try_2/README.md)（C++ 內嵌 Lua 5.5）並存、互為對照。

**這條完全 C++、純原生**——不嵌任何腳本 VM。（早期曾用 C++20 modules 當骨架示範，**已回歸傳統 header**：C++ 有 struct＋glaze 編譯期反射，接口的唯一真相源是 struct 本身，modules 那層抽象在此線畫蛇添足，拿掉。）建置走 **CMake + Ninja + vcpkg toolchain**（配 `CMakePresets.json` 釘死工具鏈），與 try_2 的手寫 `build.sh` 走不同路線，兩相對照。

目前狀態：**可建置、可 VSCode/gdb 除錯，兩塊地基＋上層 LLM 接口＋反射生成的 CLI 都長出來了；Windows／Linux（Manjaro）雙機皆已實測**。原始碼：`src/demo.{hpp,cpp}`（`sum_to`/`greet` 除錯示範）、`src/http.{hpp,cpp}`（native HTTP 傳輸）、`src/llm.{hpp,cpp}`（`llm::Client` ask 接口）、`src/llm_tool.{hpp,cpp}`（工具呼叫）、`src/llm_media.{hpp,cpp}`（多媒體/vision）、`src/llm_json.{hpp,cpp}`（結構化輸出）、`src/cli.{hpp,cpp}`（由 `llm::Client` 欄位反射生成的 `--flag` CLI）。`build/try3.exe` 一趟把全部離線 fixture 跑一遍：計算/字串、glaze 往返、HTTP file:// GET＋串流、`ask` 非串流/串流、工具呼叫、vision、結構化輸出，全綠——Windows 用 mingw preset、Linux 用 linux preset，兩邊產物都叫 `try3.exe`（詳見下方「跨平台」一節）。

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
cmake --build --preset mingw-debug  # 建置 → build/try3.exe
./build/try3.exe                    # 跑（預設 N=10）
./build/try3.exe 5 星野             # N=5、名字＝星野

# Linux（linux-debug／linux-release，指令完全同構）
cmake --preset linux-debug
cmake --build --preset linux-debug  # 建置 → build/try3.exe（見下方「跨平台」一節：Linux 也叫 .exe）
./build/try3.exe 5 星野
```

Release：把 `-debug` 換成 `-release`（`mingw-release` 或 `linux-release`）。

`cmake --list-presets` 只會列出當前平台的 preset——兩組 preset 各自掛了 `condition`（`hostSystemName` 判斷），Windows 只看得到 `mingw-*`、Linux 只看得到 `linux-*`，不用手動挑對的那組。

## 依賴（vcpkg manifest）

已接 **glaze**（反射式 JSON↔struct、header-only、超快）當第一個 vcpkg 庫，兼作整條鏈的驗證。用 **manifest 模式**（宣告式、隨專案版控）：

1. [`vcpkg.json`](vcpkg.json) 宣告依賴：
   ```json
   { "name": "try-3", "version-string": "0.0.1", "dependencies": ["glaze"] }
   ```
2. **★ MinGW triplet（最容易踩的雷）**：vcpkg 預設 triplet 是 `x64-windows`（MSVC 導向），在 MinGW 上會建置／連結失敗。`CMakePresets.json` 的 `cacheVariables` 已設（static，與本線 `-static` 獨立 exe 哲學一致）：
   ```json
   "VCPKG_TARGET_TRIPLET": "x64-mingw-static",
   "VCPKG_HOST_TRIPLET":   "x64-mingw-static"
   ```
   （要動態庫改 `x64-mingw-dynamic`，但那樣 exe 執行期要找 DLL，與本線 `-static` 相左。）
   **Linux 側 `linux-*` preset 沒設 `VCPKG_TARGET_TRIPLET`**：讓 vcpkg 用偵測到的預設 `x64-linux` 即可，不用比照 MinGW 特別指定。
3. `CMakeLists.txt`：`find_package(glaze CONFIG REQUIRED)` ＋ `target_link_libraries(try3 PRIVATE glaze::glaze)`。glaze 需 **C++23**（已把 `CMAKE_CXX_STANDARD` 提到 23，g++16 支援）。
4. `cmake --preset mingw-debug`（或 `linux-debug`）配置時 vcpkg 自動裝依賴（glaze header-only，約數秒；Linux 實測 7.4.0，約 1.4 秒裝完）。vcpkg 裝到 `build/vcpkg_installed/`（gitignored）。

**要再加庫**：`vcpkg.json` 的 `dependencies` 加一個名字 → `find_package`＋`target_link_libraries` → 重配置。實測 glaze 7.8.4：`glz::write_json(struct)` / `glz::read_json(struct, src)` 反射自動映射、中文原樣 UTF-8，`main.cpp` 的 `demo_json()` 有往返示範。

> ℹ glaze 這種現代 C++ 庫用**編譯期反射**：定義 `struct` 就自動 JSON↔struct，不用寫映射巨集（這正是 C++26 `std::meta` 反射要標準化的東西，glaze 現在就有）。

> ⚠ **glaze 預設遇未知鍵報錯**：真後端回應（OpenAI chat completion）欄位遠多於你 struct 挑的那幾個。用 `glz::read<glz::opts{.error_on_unknown_keys=false}>(obj, src)` 才會忽略多餘鍵、只填 struct 有的。`main.cpp` 的 `demo_http()` 就是這樣把 `choices[0].message.content` 台詞挑出來。

## native HTTP 傳輸（`src/http.{hpp,cpp}`）

`http`＝**純 C++ 的 HTTP 傳輸層**，對照 [try_2](../try_2/README.md) 的 `native/http.c`（那條是 C＋Lua 綁定）。拋開 Lua C API 後 API 縫變乾淨：

```cpp
#include "http.hpp"
http::Request  req{ .url = "...", .method = "POST", .headers = {"Content-Type: application/json"}, .body = "..." };
http::Response r = http::request(req);                    // 非串流：傳輸失敗 throw std::runtime_error
int status = http::stream(req, [](std::string_view chunk){ /* 逐塊 raw bytes */ return true; });  // 回 false 中止
```

- **分層**（同 try_2）：C++ 是**笨管子**——只管 TLS＋HTTP round-trip、串流時逐塊把 raw bytes 交回呼；**SSE 拆框／UTF-8 分批／JSON 編解全留上層**（傳輸層語言中立、改起來便宜）。
- **介面/實作分離**：`http.hpp` 只放乾淨介面（`Request`/`Response`/`OnData`/`request`/`stream`）；**系統標頭（`windows.h`／`winhttp.h`／`curl.h`）只在 `http.cpp` include，不外洩到 header**——傳統 header 的好處，取用端 `main.cpp` 不被 `windows.h` 汙染。
- **平台分流**：Windows＝**WinHTTP**（系統內建、Schannel TLS，零安裝；連結 `winhttp`）；Linux/Mac＝**libcurl**（`find_package(CURL)`＋`CURL::libcurl`，需在 `vcpkg.json` 加 `curl`——目前僅 Windows 實測）。
- **`file://` 特例**：兩平台共用，直接讀檔當 200 回應——保住**離線 fixture 測試 harness**（WinHTTP 不支援 `file://`，非在這層處理不可）。fixture 在 `test/fixtures/{fake,fake_stream}/chat/completions`（假 chat completion／SSE 回應，版控當測試資料）。
- **fixture 路徑不寫死**：`CMakeLists` 用 `target_compile_definitions(... TRY3_SOURCE_DIR="${CMAKE_SOURCE_DIR}")` 注入源碼根，`main.cpp` 的 demo 據此組 `file://` 路徑（比照 try_2 `_path.lua` 不綁機器路徑的精神）。
- exe 仍**全靜態獨立**：`objdump -p` 只剩 KERNEL32＋系統 CRT＋WINHTTP.dll（Windows 系統內建、必在），無 mingw runtime。

> 這是本線「native HTTP 傳輸先」的落地：先把傳輸下沉成純原生 `.cpp`，JSON 已有 glaze，**真後端 round-trip＋SSE 串流的縫都在**——上層 ask 接口（下一節）與接真後端（`--real`，見後面「接真後端」一節）都已經長出來了。
>
> **★ 上層接口方向定調（不搬 schema 表）**：try_1/try_2 的 schema 表是動態語言缺靜態反射的**補償拐杖**；C++ 有 struct＋glaze 編譯期反射，**struct 本身就是唯一真相源**。往上長 ask 接口時，JSON 對映／CLI 旗標／型別解析／驗證**全從欄位反射生成**（驗證還能移到編譯期），不把 Lua/s7 的 schema 表搬過來。

## 上層 LLM 接口（`src/llm*.{hpp,cpp}`）

在 http＋glaze 兩塊地基上長的 LLM 接口，全部落地「**struct＝唯一真相源**」：請求/回應都是 struct，glaze 反射自動 JSON↔struct，零 schema 表。命名循 `http::` 慣例（`llm::` namespace、`ask` 動詞、PascalCase 型別）。

**`llm::Client`（`llm.hpp`）＝核心**：持 endpoint＋取樣設定的呼叫端。

```cpp
#include "llm.hpp"
llm::Client client{ .endpoint = "https://…/chat/completions" };
client.api_key = "sk-…";                 // 選填 → Authorization: Bearer
client.temperature = 0.7f;               // 取樣屬性全 std::optional，未設就不送、交後端默認
std::string ans = client.ask("你好");    // 非串流
client.ask("你好", true, [](std::string_view delta){ …; return true; });  // 串流（回 false 中止）
```

- 取樣屬性（`model`/`temperature`/`top_p`/`max_tokens`/`presence_penalty`/`frequency_penalty`/`seed`）**全選填**，`std::optional` 未設時 glaze 略過（`skip_null_members`），交後端用默認（⚠ `model` 例外：本地伺服器可省、OpenAI 雲端必填）。
- 串流＝上層拆 SSE（跨塊行緩衝、逐行解 `data:`、`[DONE]` 收工）；回呼 `OnDelta` 收 `string_view`（零複製，與 `http::OnData` 一致）。
- 共用件：`llm::post()`（傳輸重用）＋ `llm::apply_sampling()`（取樣屬性 DRY 灌入任意請求 struct）——三個擴充都靠它們，不重造傳輸。

**三個擴充（各一組 `llm_*` 檔）**：

| 檔 | 能力 | 入口 | 「struct＝真相源」怎麼體現 |
|---|---|---|---|
| `llm_tool` | 工具呼叫（function calling）| `make_tool<Args>()`＋`ask_tools()` | `Args` struct 反射生成工具**參數 schema**（`glz::write_json_schema`）；回來的 `ToolCall.arguments`（JSON 字串）再反射解回 `Args` |
| `llm_media` | 多媒體/vision | `image_from_file`/`image_from_url`＋`ask_vision()` | 多模態 `content` 陣列（文字＋圖）；本地檔→base64 data URI（`glz::write_base64`）|
| `llm_json` | 結構化輸出 | `ask_as<T>()` | **丟 struct `T` 進、拿 `T` 出**：`T` 反射生成 schema 塞進 `response_format` 約束模型，回來的 JSON 再反射回 `T` |

```cpp
// 工具呼叫：Args struct 同時生成 schema、又解回 arguments
struct GetWeather { std::string city; std::string unit; };
auto calls = llm::ask_tools(client, "東京天氣如何？", { llm::make_tool<GetWeather>("get_weather", "查天氣") });
GetWeather a{}; glz::read_json(a, calls[0].arguments);   // a.city="東京" …

// 多媒體：文字＋圖（URL 或本地檔 base64）
auto img = llm::image_from_file("photo.png", "image/png");
std::string ans = llm::ask_vision(client, "這是什麼？", { img });

// 結構化輸出：丟 struct 進、拿 struct 出
std::optional<Character> c = llm::ask_as<Character>(client, "生成一個傲嬌角色");
```

- 三個擴充都**非串流**（先做這段），各自的請求/回應 struct 放在**唯一命名的內部 namespace**（`tool_impl`/`media_impl`/`json_impl`）——★ 不可用匿名 namespace：GCC 把匿名 namespace 一律 mangle 成 `_GLOBAL__N_1`，同名 struct（`ReqBody`…）的 glaze `quoted_key_v` COMDAT section 會跨 TU 撞名、大小不同→折疊後給錯 JSON 鍵（見 [common/gotchas](../workflows/common/gotchas.md)）。
- ★ **生 schema 一定要走 [`llm_schema.hpp`](src/llm_schema.hpp) 的 `schema_of<T>()`，別用裸的 `glz::write_json_schema<T>()`**（真後端實測，2026-07-14）：後者用**預設 opts** 時不生 `required`，而 JSON Schema 下沒列進 `required` 的欄位就是**選填**——後端的約束解碼只保證「不多給鍵」、不保證「每個欄位都給」。實測 LM Studio + gemma 對 `Character{name,affection,lines}` **只吐了 `{"name":…}`**。glaze 其實有這能力，只是要開 `error_on_missing_keys`（它的判準：開了這個選項且成員非 nullable ⇒ required），`schema_of<T>()` 就是把這組 opts 包起來——`required` 自動生成，且 `std::optional<…>` 欄位**正確地不會**被標成必填。**結構化輸出（`ask_as`）與工具呼叫（`make_tool`）兩處共用**（工具參數是同一個坑，容易漏掉）。⚠ **這種缺陷離線 fixture 驗不出來**——假回應是自己寫的，欄位當然齊。
- 全部離線可測：endpoint 給 `file://` 指 `test/fixtures/{fake,fake_stream,fake_tool,fake_json}/chat/completions`，`main.cpp` 的 `demo_*()` 端到端跑過。

## CLI（由 `llm::Client` 欄位反射生成，`src/cli.{hpp,cpp}`）

本線主張的最後一哩：try_1/try_2 從一張 schema 表生 `--flag` CLI，try_3 **沒有 schema 表**——旗標名、值型別解析全從 `llm::Client` 的欄位**反射**推出。要多一個取樣旋鈕＝在 `Client` 加一個欄位，CLI 旗標自動長出來、零改動（與送/收 JSON 同一個真相源）。

```bash
# base 走 llm::from_env()（預設打本機 LM Studio），反射的旗標只做「覆寫」
./build/try3.exe --prompt "用一句話介紹你自己"
./build/try3.exe --prompt "數到五" --stream --temperature 0.7
./build/try3.exe --help                       # 用法也是從欄位反射印出來的
# --endpoint 本身就是反射欄位 → 指向 file:// fixture 即可離線自測 CLI
./build/try3.exe --prompt "你好" --endpoint "file:///<abs>/test/fixtures/fake/chat/completions"
```

- **怎麼把「欄位名」對到「欄位值」**：`glz::for_each_field(client, cb)` 只餵欄位參照、不給名字，但它是 fold-over-逗號運算子、**求值順序有保證**，所以配一個可變 `idx` 就能對到 `glz::reflect<Client>::keys[idx]`。全程只用 `for_each_field` ＋ `keys` 兩個公開面，**不碰 glaze 內部符號**（`to_tie`/`get_member`）。
- **兩次反射**：一次走空 probe 建「合法旗標表＋`--help` 用法」；一次走真 `client` 把命令列字串 coerce 進欄位。型別分派用 `if constexpr`（`std::string` 直接指派、數值走 `std::stof`/`stoi`/`stol`），`std::optional<T>` 用 trait 拆出 `value_type` 再解析、包回 optional。
- **base＝`from_env()`、旗標只覆寫**：`--model`/`--temperature` 語意單純就是「蓋掉 env 預設」；而 `--endpoint file://` 讓 CLI 天生可離線自測——拿到 fixture 台詞就**反證了反射對 `endpoint` 的寫入真的生效**（否則會去打 localhost、不會讀到檔）。
- **三個非欄位的特例旗標**（不在 `Client` 上、手寫）：`--prompt`（問題本體，必填）、`--stream`（布林、注入 stdout callback 即時印，比照 try_2 `cli.lua`）、`--help`/`-h`。旗標名＝欄位名把底線換連字號（`api_key`→`--api-key`、`max_tokens`→`--max-tokens`），對齊 try_2 `flag_of`。
- **模式分流**（`main.cpp`）：`--real` 打真後端 demo；出現**任何 `--旗標`** 就進 CLI（把整包命令列交給 `cli::run`）；兩者皆無才落到原本的純位置參數 demo（`N` 名字）。三條路互斥，原離線 fixture 回歸套件不受影響。

## 接真後端（本機 LM Studio）

離線 fixture（`demo_*()`）是**回歸測試**，`./build/try3.exe` 不帶旗標永遠跑這條、不連網。要打**真的**本機 LM Studio，另開一條路徑：

```bash
./build/try3.exe --real
```

`--real` 與離線 demo **互斥**（給了這個旗標就只跑真後端示範、略過所有離線 fixture），對應 `main.cpp` 的 `demo_real()`。

**環境變數**（`llm::from_env()`，`src/llm.{hpp,cpp}`；命名與預設值對齊 try_1/try_2 兩條線既有慣例，三線一致）：

| 變數 | 預設 | 說明 |
|---|---|---|
| `AI_CORE_LLM_BASE_URL` | `http://localhost:1234/v1` | 到 `/v1` 為止；`from_env()` 自己接上 `/chat/completions` 組成 `endpoint` |
| `AI_CORE_LLM_MODEL` | `local-model` | 實測 LM Studio 接受這個名字，會自動用**目前已載入**的模型；雲端 API（如 OpenAI）需填真實 model id |
| `AI_CORE_LLM_API_KEY` | （空） | 有值才送 `Authorization: Bearer`；本機通常留空 |

`llm::Client` 本身仍可照舊手動指定 `endpoint`（離線 fixture 就是這樣用 `file://`，不經過 `from_env()`）；`from_env()` 只是另外新增的工廠函式，不影響既有建構方式。`from_env()` 額外把 `client.timeout_ms` 設成 120000（2 分鐘）——真後端（尤其 reasoning 模型）單次可能要等數十秒，給寬裕的逾時空間（`0`＝不設逾時，`file://` 離線讀檔本來就不吃這個欄位、不受影響）。

`demo_real()` 對真後端示範四種能力（`ask` 非串流、`ask` 串流、`ask_as<T>` 結構化輸出、`ask_tools` 工具呼叫），全用繁體中文 prompt，逐步印進度字樣（讓人知道還在等模型想、沒當掉）。

**⚠ reasoning 模型的 `max_tokens` 陷阱（2026-07-14 真後端實測血淚）**：本機掛載的是 **reasoning 模型**（`google/gemma-4-e4b`）——它把思考鏈放在 `message.reasoning_content`、答案才在 `message.content`，**兩者共用同一份 `max_tokens` 預算**。實測設一個看似夠用的小值（如 600）會讓 reasoning 把預算吃光，`content` 變**空字串**（`finish_reason` 也不會是乾淨的 `stop`）。**因此 `demo_real()`／`from_env()` 刻意完全不設 `max_tokens`**（`client.max_tokens` 維持 `nullopt`），交後端用 context 上限——實測這樣 `finish_reason: stop`、`content` 完整。若日後真的要限制長度，得先扣掉 reasoning 可能吃掉的份額，抓夠大的值（別學 600 那樣小）。

另一個連帶的坑：真後端回應**很慢**（reasoning 要想，單次可能 5～30 秒）——這也是 `from_env()` 把逾時設到 120 秒、且 demo 逐步印進度的原因，避免真等待被誤判成當掉。

## VSCode 除錯

以「**File > Open Folder**」開 `try_3` 這個資料夾（讓 `${workspaceFolder}=try_3`），然後：

- **Ctrl+Shift+B** → 跑預設任務「cmake: build」建置。
- **F5** → 「Debug try3 (gdb/MinGW)」：F5 前會自動建置（`preLaunchTask`），在 `demo.cpp` 的 `sum_to` 迴圈、`greet` 或 `main.cpp` 的 `run` 下中斷點試單步、看變數。傳統 header 下符號名無模組修飾，`break sum_to` 直接命中（不像具名模組要 `sum_to@demo`）。

`.vscode/` 四個檔：`tasks.json`（cmake configure/build/run，走 `--preset`）、`launch.json`（cppdbg + MinGW gdb）、`settings.json`（CMake/UTF-8）、`c_cpp_properties.json`（IntelliSense 讀 `build/compile_commands.json`）。

## 編輯器整合（clangd／nvim／VSCode 共用一份設定）

**LSP／IntelliSense 的真相源是 `build/compile_commands.json`**：`CMakeLists.txt` 開了 `CMAKE_EXPORT_COMPILE_COMMANDS ON`，所以只要 `cmake --preset <xxx>` configure 過一次，這份檔案就在 `build/` 底下（`build/` 整包 gitignored，換機/清乾淨重建都會自動再生，不用手動維護）。VSCode 端 `c_cpp_properties.json` 本來就指到這份檔；nvim 端（LazyVim + clangd）另外靠下面這個檔接上。

- **[`.clangd`](.clangd)**（版控、根目錄）：clangd 預設只會往「當前檔案所在目錄」往上層找 `compile_commands.json`，不會自動探進 `build/`，所以明講 `CompilationDatabase: build`。這是 clangd 原生設定格式，nvim 的 clangd client 和 VSCode 的 clangd 擴充都吃——**一份設定兩邊共用、也進版控**，比在各編輯器設定裡各塞一次 `--compile-commands-dir` 乾淨。
  - vcpkg 的 include 路徑（`build/vcpkg_installed/x64-linux/include`）**不用額外處理**：`compile_commands.json` 裡每條指令本來就帶 `-isystem .../vcpkg_installed/x64-linux/include`，clangd 讀資料庫時原樣套用，實測過（`src/llm.cpp` 這種吃 glaze 的檔案沒有 `glaze/glaze.hpp` 找不到的診斷）。
  - `.clangd` 還設了 `Remove`，濾掉 `-fmodules-ts`／`-fmodule-mapper=*`／`-fdeps-format=*` 三個旗標——這是 CMake＋Ninja 為了 C++20 modules 依賴掃描（P1689）自動塞給每個編譯單元的（即使本專案早就回歸傳統 header、不用 modules，Ninja generator 仍會加），clang 驅動器不認得會冒出 `Unknown argument` 診斷雜訊（不影響實際程式碼分析，純視覺雜訊，過濾掉即可）。
  - **VSCode 也吃這份 `.clangd`**：若改用 clangd 擴充（`llvm-vs-code-extensions.vscode-clangd`）取代 cpptools 的 IntelliSense，會自動套用同一份設定；目前 VSCode 側仍走 cpptools＋`c_cpp_properties.json`（見上「VSCode 除錯」一節），`.clangd` 是為 nvim 準備的，兩邊不衝突。
- **nvim（LazyVim）**：clangd 由 nvim-lspconfig 依附 C/C++ 檔案自動啟動，讀到根目錄的 `.clangd` 就會用 `build/compile_commands.json`；`CMakeLists.txt` 會被 `neocmakelsp` 接手（獨立 LSP，不需要額外設定）。實測：`nvim --headless src/llm.cpp` 等 clangd 索引完，`vim.lsp.get_clients()` 看得到 `clangd`、`vim.diagnostic.get(0)` 乾淨（無 include 錯誤、無旗標雜訊）。
- **除錯要先自己 build**：VSCode 的 `launch.json` 用 `preLaunchTask` 在 F5 前自動跑 `cmake --build --preset mingw-debug`；**nvim 的 dap 不會跑這個 preLaunchTask**（那是 VSCode 專屬機制），用 nvim 除錯前要自己先手動 `cmake --build --preset linux-debug`（或對應平台的 preset）建一次，之後改了程式碼記得重建，不然中斷點會對不上舊的執行檔。
- `.vscode/launch.json` 的 `"type": "cppdbg"` 兩邊都不用改：nvim 端把 `cppdbg` 這個 adapter 名稱另外別名到 codelldb（設定在使用者的 nvim config repo，不在本專案範圍內）。`miDebuggerPath`（寫死 `C:/dev/mingw64/bin/gdb.exe`）、`MIMode`、`setupCommands` 幾欄是 cppdbg（gdb/MI）專屬，codelldb 會直接忽略，不影響 nvim 側除錯；檔案內已加註解說明。

## 設計要點（吸取 try_2 踩過的坑）

- **`-static` 靜態連結** libstdc++/libgcc/winpthread → 產出獨立 exe，**不依賴 `C:/dev/mingw64/bin` 在 PATH 上**。否則 gdb 啟動 exe 時會找不到 mingw runtime DLL 而失敗。（實測 `objdump -p` 只剩 KERNEL32＋系統 CRT。）
- **中文不亂碼**：`wmain` + `-municode` 取寬字元命令列（中文 argv 不壞）＋ `SetConsoleOutputCP(CP_UTF8)`。跨平台：Windows 專屬碼以 `#ifdef _WIN32` 包起，Linux 走標準 `main`＋原生 UTF-8 argv。
- **⚠ SAC（Smart App Control）偶發封鎖**：新編 exe 首次執行可能被「應用程式控制原則」擋（`Permission denied` / 「應用程式控制原則已封鎖此檔案」）。重新連結（產生新 hash）即放行——與 try_2 同一個坑，詳見 [common/gotchas](../workflows/common/gotchas.md)。
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
cmake --preset linux-debug          # configure：glaze:x64-linux@7.4.0 vcpkg 自動裝、約 1.4 秒；系統 CURL 直接命中
cmake --build --preset linux-debug  # 16 個編譯單元全過，link 出 build/try3.exe
./build/try3.exe 5 星野             # 全部離線 demo 綠：glaze 往返、file:// HTTP GET/串流、ask 非串流/串流、
                                     # 工具呼叫、vision、結構化輸出；中文輸出（繁中台詞/城市名）原樣 UTF-8、無亂碼
```

從乾淨 `rm -rf build` 重跑一次 configure＋build＋執行，結果一致（exit 0）。`linux-release`（`-O2`）也建置成功、跑起來同樣全綠。

**產物名**：CMake 在 Linux 預設不加副檔名（會是 `try3`），但這裡刻意在 `CMakeLists.txt` 加了一行 `set_target_properties(try3 PROPERTIES SUFFIX ".exe")`，比照 try_2 `build.sh` 的慣例（「產物名一律帶 `.exe`，Linux 不在意副檔名、指令跨平台一致」）——這樣兩邊都用 `./build/try3.exe` 這條指令，不用因平台切換記兩套檔名。此設定對 Windows 端無副作用（MinGW 本來就輸出 `.exe`）。

**Linux 特有踩坑**：目前實測下**沒踩到需要繞路的坑**——`find_package(CURL)` 直接吃到系統庫、vcpkg 預設 triplet 免設、glaze header-only 秒裝。唯一值得注意的是 vcpkg 這次解出的 glaze 版本是 **7.4.0**（Windows 側原記錄 7.8.4，manifest 沒釘版號，baseline 對到的版本會隨 vcpkg registry 更新而變動）；API 沒動過，行為未受影響，若要鎖版可在 `vcpkg.json` 加 `builtin-baseline` 或 `version>=`。
