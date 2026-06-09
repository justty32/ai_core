# Python Library 實作規範 (lib_spec.md)

這份文件是在講 `ai_core` 這個 Python library 到底要怎麼用。如果你正在寫一個要整合進系統的工具，這就是你的 API 合約。

---

## 核心機制：宣告 (Register) 與 攔截 (Intercept)

以前我們把這兩件事混在一起，現在為了讓工具更好用、更好測試，我們把它拆開了。

### 1. `register()`：告訴系統你是誰
這只是在程式內部做個登記，**完全沒有副作用**。你可以安全地在 `main()` 裡面呼叫它。
*   `register(**kwargs)`：設定頂層的 metadata。
*   `register_subcommand(name, **kwargs)`：如果你的工具有子命令（像 `git commit`），可以幫子命令各別設定。
*   **注意**：沒事不要在 module 的最頂層呼叫 `register()`，免得別人 `import` 你的時候，他的 metadata 被你覆寫掉。請把它放在 `if __name__ == "__main__":` 裡面。

### 2. `intercept()`：處理 `--metadata` 查詢
這是真正的「攔截器」。它會去讀 `sys.argv`：
*   如果有人下 `prog --metadata`，它就印出 JSON 然後直接 `sys.exit(0)`。
*   如果沒人查 metadata，它就什麼都不做，把控制權還給你的 `main()` 繼續跑。
*   **重要**：要在你的 ArgumentParser 解析之前呼叫它。

---

## 如何定義 I/O 出入口 (Entries)

你的工具怎麼接資料？我們用 `entries` 這個 dictionary 來描述。

每個 Entry 可以設定：
*   **方向**：`able_in` (能不能收資料), `able_out` (能不能噴資料)。
*   **模式 (Mode)**：
    *   `"batch"`：一次給整包。
    *   `"streaming"`：像水流一樣一直噴。
    *   `"interactive"`：會停下來問問題。
*   **格式 (Type)**：`"text"` 或 `"binary"`。
*   **CLI 綁定 (terminal_binding)**：這很實用，你可以指定這個 entry 對應到哪個 flag（例如 `--input`），以及沒給 flag 時預設是不是接 `stdin`。

---

## 9 軸 Metadata 的 JSON 格式

當你呼叫 `register()` 時，背後其實是在填這些 JSON 欄位：

### 1. 生命週期 (`lifecycle`)
*   值：`"one_shot"` (預設) 或 `"persistent"`。

### 2. 狀態 (`state`)
*   值：`"stateless"` (預設) 或 `"stateful_external"`。
*   如果你會改到外部檔案，請務必宣告為 `stateful_external`。

### 3. 可中斷性 (`interruptible`)
*   值：`"safe"`, `"unsafe"` (預設), `"resettable"`, `"resumable"`, `"graceful"`。
*   這決定了 Hub 能不能隨便把你 kill 掉。

### 4. 資源 (`resources`)
這是一個自由填寫的 dict，但我們建議用這些標準 key：
*   `"memory"`: `"4gb"` 或 `{"startup": "1gb", "peak": "4gb"}`。
*   `"gpu"`: `true` 或 `{"vram": "6gb"}`。
*   `"time"`: `"30s"`。

### 5. 執行保證 (`guarantee`)
*   值：`"none"` (預設), `"idempotent"` (冪等), `"transactional"` (交易性)。

### 6. 確定性 (`nondeterministic`)
*   `true`：代表這東西背後接了 LLM，結果會亂噴。
*   `{ "model": "...", "stability": "92%" }`：這就是「證書」，代表隨機性已經被馴化到一定水準了。

---

## 程式碼範例 (白話版)

### 簡單的單一工具
```python
import ai_core

def main():
    # 1. 先宣告
    ai_core.register(
        lifecycle="one_shot",
        entries={
            "stdin": {"able_in": True, "mode": "batch", "type": "text"}
        }
    )
    
    # 2. 攔截 --metadata。如果是查詢，這行就直接 exit 了。
    ai_core.intercept()
    
    # 3. 正常的邏輯
    print("Hello, context!")

if __name__ == "__main__":
    main()
```

### 帶子命令的工具 (像 git)
```python
def main():
    # 頂層預設
    ai_core.register(lifecycle="one_shot")
    
    # forge 子命令是一個常駐的 server
    ai_core.register_subcommand("forge", lifecycle="persistent")
    
    # 攔截：支援 `prog --metadata` 也支援 `prog forge --metadata`
    ai_core.intercept()
    
    # ... 剩下的 dispatch 邏輯
```

記住：**先 Register，再 Intercept，最後才跑你自己的 ArgumentParser**。這樣你的工具就能完美融入我們的系統了。
