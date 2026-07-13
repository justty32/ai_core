---
description: WF5 頭腦風暴・擴展 — 用 ai_core LLM 工具打 API 把點子變大、變多
argument-hint: "[要擴展的主題 slug 或檔名，可留空=最新 notes]"
---
進入「擴展發想模式」（WF5），對 `workflows/ideas/` 軌的點子做發散延伸。發想的實際加工**不由你（agent）自己做，改由 ai_core 的 LLM 工具 `idea expand` 打 API 完成**（dogfood：`bind` → LLM Entry Manager → 真 backend）。全程繁體中文。

## 對象
$ARGUMENTS

留空＝取 `workflows/notes/` 最新一份。先決定要擴展哪一份 `workflows/notes/<slug 檔>`。

## 前置：設定要打哪個 LLM
`idea` 工具依環境變數挑 backend，未設定則回 EchoBackend（只回顯，測試用）。要真發想先設定：
```bash
export AI_CORE_LLM_PROVIDER=openai   # 或 anthropic
export AI_CORE_LLM_BASE_URL=...
export AI_CORE_LLM_MODEL=...
```
（選用）要讓 consume rate 跨呼叫累計：先起 `llm_entry_manager.py --socket /tmp/ai_core_llm.sock &`，再 `export AI_CORE_LLM_SOCKET=/tmp/ai_core_llm.sock`，`idea` 會自動連上同一個 daemon。

## 怎麼做（shell out 給 idea 工具）
先 `date +%Y%m%d-%H%M` 取時間戳，把選定的 notes 檔 pipe 進 `idea expand`，輸出落到 `workflows/ideas/brainstorm/`：

```bash
python3 sub_projs/ver_1/try_implement/tools/idea.py expand < workflows/notes/<slug 檔> \
  > workflows/ideas/brainstorm/<時間戳>-expand-<slug>.md
```

`idea expand` 已內建發想的 system prompt（相鄰應用場景／可結合的技術／「如果再進一步…」的變體／可借鑑的其他領域做法；AI 新加靈感標 `💡`，每條附「為什麼值得試」）。

## 規矩
- **不改原文／notes**；產物是 `workflows/ideas/brainstorm/` 下的獨立檔。
- 跑完後在產出檔頂端補一行 `> 來源：workflows/notes/<slug 檔>`，並把重點摘要回報給使用者。
- 寧可多給選項讓使用者挑。
