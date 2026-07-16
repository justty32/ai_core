# `llm` CLI 手冊（unix filter）

← [docs 索引](README.md)｜[技術入口 README](../README.md)｜[C ABI 參考](c-abi-reference.md)｜[C++ 鏡像](cpp-mirror-reference.md)

`llm` 是建在 [C++ 薄鏡像 `cabi.hpp`](cpp-mirror-reference.md) 之上的第二個交付物：一支 unix 風格過濾器。**prompt 走位置參數＋導管 stdin，可合體**（見下）；答案吐 stdout、診斷吐 stderr。可當 `llm --help` 的延伸（`--help` 自身也是從 `Client` 欄位反射生出來的）。

> 產物名跨平台：Linux `build/llm`（無副檔名）、Windows `build/llm.exe`。

## 概要（SYNOPSIS）

```
llm [旗標...] [prompt...]
```

**prompt 來源**（位置參數 × 導管 stdin）：

| 給了什麼 | prompt 是 |
|----------|-----------|
| 只有位置參數 | 位置參數（空格拼接）|
| 只有導管 stdin | stdin 整段 |
| 兩者都有、無「-」 | 位置參數＋空行＋stdin（**指令在前、資料在後**）|
| 位置參數含「-」 | 「-」被 stdin 整段替換（**明指插入點**；stdin 須為導管/檔案）|

互動終端一律不讀 stdin（不卡住）；兩者皆空＝用法錯。

## 範例

```bash
llm 用一句話介紹你自己                      # 位置參數當 prompt
echo "數到五" | llm --stream               # 沒位置參數 → 讀 stdin；--stream 逐段即時印
cat report.txt | llm 總結這份               # prompt＋stdin 合體（指令在前、資料在後）
git diff | llm 把 - 寫成 commit 訊息        # 「-」明指 stdin 插入點
llm 這張圖是什麼 --image ./cat.jpg          # --image 可重複，mime 由副檔名推
llm 生成一個傲嬌角色 --schema ./char.json   # --schema：JSON Schema 檔 → 結構化輸出
llm 東京天氣如何 --tool ./get_weather.json  # --tool 可重複；tool_calls 一行一則 JSON 吐 stdout
llm 說你好 --modality audio=./cfg.json --media-out ./out   # 產出媒體落檔、路徑吐 stdout
llm --help                                 # 用法（含反射旗標）也是從 struct 生出來的
llm 你好 --endpoint http://localhost:1234/v1/chat/completions --model local-model
# 離線自測：--endpoint 指向 test/fixtures 的假回應（免真後端、不連網）
llm 你好 --endpoint "file:///<abs>/test/fixtures/fake/chat/completions"
```

---

## 旗標分兩類

### (1) 固定旗標（手寫）

| 旗標 | 說明 |
|------|------|
| `--stream` | 串流逐段吐 stdout（布林，無值）|
| `--image <檔>` | 夾帶輸入媒體（**可重複**；mime 由副檔名推）|
| `--schema <檔>` | JSON Schema 檔，要求結構化輸出 |
| `--tool <檔>` | 工具定義檔（**可重複**）：`{"name","description","parameters"}`，`description` 選填、容忍多餘鍵；模型的 tool_calls 一行一則 JSON 吐 stdout（見下「輸出與 handler」）|
| `--modality <名[=檔]>` | 要求輸出模態（**可重複**）：如 `audio` 或 `audio=cfg.json`（`=` 後接該模態參數的 JSON 檔）|
| `--media-out <目錄>` | 產出媒體落檔目錄（須已存在）；沒給＝收到媒體時 stderr 明說丟棄 |
| `--config <檔>` | 設定檔（扁平 JSON，對應下列連線／取樣欄位）|
| `--` | 分隔符：其後所有參數一律當 prompt（unix 慣例）|
| `--help`, `-h` | 顯示用法 |

`--image` 支援的副檔名 → mime：`png`／`jpg`/`jpeg`／`gif`／`webp`（圖）、`wav`／`mp3`（音）；認不得退成 `application/octet-stream`。落檔（`--media-out`）用同一張表反推副檔名，認不得退成 `bin`。

### (2) 連線／取樣旗標（從 `llm::abi::Client` 欄位反射生成）

要多一個旋鈕 = 在 `Client` 加一個欄位，旗標與 `--help` 自動長出來、零改動。欄位名底線換連字號。

| 旗標 | 型別 | 對應欄位 | 未給語意 |
|------|------|----------|----------|
| `--endpoint <url>` | 字串 | `endpoint` | 內建 `http://localhost:1234/v1/chat/completions` |
| `--api-key <s>` | 字串 | `api_key` | 不送 `Authorization` |
| `--model <s>` | 字串（選填）| `model` | 不送（本地後端用已載入模型；雲端必填）|
| `--timeout-ms <ms>` | 整數（毫秒）| `timeout_ms` | `0` = 不設逾時 |
| `--temperature <f>` | 小數 | `temperature` | 不送、交後端默認 |
| `--top-p <f>` | 小數 | `top_p` | 同上 |
| `--presence-penalty <f>` | 小數 | `presence_penalty` | 同上 |
| `--frequency-penalty <f>` | 小數 | `frequency_penalty` | 同上 |
| `--max-tokens <i>` | 整數 | `max_tokens` | 同上（⚠ reasoning 模型見下）|
| `--seed <i>` | 整數 | `seed` | 同上 |

