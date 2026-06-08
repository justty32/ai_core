---
description: WF4 頭腦風暴・找漏洞 — 用 ai_core LLM 工具打 API 審查點子（指出哪裏有錯／考慮不周）
argument-hint: "[要檢視的主題 slug 或檔名，可留空=最新 notes]"
---
進入「找漏洞模式」（WF4），對 `ideas/` 軌的點子做嚴格但善意的審查。審查的實際加工**不由你（agent）自己做，改由 ai_core 的 LLM 工具 `idea critique` 打 API 完成**（dogfood：`bind` → LLM Entry Manager → 真 backend）。全程繁體中文。

## 對象
$ARGUMENTS

留空＝取 `ideas/notes/` 最新一份。先決定要審哪一份 `ideas/notes/<slug 檔>`（必要時回溯 `ideas/cleaned/`）。

## 前置：設定要打哪個 LLM
`idea` 工具依環境變數挑 backend，未設定則回 EchoBackend（只回顯，測試用）。要真審查先設定：
```bash
export AI_CORE_LLM_PROVIDER=openai   # 或 anthropic
export AI_CORE_LLM_BASE_URL=...
export AI_CORE_LLM_MODEL=...
```
（選用）要讓 consume rate 跨呼叫累計：先起 `llm_entry_manager.py --socket /tmp/ai_core_llm.sock &`，再 `export AI_CORE_LLM_SOCKET=/tmp/ai_core_llm.sock`，`idea` 會自動連上同一個 daemon。

## 怎麼做（shell out 給 idea 工具）
先 `date +%Y%m%d-%H%M` 取時間戳，把選定的 notes 檔 pipe 進 `idea critique`，輸出落到 `ideas/brainstorm/`：

```bash
python3 try_implement/tools/idea.py critique < ideas/notes/<slug 檔> \
  > ideas/brainstorm/<時間戳>-critique-<slug>.md
```

`idea critique` 已內建審查的 system prompt（找邏輯漏洞／自相矛盾／沒考慮到的前提與邊界／過度樂觀的假設／與常識衝突，並標嚴重度）。

## 規矩
- **不改原文、不改 notes**；產物是 `ideas/brainstorm/` 下的獨立檔。
- 跑完後在產出檔頂端補一行 `> 來源：ideas/notes/<slug 檔>`，並把重點摘要回報給使用者。
- 對事不對人，給的是「值得再想」的點，不是否定。
