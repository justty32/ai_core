"""smoke_cases_exit.py — 「設定來源＋退出碼分流」那半的 case：config 檔／env 指路／旗標覆寫，
以及各種用法錯（退 1）、請求失敗（退 2）。斷言與計數走 smoke_harness（跨檔共享計數）。
"""
import os

from smoke_harness import HERE, TEMP_FILES, expect_exit, expect_out, fx


def run():
    # ── 設定來源：config 檔 + env 指路 + 旗標覆寫 ──
    print("-- 設定來源 --")
    conf = os.path.join(HERE, "_smoke_conf.json")
    TEMP_FILES.append(conf)
    with open(conf, "w", encoding="utf-8") as f:
        f.write('{"endpoint":"%s","temperature":0.7}' % fx("fake"))
    expect_out("才不是為你回答的", "--config 提供 endpoint（無 --endpoint 旗標）",
               ["你好", "--config", conf])
    expect_out("才不是為你回答的", "env LLM_CLI_CONFIG 指路",
               ["你好"], extra_env={"LLM_CLI_CONFIG": conf})
    expect_out('"name":"星野"', "命令列旗標覆寫 config 的 endpoint",
               ["你好", "--config", conf, "--endpoint", fx("fake_json")])

    # ── 退出碼分流 ──
    print("-- 退出碼分流 --")
    expect_exit(0, "成功", ["你好", "--endpoint", fx("fake")])
    expect_exit(0, "--help", ["--help"])
    expect_exit(1, "未知旗標", ["你好", "--bogus"])
    expect_exit(1, "旗標缺值（--image）", ["你好", "--image"])
    expect_exit(1, "缺 prompt（stdin 空）", ["--endpoint", fx("fake")], stdin="")
    expect_exit(1, "「-」但 stdin 空（拼完仍空）", ["-", "--endpoint", fx("fake")], stdin="")
    expect_exit(1, "--config 讀不到", ["你好", "--config", "/no/such.json"])
    # 字面優先後：給路徑會被當字面文字、解 JSON 失敗 → 退出碼 1（刻意與 C++ llm 分歧）
    expect_exit(1, "--schema 給路徑（當字面、非法 JSON）", ["你好", "--schema", "/no/such.json"])
    expect_exit(1, "--schema 字面非法 JSON", ["你好", "--schema", "{bad json"])
    expect_exit(1, "--system 缺值", ["你好", "--system"])
    bad = os.path.join(HERE, "_smoke_bad.json")
    TEMP_FILES.append(bad)
    with open(bad, "w", encoding="utf-8") as f:
        f.write("{bad json")
    expect_exit(1, "config JSON 壞", ["你好", "--config", bad])
    expect_exit(1, "--tool 給路徑（當字面、非法 JSON）", ["你好", "--tool", "/no/such.json"])
    expect_exit(1, "--tool 字面缺 name/parameters", ["你好", "--tool", '{"description":"x"}'])
    expect_exit(1, "--modality config 字面非法 JSON", ["你好", "--modality", "audio=/no/such.json"])
    expect_exit(1, "--media .json 描述子讀不到", ["你好", "--media", "/no/such.json"])
    expect_exit(1, "--media-out 不是目錄", ["你好", "--media-out", "/no/such/dir"])
    expect_exit(1, "數值旗標型別錯（--temperature abc）",
                ["你好", "--temperature", "abc", "--endpoint", fx("fake")])
    expect_exit(2, "請求失敗（連不上真 endpoint）",
                ["你好", "--endpoint", "http://127.0.0.1:59999/v1/chat/completions",
                 "--timeout-ms", "800"])
