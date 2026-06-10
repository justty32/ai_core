# ext_06 — 開發者上路指南 (Developer Onboarding)

> **關聯檔**：`doc_01` (本質), `doc_06` (Library 契約)

## 1. 你的第一個符合規範的函式
如何快速建立一個能被 `hub` 識別並被 `ai_core` 治理的函式？

### 步驟 A：編寫核心代碼
```python
def my_func(input_str):
    # 做點什麼
    return f"Processed: {input_str}"
```

### 步驟 B：註冊與攔截
```python
import sys
from ai_core import register, intercept

def main():
    register(
        name="my-tool",
        lifecycle="one_shot",
        guarantee="idempotent",
        # ... 其他軸
        nondeterministic=False # 如果是純程式碼
    )
    
    # 攔截 --metadata 等旗標，或是處理正常輸入
    res = intercept(sys.argv)
    if res is not None:
        # intercept 會處理 --metadata 並直接 exit
        # 若傳回非 None，表示這是正常執行的 input
        print(my_func(res["input"]))

if __name__ == "__main__":
    main()
```

## 2. 測試規範 (Testing Checklist)
在將函式放入 `store/` 之前，請確保：
1.  `python my_tool.py --metadata` 回傳正確的 JSON。
2.  多次執行（相同輸入）的輸出符合 `guarantee` 宣告。
3.  若有 `state_dirs` 需求，確認 `.config` 等目錄會自動建立。

## 3. 加入生態系
1.  將腳本放入系統 PATH 或 `store/` 目錄。
2.  執行 `hub scan`。
3.  現在你可以在 `chain` 或 `idea` 中使用這個新工具了。

## 4. 進階：從 LLM 函式轉為證書函式
1.  起初：`nondeterministic=True`。
2.  運行一段時間後，收集 log。
3.  執行 `certify my-tool --model llama3 --stability 0.98`（假設有此工具）。
4.  更新代碼中的 `register` 資訊。
