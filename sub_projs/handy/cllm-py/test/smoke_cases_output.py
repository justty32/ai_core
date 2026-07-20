"""smoke_cases_output.py — 「輸出／body 正確」那半的 case：happy path、system role、
tools/modalities、media 直通三分流。斷言與計數走 smoke_harness（跨檔共享計數）。
"""
import os

from smoke_harness import (HERE, TEMP_FILES, _ng, _ok, expect_body, expect_exit,
                           expect_no_body, expect_out, fx)


def run():
    # ── (1) happy path：輸出正確 ──
    print("-- 輸出正確 --")
    expect_out("才不是為你回答的", "位置參數當 prompt（非串流）",
               ["你好", "--endpoint", fx("fake")])
    expect_out("才不是為你回答的", "--stream 串流逐段",
               ["數到五", "--stream", "--endpoint", fx("fake_stream")])
    expect_out("才不是為你回答的", "沒位置參數 → 讀 stdin",
               ["--endpoint", fx("fake")], stdin="從管線\n")
    expect_out("才不是為你回答的", "prompt＋stdin 合體（指令在前、資料在後）",
               ["總結這份", "--endpoint", fx("fake")], stdin="附加資料\n")
    expect_out("才不是為你回答的", "「-」＝stdin 插入點",
               ["把", "-", "翻成英文", "--endpoint", fx("fake")], stdin="內容\n")
    expect_out("才不是為你回答的", "旗標在前 + prompt（--stream 你好）",
               ["--stream", "--endpoint", fx("fake_stream"), "你好"])
    expect_out("才不是為你回答的", "-- 分隔符（旗標在前 -- prompt）",
               ["--stream", "--endpoint", fx("fake_stream"), "--", "你好"])

    # --schema 收「字面 JSON 文字」（⚠ 與 C++ llm 分歧：不再讀檔；要吃檔案用 $(cat s.json)）
    expect_out('"name":"星野"', "--schema 收字面 JSON → 結構化輸出",
               ["給我角色", "--schema", '{"type":"object"}', "--endpoint", fx("fake_json")])

    # ── system role ──
    print("-- system role --")
    expect_body('"role":"system","content":"你是傲嬌貓"', "--system → body 首則為 system 訊息",
                ["--system", "你是傲嬌貓", "你好", "--endpoint", fx("fake")])
    expect_body('"role":"user"', "--system 後仍保留 user 訊息",
                ["--system", "你是傲嬌貓", "你好", "--endpoint", fx("fake")])
    expect_no_body('"role":"system"', "未給 --system → body 無 system 訊息",
                   ["你好", "--endpoint", fx("fake")])

    # ── tools / modalities（皆收字面 JSON）──
    print("-- tools / modalities --")
    tool_lit = ('{"name":"get_weather","description":"查天氣",'
                '"parameters":{"type":"object","properties":{"city":{"type":"string"}}}}')
    expect_out('"tool":"get_weather"', "--tool 收字面 JSON → tool_calls 一行 JSON 吐 stdout",
               ["東京天氣", "--tool", tool_lit, "--endpoint", fx("fake_tool")])
    expect_out('"city":"東京"', "--tool → arguments 原樣內嵌",
               ["東京天氣", "--tool", tool_lit, "--endpoint", fx("fake_tool")])

    outdir = HERE
    media_file = os.path.join(outdir, "llm-media-1.wav")
    TEMP_FILES.append(media_file)
    if os.path.exists(media_file):
        os.remove(media_file)
    expect_out("llm-media-1.wav", "--modality 收字面 JSON＋--media-out → 媒體落檔、路徑吐 stdout",
               ["說你好", "--modality", 'audio={"voice":"alloy"}', "--media-out", outdir,
                "--endpoint", fx("fake_media")])
    try:
        with open(media_file, "rb") as f:
            content = f.read()
        if content == b"hello-wav-bytes":
            _ok("落檔內容正確（base64 已解碼）")
        else:
            _ng("落檔內容不對：<%r>" % content)
    except OSError:
        _ng("落檔不存在：%s" % media_file)
    expect_exit(0, "媒體但沒給 --media-out → 明說丟棄、不炸",
                ["說你好", "--endpoint", fx("fake_media")])

    # ── media 直通三分流（--image/--media）──
    print("-- media 直通三分流 --")
    # 2) data:/http(s):// URL → 直接當 url 送、不編碼
    expect_body('"url":"data:image/png;base64,BBBB"', "--media data: URL 直通（不編碼）",
                ["描述這張圖", "--media", "data:image/png;base64,BBBB", "--endpoint", fx("fake")])
    # 3a) .json 描述子 {"url": …} → 直通 url
    murl = os.path.join(HERE, "_smoke_media_url.json")
    TEMP_FILES.append(murl)
    with open(murl, "w", encoding="utf-8") as f:
        f.write('{"url":"data:image/png;base64,AAAA"}')
    expect_body('"url":"data:image/png;base64,AAAA"', "--media .json 描述子 {url} → 直通",
                ["描述這張圖", "--media", murl, "--endpoint", fx("fake")])
    # 3b) .json 描述子 {"mime","data"} → decode→bytes→內核重組 data URI（不重讀原圖）
    mbin = os.path.join(HERE, "_smoke_media_bin.json")
    TEMP_FILES.append(mbin)
    with open(mbin, "w", encoding="utf-8") as f:
        f.write('{"mime":"image/png","data":"aGVsbG8="}')  # base64("hello") 可 round-trip
    expect_body('data:image/png;base64,aGVsbG8=', "--media .json 描述子 {mime,data} → 直通",
                ["描述這張圖", "--media", mbin, "--endpoint", fx("fake")])
    # 別名 --image 走同一分流
    expect_body('"url":"data:image/png;base64,CCCC"', "--image 別名亦支援 URL 直通",
                ["描述這張圖", "--image", "data:image/png;base64,CCCC", "--endpoint", fx("fake")])
    # 形狀不符的 .json 描述子 → 退出碼 1
    mbad = os.path.join(HERE, "_smoke_media_bad.json")
    TEMP_FILES.append(mbad)
    with open(mbad, "w", encoding="utf-8") as f:
        f.write('{"foo":1}')
    expect_exit(1, "--media .json 描述子形狀不符 → 退出碼 1", ["你好", "--media", mbad])
