#!/usr/bin/env python3
"""unipath step 5 — tick 回合制轉移示範入口。

tick / TickWorld 見 up_tick.py；規則與世界見 up_tick_rules.py。
跑：./unipath_tick.py（印出連續幾回合，證明狀態轉移＋使用者影響被吸收）。
"""
from up_tick import TickWorld


def _line(tw):
    w = tw.world
    return (f"回合 {tw.round:>2} | counter={w[0]['value']:>3} | guard.hp={w[1]['hp']:>4} | "
            f"well.amount={w[2]['amount']:>3} | echo(抄counter)={w[3]['value']:>3} | "
            f"pending={len(tw.pending)}")


def _demo():
    tw = TickWorld()
    print("=== unipath step 5 — tick 回合制轉移示範 ===")
    print("元素：/0 counter(累加) /1 guard(回血封頂100) /2 well(每回合-1) /3 echo(抄/0)")
    print(_line(tw), "  ← 初始（尚未 tick）")
    print("\n--- 先自然演化 3 回合（無影響）---")
    for _ in range(3):
        tw.step(); print(_line(tw))
    print("\n--- 執行期注入：對 guard -60 傷害、well +5 ---")
    tw.push_influence({"target": 1, "field": "hp", "op": "add", "value": -60})
    tw.push_influence({"target": 2, "field": "amount", "op": "add", "value": 5})
    print(f"（已 push {len(tw.pending)} 條影響，尚未 tick）")
    tw.step(); print(_line(tw), "  ← 影響被吸收後 + 規則轉移")
    print("\n--- 再演化 3 回合 ---")
    for _ in range(3):
        tw.step(); print(_line(tw))
    print("\n--- push set hp 0、run(Δt=2) 快轉 ---")
    tw.push_influence({"target": 1, "field": "hp", "op": "set", "value": 0})
    tw.run(2)
    print(_line(tw), "  ← guard 被 set 0 後不再回血（驗死亡規則）")
    print("\n觀察：counter+1/回合；echo 抄上一回合開頭的 counter；well 觸底 0；guard 傷害/回血/死亡依規則。")


if __name__ == "__main__":
    _demo()
