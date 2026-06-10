# ext_12 — LLM 輸出的決定性測試模式 (Deterministic Testing)

> **關聯檔**：`code_01` (核心 library), `tests/test_core.py`

## 1. 核心動機
測試 LLM 函式是出了名的難。我們需要一套標準的模式，在不依賴第三方測試框架的前提下，確保 `ai_core` 內部邏輯的正確性。

## 2. 結構化斷言 (Structural Assertion)
不要直接比對字串，而是將輸出轉化為結構化數據後再驗證：
- **JSON Schema Validation**：使用內建 `json` 模組讀取，並手動遍歷 key/type。
- **AST Comparison**：對於生成的代碼，將其解析為 AST 樹，比對樹的節點屬性而非字串。

## 3. Mocking 策略 (Mocking LLM)
在測試環境中，透過攔截 `lib/llm_call.py` 的呼叫，回傳預定義的「確定性回應」：

```python
# 測試範例
def test_intent_parsing():
    mock_response = '{"action": "delete", "line": 3}'
    with mock_llm_response(mock_response):
        result = parse_user_intent("刪除第三行")
        assert result["action"] == "delete"
```

## 4. 煙霧測試與穩定度回歸
- **Seed-based Generation**：如果模型支援，固定 `seed` 參數。
- **Multiple Run Consistency**：連續跑 5 次，確保輸出內容在語義上是一致的（或是在 `nondeterministic: true` 的容忍範圍內）。
