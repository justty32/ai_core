#!/usr/bin/env python3
"""unipath step 5 — tick（世界狀態轉移函數）原型。

把 step 2~4 的「背景 thread 每秒 world[0]+=1」雛形，升級成**真正的回合制轉移**。
語意照設計筆記定案（20260719-1301 / 20260719-1150）：

    tick(當前狀態, 使用者影響) → 下個時間狀態

- 一個 tick ＝**掃過路徑樹上的元素、依規則算出下一個狀態**（不是內部模擬時鐘）。
- **Δt ＝回合數**：跑 N 個 tick 就是快轉 N 回合（見 run(n)）。
- **使用者影響 ＝執行期持續介入**（非啟動前快照參數）：用一條 pending influence 佇列體現，
  玩家在任何時刻 push 進去，於**下一個 tick 開頭被吸收**（＝「執行期持續借樹」的最小落地）。

本檔**不改動 unipath_env.py 的既有行為**：只 import 其純邏輯 Env，並以 `tick=False`
建構（關掉它自帶的背景 +1 thread），改由本檔的回合制 tick 驅動同一棵活物件圖。
故 step 3/4 的發布端若改掛這棵 world，外部 walk/cat 讀到的就是回合制轉移後的狀態。

試驗田心態：最小、能跑、可推倒。規則刻意簡單，之後要換成真 NPC 腳本再說。
"""
import copy

from unipath_env import Env


# ---- 世界：幾個元素當 NPC / 計數器 / 資源 ----
# 每個元素是一個 dict，帶 "kind" 決定它每回合怎麼演化。
# 對映路徑樹：world 是 list → 數字子路徑 /0../3；每元素是 dict → 字串鍵子路徑（/1/hp…）。
def build_world():
    """建一個示範小世界（回傳純資料，好讓 tick 對它做狀態轉移）。"""
    return [
        {"name": "counter", "kind": "counter", "value": 0},              # /0 純累加計數器
        {"name": "guard",   "kind": "npc",      "hp": 100, "regen": 5},  # /1 NPC：每回合回血、封頂 100
        {"name": "well",    "kind": "resource",  "amount": 10},          # /2 資源：每回合自然消耗
        {"name": "echo",    "kind": "mirror",    "value": 0, "src": 0},  # /3 鏡子：抄鄰居（/0）的值
    ]


# ---- 規則：每種 kind 一條「讀快照 → 寫下一態」的轉移 ----
# 統一簽名 rule(elem, snapshot) -> None：就地改 elem（下一態），只從 snapshot（本回合開頭的
# 全樹快照）讀值。用快照讀值 → 全樹**同時更新**（cellular-automaton 式），避免掃描順序造成偏差。
def _rule_counter(elem, snapshot):
    elem["value"] += 1


def _rule_npc(elem, snapshot):
    # 每回合回血，封頂 100；hp 掉到 0 以下就維持 0（死了不再回）。
    hp = elem["hp"]
    if hp > 0:
        elem["hp"] = min(100, hp + elem["regen"])
    else:
        elem["hp"] = 0


def _rule_resource(elem, snapshot):
    # 資源每回合自然消耗 1，不低於 0。
    elem["amount"] = max(0, elem["amount"] - 1)


def _rule_mirror(elem, snapshot):
    # 依鄰居值更新：抄 snapshot 中 src 指到的那個元素的 value（讀本回合開頭值 → 同步更新語意）。
    src = elem.get("src", 0)
    if 0 <= src < len(snapshot) and isinstance(snapshot[src], dict):
        elem["value"] = snapshot[src].get("value", elem["value"])


RULES = {
    "counter": _rule_counter,
    "npc": _rule_npc,
    "resource": _rule_resource,
    "mirror": _rule_mirror,
}


# ---- 使用者影響：執行期持續介入的最小通道 ----
# 一條 influence 就是一個 dict：{"target": <索引>, "field": <鍵>, "op": "add"|"set", "value": <值>}
# 例：玩家砍 guard 30 血 → {"target": 1, "field": "hp", "op": "add", "value": -30}
def _apply_influence(world, inf):
    i = inf.get("target")
    if not (isinstance(i, int) and 0 <= i < len(world)):
        return
    elem = world[i]
    if not isinstance(elem, dict):
        return
    field, op = inf.get("field"), inf.get("op", "set")
    if field not in elem:
        return
    if op == "add":
        elem[field] = elem[field] + inf.get("value", 0)
    else:  # set
        elem[field] = inf.get("value")