> 上表由反射生成，**以 `llm --help` 的實際輸出為準**——在 `Client` 加欄位時本表可能落後。

---

## 設定來源（三層，後者覆寫前者）

```
Client{} 內建預設  →  config 檔（glaze 反射整份灌入）  →  命令列旗標
```

**config 檔是扁平 JSON**，鍵 = 上面那些欄位名（**底線版**，非連字號）：

```json
{
  "endpoint": "http://localhost:1234/v1/chat/completions",
  "model": "local-model",
  "temperature": 0.7,
  "timeout_ms": 120000
}
```

**config 檔路徑解析**（優先序）：

1. `--config <檔>` — **明指**，讀不到就報用法錯。
2. 環境變數 `LLM_CLI_CONFIG` — **明指**，讀不到就報用法錯。
3. `~/.config/llm/config.json` — **探測**，不存在就靜默用預設；存在卻壞（JSON 解析失敗）才報錯。

> ⚠ **`LLM_CLI_CONFIG` 只「指定 config 檔路徑」，不存任何設定值本身**。要存值請寫進 config 檔。

---

## 輸出與 handler（四路全開，機制在 `src/cli_output.*`）

- **文字**（串流逐段／非串流整段）吐 **stdout**，成功收尾補一個換行。
- **tool_calls**（`--tool` 給了定義才會來）**一行一則 JSON** 吐 stdout（jq 友善）：
  `{"tool":"get_weather","id":"call_abc","arguments":{"city":"東京"}}`——`arguments` 為後端原文內嵌。
- **產出媒體**落檔到 `--media-out <目錄>`（`llm-media-<n>.<ext>`，ext 由 mime 反推），**落檔路徑逐行吐 stdout**；沒給 `--media-out` ＝ stderr 明說丟棄（不無聲吞、也不驚喜寫檔）。
- **錯誤**走 `on_error` 吐 **stderr**，帶「請求失敗：」前綴；其餘用法錯附「`llm --help` 看用法」提示。

## 退出碼（三段分流 + 取消）

| 碼 | 意義 | 觸發例 |
|----|------|--------|
| `0` | 成功 | 正常回答、`--help` |
| `1` | 用法錯 | 未知旗標、缺 prompt（stdin 也空）、「-」但 stdin 是互動終端、`--image`/`--schema`/`--config`/`--tool`/`--modality` 檔讀不到、config／工具定義 JSON 壞、`--media-out` 不是目錄、數值旗標型別錯（如 `--temperature abc`）|
| `2` | 請求失敗 | 傳輸失敗（連不上 endpoint）／後端回錯／媒體落檔失敗（結果真掉了，不報成功）|
| `130` | 收到 SIGINT 取消 | `Ctrl-C`（128+SIGINT，POSIX）；handler 只做 atomic store 戳 `Context::cancel()`，串流可乾淨中止 |

---

## 接真後端（本機 LM Studio）

離線 fixture 是回歸測試（全走 `file://`、不連網）。要打真的本機 LM Studio，把 `--endpoint` 指過去（或寫進 config 檔）：

```bash
llm 用一句話介紹你自己 --endpoint http://localhost:1234/v1/chat/completions --model local-model
```

- **`model`**：LM Studio 接受 `local-model`（用目前已載入模型）；雲端 API 需真實 model id + `api_key`。
- **`timeout_ms`**：真後端（尤其 reasoning 模型）單次可能等數十秒，config 設 `120000` 寬裕些。
- ⚠ **reasoning 模型的 `max_tokens` 陷阱**：思考鏈與答案共用同一份預算，設太小（如 600）會讓 reasoning 吃光、`content` 變空。打 reasoning 模型時**刻意不設 `--max-tokens`**，交後端用 context 上限。詳見 [README「接真後端」](../README.md#接真後端本機-lm-studio)。

## 離線自測

```bash
bash test/cli_smoke.sh    # 離線黑箱煙霧測試（31/31）：驅動 build/llm 打 file:// fixtures
```

`--endpoint file://<絕對路徑>` 就是反射欄位，`cli_smoke.sh` 在執行期用 `$ROOT` 組出 `file://` 路徑餵進去，驗輸出正確（含串流／結構化／stdin 合體／「-」插入點）、tools（JSON 行）／媒體落檔（含內容）、config 三層來源、退出碼 0/1/2 三段分流。

---

## 相關文檔

- **底層 C ABI（CLI 就是它的消費端）** → [C ABI 參考](c-abi-reference.md)
- **CLI 走的 C++ 鏡像** → [C++ 鏡像參考](cpp-mirror-reference.md)
- **建置 / 跨平台 / 除錯** → [README.md](../README.md)
- **視覺總覽** → [`overview.html`](overview.html)
