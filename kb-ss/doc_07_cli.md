# CLI 介面標準 (cli_spec.md)

這份文件定義了我們系統中 CLI 工具的「長相」。我們對 CLI 有一個核心哲學：**CLI 呼叫其實就是函式呼叫**。

---

## 核心概念：CLI 就是 Lisp

如果你學過 Lisp，你會發現 CLI 呼叫跟它一模一樣。

```bash
# 這是 CLI
my_tool input.txt --output result.json --verbose

# 這是它對應的函式呼叫 (Lisp 風格)
(my_tool "input.txt" :output "result.json" :verbose true)
```

當你寫一個工具時，請記住這個對應關係：
*   **Positional Args (位置參數)**：就像函式的一般參數，順序很重要。
*   **Flags (--key value)**：就像函式的 Keyword Arguments，用來傳遞具名的參數。
*   **`--` 前綴**：只是為了標記「這是一個 Key，不是 Value」。

在實作時，我們會把 `argv` 拆成兩部分：
1.  **Array**：存放所有的位置參數。
2.  **Dict**：存放所有的 Flag 鍵值對。

---

## 實作規範 (Python 版)

1.  **解析工具**：請唯一指名使用標準庫的 `argparse`。不要用其他第三方的解析庫，保持零相依。
2.  **資料型別**：
    *   布林值：用 `action='store_true'` (例如 `--verbose`)。
    *   字串：預設型別。
    *   列表：用 `nargs='+'` (例如 `--files a.py b.py`)。
3.  **解析結果**：使用 `vars(parser.parse_args())` 拿到的 dict，應該要可以直接用 `json.dumps()` 轉成 JSON。

---

## 必備功能：`--metadata`

這是我們系統中**每個工具都必須實作**的旗標。

當使用者（或系統）執行 `your_tool --metadata` 時，工具必須印出一段 JSON，描述它自己的脾氣：
*   它吃多少記憶體？
*   它需不需要 GPU？
*   它是 One-shot 還是 Persistent？
*   （詳細格式請看 `doc_06_lib_contract.md`）

我們就是靠這個旗標來讓機器自動理解與串接所有的工具。

---

## 我們不用的東西 (為了簡單)

*   **不合併短旗標**：不要搞 `-abc` 這種把 `-a -b -c` 合在一起寫的東西，`argparse` 預設不支援，我們也不需要。
*   **不強求等號**：`--flag=value` 隨便你用不用，但我們標準建議是用空白格開：`--flag value`。
