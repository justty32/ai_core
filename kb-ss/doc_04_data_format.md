# doc_04 — 資料格式與 JSON 規範

> **一句話講完**：在我們的世界裡，JSON 就是通用語言。不管是設定檔、規格說明還是工具之間的對話，通通都用 JSON。

---

## 1. 為什麼是 JSON？

JSON 到處都通，而且 LLM 寫起來特別順手。最重要的是，Python 內建就有超強的支援，不用另外裝套件。

我們所有的東西都用 JSON：
- **結構化 I/O**：如果你要傳複雜的資料給下一個工具，就噴 JSON。
- **Metadata**：那最重要的「九軸」規格就是 JSON。
- **設定檔**：通通都是 `.json`。

---

## 2. 把指令參數變 JSON (不造輪子)

當你想把命令列打的 `--name Alice --age 30` 變成 `{"name": "Alice", "age": 30}` 時，不要自己寫解析器！

**我們的首選方案**：
用 Python 內建的 `argparse` 配合 `vars()`：
```python
import argparse, json

parser = argparse.ArgumentParser()
parser.add_argument("--name")
parser.add_argument("--age", type=int)

args = parser.parse_args()
json_output = json.dumps(vars(args))  # 一秒變 JSON！
```

這招最乾淨、零依賴，完全符合我們「輕量化」的硬原則。如果你在寫子指令 (Subcommand)，`argparse` 的 `add_subparsers` 也能幫你處理得好好的。
