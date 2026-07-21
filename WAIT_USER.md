# WAIT_USER — 等待使用者的事

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

需要**使用者親自做 / 驗證 / 追認**才能繼續的事。Claude 能做結構性驗證＋打包到極限；跨不過去的那一關記這裡等使用者。**只列還沒做的**——做完即移除（歷史看 git log）。

## 待使用者項

- **handy：本機 LM Studio 實測**（2026-07-21）。`llme.json` 的 `local`／`qwen` 兩個 endpoint 指向 `http://localhost:1234/v1`，**從未對真的 LM Studio 跑過**（使用者那邊多離線）。需要**你把 LM Studio 連模型一起跑起來**，然後：
  ```sh
  cd sub_projs/handy && ./.venv/Scripts/python.exe llme.py local 你好
  ```
  重點是驗「免認證端點」那條路——litellm 的 openai provider 一律要求 api_key，handy 補了佔位字串 `handy-no-auth` 繞過，這招只在假後端上驗過，真的 LM Studio 吃不吃還不知道。細節見 [util/llm/README.md](sub_projs/handy/util/llm/README.md) 的坑 2。
