# code_04 — 原型 Demo 與測試說明書

> **一句話講完**：想看這些東西怎麼動？看這裡就對了。我們準備了三個 Demo、兩套自動測試（共 161 個斷言全過），還有一些範例 Function 讓你玩。

---

## 1. 三個端到端 Demo（邊跑邊學）

這三個檔案既是範例，也是實戰測試：

### `demos/reliable_code_qa.py` — 馴服隨機的 AI
- **看點**：示範怎麼讓 LLM 變可靠。
- **過程**：AI 第一次亂講話會被「重抽（Retry）」，第二次講錯語法會被「修復（Repair）」，最後還會被「快取（Memoize）」起來。證明了我們可以透過組合多層保護，讓笨模型也能產出高品質結果。

### `demos/call_chain_trace.py` — 追蹤調用鏈
- **看點**：示範 `trace.py` 怎麼運作。
- **過程**：它會跑一個複雜的流程，然後印出一棵漂亮的樹，告訴你誰叫了誰、花了多少時間。

### `demos/resumable_batch.py` — 續傳批次處理
- **看點**：示範「中斷恢復」功能。
- **過程**：它故意跑到一半崩潰，重跑時會自動從斷掉的地方接下去，不會浪費時間重做已經寫好的東西。

---

## 2. 煙霧測試（161 項全綠）

我們不用 `pytest`，只用純 Python 寫了兩套超嚴格的煙霧測試（Smoke Test）。只要改壞一個地方，這裡就會噴紅字。

- **`smoke_test.py` (83 項)**：測試所有 **工具**（Indexer, Router, SFC, Hub 等）。包含動態增減函式、超時處理、限流守門等。
- **`lib_smoke_test.py` (78 項)**：測試所有 **底層庫**（StateDirs, Recovery, Compose 等）。包含模擬 HTTP 請求、複雜的投票（Vote）與辯論（Debate）邏輯。

**怎麼跑？**（進入 `try_implement/` 資料夾後）：
```bash
python smoke_test.py
python lib_smoke_test.py
```

---

## 3. 給你玩的範例

### `funcs/` — 簡單的小程式
- `upper.py`: 把字變大寫。
- `reverse.py`: 把字反過來。
- `c_linter.sh`: 一個假的 C 語言檢查器（Shell 腳本範例）。

### `store/` — 內建的小函式庫
SFC 裡面已經先塞了兩個好用的東西：
- `shout`: 把輸入變大寫加驚嘆號（Python 版）。
- `wc_words`: 幫你數有幾個字（Shell 版）。

可以用這個指令試試看：
```bash
echo "hello world" | sfc shout
```
如果你看到 `HELLO WORLD!`，代表一切正常！
