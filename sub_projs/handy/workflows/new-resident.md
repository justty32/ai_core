# new-resident — 加一個新住戶（寫個小腳本包裝某能力）

← [INDEX](../INDEX.md)｜[WORKFLOWS](../WORKFLOWS.md)

handy 是試驗田，**往裡丟小程式＝加住戶**。這是加住戶的方法論與慣例。

## 方法論（handy 的鐵律具體化）

1. **拿現成用腳本包**——先看 [INDEX「不重造」](../INDEX.md)有沒有現成件可 wrap（cllm `llm`、`libcllm.so`、`llm-login`…），別憑記憶重造。
2. **資料夾＝callable**：住戶大到需要配置/多檔 → 做成資料夾，入口固定名（如 llme 的 `_exec` shell shim → `_exec.fnl` 本體）＋外層 `<name>.sh` 轉發、上 PATH。單純一招 → 單檔（如 `zhtw`，檔頭註解當入口）。
3. **選載體看性質**（見 [dev-env 技術棧](dev-env.md)）：純膠水但緊接 LLM 慢呼叫 → Fennel 免建置；有邏輯的小工具 → Fennel/LuaJIT；純膠水零延遲 → shell/C++。
4. **先執行再說，邊做邊生慣例**——不開工前立法。

## 站在現有住戶上的通用慣例（照抄）

- **預設後端 `deepseek`**：新住戶要打 LLM 就轉呼 `llme deepseek`（key 自動注入，見 [llme/README](../llme/README.md)）。
- **stdin 管線公民**（照 `zhtw`）：有位置參數＝拿參數；無參數＝讀 stdin（`cat file | 住戶`）；兩者皆空印用法 exit 2。讓住戶能進 pipeline。
- **shell 安全轉義**：轉發使用者輸入到 `llm`/子命令時，每個參數單引號包起（見 `zhtw`/`_exec.fnl` 的 `shquote`），才吃得下中文/空白/旗標。
- **人格薄包裝**（照 `zhtw`）：把 endpoint／取樣參數／`--system` 人格烤死成幾個常數，只留使用者輸入當變數。改常數即換人格。

## 落地後

- 住戶入口寫清楚（資料夾型→`README.md`；單檔→檔頭註解）。
- 在 [INDEX「住戶」表](../INDEX.md)加一列。
- 有 open 待續 → [SESSION-LOG](../SESSION-LOG.md)；踩到共通坑 → [gotchas](common/gotchas.md)。
- 驗證：驅動它跑真的（見 [dev-env 驗證](dev-env.md)）。
