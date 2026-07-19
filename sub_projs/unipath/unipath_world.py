#!/usr/bin/env python3
"""unipath step 6 — 進階世界示範：NPC＝可定址腳本 ＋ 巢狀 tick ＋ 影響走路徑樹。

三件事一次演：
1. 規則住在路徑樹裡（每元素一段 script，可 echo 改）→ NPC 行為＝可定址的資料。
2. 巢狀 tick：town 含子世界＋rate，父一拍 → 子跑 rate 拍。
3. 使用者影響＝直接寫路徑樹（Env.write，就是外部 echo 走的同一介面），無旁路佇列。
轉移引擎見 up_tick_script.py；環境見 up_env.py。
"""
from up_env import Env
from up_tick_script import tick


def build_world():
    return [
        {"name": "guard", "hp": 100, "regen": 5,
         "script": "hp = min(100, hp + regen) if hp > 0 else 0"},   # 回血封頂
        {"name": "echo", "value": 0,
         "script": "value = peer[2]['world'][0]['n']"},             # 抄 town 巢狀時鐘
        {"name": "town", "rate": 3,                                 # 父一拍 → 子三拍
         "world": [{"name": "clock", "n": 0, "script": "n = n + 1"}]},
    ]


def _show(env, label):
    w = env.world
    print(f"{label} | guard.hp={w[0]['hp']:>4} | echo={w[1]['value']:>3} | "
          f"town.clock={w[2]['world'][0]['n']:>3}")


def _round(env, n=1):
    with env.lock:
        for _ in range(n):
            tick(env.world)


def _demo():
    env = Env(world=build_world(), tick=False)
    print("=== unipath step 6 — 腳本化 NPC ＋ 巢狀 tick ＋ 影響走路徑樹 ===")
    print("guard(script 回血) / echo(抄 town 巢狀時鐘) / town(rate=3 子世界)")
    _show(env, "初始    ")
    print("\n--- 自然演化 2 回合（town 內時鐘每父回合 +3）---")
    for _ in range(2):
        _round(env); _show(env, "tick    ")
    print("\n--- 影響走路徑樹：env.write('/0/hp/data', b'40')（＝外部 echo 40 > /0/hp/data）---")
    env.write("/0/hp/data", b"40")
    _round(env); _show(env, "傷害後  ")            # 40 →(script 回血)→ 45
    print("\n--- 改 NPC 行為＝改樹裡的腳本：把回血改成每回合流血 20 ---")
    env.write("/0/script/data", b"hp = hp - 20")
    print("  現在 cat /0/script/data =", env.read("/0/script/data").decode().strip())
    _round(env); _show(env, "改腳本後")            # 45 → 25
    _round(env); _show(env, "再一回合")            # 25 → 5
    print("\n觀察：規則(腳本)住路徑樹、可 cat/echo 改；影響也是寫樹；巢狀 town 每父回合跑 3 拍。")


if __name__ == "__main__":
    _demo()
