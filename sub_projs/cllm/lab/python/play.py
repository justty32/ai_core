#!/usr/bin/env python3
"""play.py — cllm Python binding：基本 ask、串流、schema+JSON 解析、shell(CLI) 呼叫。
POSIX 跑：source ~/dev/cllm/env.sh 後  python3 play.py "$CLLM_FIXTURES"
Windows 跑：pwsh -File run.ps1（湊好 PYTHONPATH/LIBCLLM/PATH/fixtures URI）"""
import sys, json, subprocess
import llm
base = sys.argv[1] if len(sys.argv) > 1 else ""
ep = lambda n: (base + n) if base else None

print("[py] ask =>", llm.ask("你好", ep("fake/chat/completions")))
print("[py] 串流 => ", end="")
llm.ask("數到五", endpoint=ep("fake_stream/chat/completions"), stream=True,
        on_delta=lambda p: (print(f"[{p}]", end="", flush=True), False)[1]); print()

# schema → JSON → stdlib json 解析
obj = json.loads(llm.ask("給我角色", endpoint=ep("fake_json/chat/completions"), schema='{"type":"object"}'))
print(f"[py] json => name={obj['name']} affection={obj['affection']} lines={len(obj['lines'])}")

# tools＋on_tool：模型回 tool_call，on_tool 收 call={id,name,arguments} 並解析 arguments
def handle_tool(call):
    args = json.loads(call["arguments"])
    print(f"[py] tool => {call['name']}(city={args['city']}, unit={args['unit']})")

llm.ask("東京天氣如何？", endpoint=ep("fake_tool/chat/completions"),
        tools=[{"name": "get_weather", "description": "查某城市天氣",
                "parameters": '{"type":"object","properties":{"city":{"type":"string"},'
                              '"unit":{"type":"string"}}}'}],
        on_tool=handle_tool)

# media 輸出：模型回 audio，on_media 收 m={mime,bytes}
def handle_media(m):
    print(f"[py] media out => mime={m['mime']} bytes={len(m['bytes'])}")

llm.ask("說句話", endpoint=ep("fake_media/chat/completions"), on_media=handle_media)

# media 輸入＋modalities：掛上 Request 送出（fixture 不看 body，驗的是搬運不炸）
try:
    llm.ask("描述這張圖", endpoint=ep("fake/chat/completions"),
            media=[{"url": "data:image/png;base64,iVBORw0KGgo="}],
            modalities=[{"name": "audio", "config": '{"voice":"alloy"}'}])
    print("[py] media in+modality => ok")
except llm.LLMError as e:
    print("[py] media in+modality =>", e)

# shell 呼叫：從 Python 呼叫 llm CLI，捕捉答案
#   ⚠ Windows 兩個坑（兩者對 POSIX 也無害，故不分平台）：
#   ① stdin=DEVNULL：llm 是 unix filter，會讀 inherited stdin；不給 EOF 會卡住等輸入。
#   ② encoding="utf-8"：llm 輸出 UTF-8，但 text=True 在 Windows 用 locale 碼頁（繁中 cp950）
#      解碼會炸 UnicodeDecodeError，須顯式指定 utf-8。
out = subprocess.run(["llm", "你好", "--endpoint", ep("fake/chat/completions")],
                     capture_output=True, encoding="utf-8", stdin=subprocess.DEVNULL)
print("[py] shell(llm) =>", out.stdout.strip())