# ---- tick：世界狀態轉移函數 ----
def tick(world, influences=None):
    """對 world 跑一回合的狀態轉移（就地）。

    tick(當前狀態, 使用者影響) → 下個時間狀態。次序：
      1. **吸收使用者影響**（本回合開頭把佇列裡的介入全部作用上去）。
      2. 對「吸收影響後」的世界拍**快照**。
      3. **掃過每個元素**、依其 kind 的規則從快照讀值、寫下一態（全樹同時更新）。

    回傳 world 本身（就地轉移；同時回傳方便串接）。
    """
    # 1. 吸收使用者影響
    for inf in (influences or []):
        _apply_influence(world, inf)

    # 2. 拍快照（規則一律讀快照 → 同步更新）
    snapshot = copy.deepcopy(world)

    # 3. 掃樹套規則
    for elem in world:
        if isinstance(elem, dict):
            rule = RULES.get(elem.get("kind"))
            if rule:
                rule(elem, snapshot)
    return world


# ---- 驅動器：把 tick 掛到 unipath 的 Env 上，並管回合數與影響佇列 ----
class TickWorld:
    """回合制世界驅動器：持有一棵活 Env（關掉背景 +1）＋ pending 影響佇列 ＋ 回合計數。

    - push_influence(inf)：執行期任意時刻注入使用者影響，排進佇列。
    - step()：跑一個 tick（吸收並清空當前佇列）。
    - run(n)：跑 N 個 tick，Δt＝回合數（見筆記 §2.3）。
    - env：底層 unipath Env，可被 step 3/4 發布端拿去暴露成路徑樹。
    """

    def __init__(self, world=None):
        self.env = Env(world=build_world() if world is None else world, tick=False)
        self.pending = []
        self.round = 0

    @property
    def world(self):
        return self.env.world

    def push_influence(self, inf):
        """執行期持續介入：把一條影響排進佇列，下個 tick 開頭被吸收。"""
        self.pending.append(inf)

    def step(self):
        """跑一個 tick：吸收當前佇列的影響 → 全樹轉移 → 佇列清空、回合 +1。"""
        with self.env.lock:                 # 與 Env 對外讀寫共用同一把鎖，跨 process 讀不到半更新態
            influences = self.pending
            self.pending = []
            tick(self.env.world, influences)
        self.round += 1
        return self.round

    def run(self, n):
        """Δt ＝回合數：連跑 n 個 tick。"""
        for _ in range(n):
            self.step()
        return self.round


# ---- 自測：印出連續幾回合，證明確實在轉移、且使用者影響有被吸收 ----
def _snapshot_line(tw):
    w = tw.world
    return (f"回合 {tw.round:>2} | "
            f"counter={w[0]['value']:>3} | "
            f"guard.hp={w[1]['hp']:>4} | "
            f"well.amount={w[2]['amount']:>3} | "
            f"echo(抄counter)={w[3]['value']:>3} | "
            f"pending={len(tw.pending)}")


def _demo():
    tw = TickWorld()
    print("=== unipath step 5 — tick 回合制轉移示範 ===")
    print("元素：/0 counter(累加) /1 guard(回血封頂100) /2 well(每回合-1) /3 echo(抄/0)")
    print(_snapshot_line(tw), "  ← 初始（尚未 tick）")

    print("\n--- 先自然演化 3 回合（無影響）---")
    for _ in range(3):
        tw.step()
        print(_snapshot_line(tw))

    print("\n--- 執行期注入使用者影響：對 guard 造成 -60 傷害、給 well +5 ---")
    tw.push_influence({"target": 1, "field": "hp", "op": "add", "value": -60})
    tw.push_influence({"target": 2, "field": "amount", "op": "add", "value": 5})
    print(f"（已 push {len(tw.pending)} 條影響進佇列，尚未 tick）")
    tw.step()  # 下個 tick 吸收：先 -60 傷害/+5 資源，再套規則（guard 回 5、well 消 1）
    print(_snapshot_line(tw), "  ← 影響被吸收後 + 規則轉移")

    print("\n--- 再自然演化 3 回合，看 guard 逐回合回血、echo 續抄 counter ---")
    for _ in range(3):
        tw.step()
        print(_snapshot_line(tw))

    print("\n--- 一次 push 多條、再用 run(Δt=2) 快轉 2 回合 ---")
    tw.push_influence({"target": 1, "field": "hp", "op": "set", "value": 0})   # 直接把 guard 打成 0（死）
    tw.run(2)
    print(_snapshot_line(tw), "  ← guard 被 set 0 後不再回血（驗死亡規則）")

    print("\n觀察：counter 每回合 +1；echo 每回合等於『上一回合開頭的 counter』（依鄰居值更新）；")
    print("      well 每回合 -1 觸底 0；guard 傷害/回血/死亡三態皆依規則轉移；使用者影響確被吸收。")


if __name__ == "__main__":
    _demo()
