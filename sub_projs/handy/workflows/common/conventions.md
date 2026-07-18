# 程式碼慣例 + code map（碰程式碼的工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（new-resident / 改住戶 / refactor…）共用這套。結構整理原則（被動）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../AGENTS.md)。

## 程式碼慣例

- **載體選擇看性質**（見 [dev-env 技術棧](../dev-env.md)）：純膠水緊接 LLM 慢呼叫 → Fennel（免建置）；有邏輯小工具 → Fennel/LuaJIT；純膠水零延遲 → shell/C++。
- **folder-as-callable**：資料夾型住戶入口固定名（`_exec` shell shim → `_exec.fnl` 本體），外層 `<name>.sh` 轉發、上 PATH。`_exec`／`.sh` 都要 `readlink -f` 解 symlink 才定位得到同層檔。
- **shell 安全轉義**：轉發使用者輸入到子命令時，每個參數單引號包起（`shquote`：`'` → `'\''`），吃得下中文/空白/旗標。
- **stdin 管線公民**：無位置參數則讀 stdin，兩者皆空印用法 exit 2。
- **退出碼透傳**：wrap 子命令時，子命令回幾就回幾（`os.execute` 的第三回傳值）。
- **單檔行數門檻 300 行**（與 [DEV-GUIDE 觸發 A](../../DEV-GUIDE.md) 一致），超了檢視是否該拆。
- **config 是資料不是碼**：`llme/configs/*.json` 只放 Client 欄位，**不放註解鍵**（glaze 嚴格，見 [gotchas](gotchas.md)）。

## 導航 index（code map）維護鏈

**優先級（衝突時依序保持一致）**：程式碼 > code map > 文檔。code map 與程式碼衝突 → 以程式碼為準，立即修 code map。
**日常規則**：① 改前先讀本 code map 找相關檔；② 新增/刪除原始碼檔或職責顯著改變 → 同步更新本表；③ 原始碼檔本身不加「對應 code map」註釋，反向查直接 grep 本檔。

## code map（哪個檔負責什麼）

| 檔案 | 載體 | 職責 |
|------|------|------|
| `llme.sh` | POSIX shell | llme 的**最外層轉發入口**：`readlink -f` 解 symlink → exec `llme/_exec`。上 PATH 的就是它。 |
| `llme/_exec` | POSIX shell | folder-as-callable **入口薄殼**：解 symlink → exec 同層 `_exec.fnl`。只為讓 `.fnl` 拿語法高亮而存在，無邏輯。 |
| `llme/_exec.fnl` | Fennel | llme **dispatcher 本體**：解析 `<endpoint>` → `configs/<ep>.json`；掃可用 endpoint（`ls`）；**auto-inject api key**（`LLME_KEY_<EP>` ＞ `<EP>_API_KEY`，偵測使用者是否自帶 `--api-key`）；`shquote` 轉義；組 `llm --config <cfg> [--api-key …] <argv>` 並 exec、透傳退出碼。 |
| `llme/configs/*.json` | 資料 | cllm config（`endpoint`/`model`/`api_key`/`timeout_ms`）。`_` 開頭＝模板/隱藏不列。 |
| `zhtw` | Fennel（單檔住戶）| 繁中翻譯薄包裝：烤死 `ENDPOINT`/`SYSTEM`/`PREFIX`/`FLAGS` 常數；argv 或 stdin 取輸入；`shquote` → 組 `llme deepseek <FLAGS> --system <SYSTEM> -- <前置+文字>` exec。改常數即換人格。 |

> 新住戶落地後在此表加列。目前住戶少、一個檔就夠；大了按住戶拆成 `common/code-map/` 多份子 index。
