"""tick：世界狀態轉移函數 + 回合制驅動器 TickWorld。

語意（筆記 20260719-1301/1150）：tick(當前狀態, 使用者影響) → 下個狀態；Δt＝回合數；
使用者影響＝執行期持續介入。TickWorld 建在 up_env.Env(tick=False) 上，可被 step 3/4 發布端暴露。
"""
import copy

from up_env import Env
from up_tick_rules import build_world, RULES, apply_influence


def tick(world, influences=None):
    """對 world 跑一回合的狀態轉移（就地）。
    次序：吸收影響 → 拍全樹快照 → 掃樹依 kind 規則從快照讀值寫下一態（同步更新）。"""
    for inf in (influences or []):
        apply_influence(world, inf)
    snapshot = copy.deepcopy(world)
    for elem in world:
        if isinstance(elem, dict):
            rule = RULES.get(elem.get("kind"))
            if rule:
                rule(elem, snapshot)
    return world


class TickWorld:
    """回合制世界驅動器：活 Env（關背景 +1）＋ pending 影響佇列 ＋ 回合計數。"""

    def __init__(self, world=None):
        self.env = Env(world=build_world() if world is None else world, tick=False)
        self.pending = []
        self.round = 0

    @property
    def world(self):
        return self.env.world

    def push_influence(self, inf):
        """執行期持續介入：排一條影響進佇列，下個 tick 開頭吸收。"""
        self.pending.append(inf)

    def step(self):
        with self.env.lock:                 # 與 Env 讀寫共用一把鎖，跨 process 讀不到半更新態
            influences, self.pending = self.pending, []
            tick(self.env.world, influences)
        self.round += 1
        return self.round

    def run(self, n):
        """Δt ＝回合數：連跑 n 個 tick。"""
        for _ in range(n):
            self.step()
        return self.round
