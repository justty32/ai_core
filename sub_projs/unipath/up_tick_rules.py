"""tick 的世界建構、每種 kind 的轉移規則、以及使用者影響的套用。

規則簽名 rule(elem, snapshot)：就地改 elem（下一態），只從 snapshot（本回合開頭全樹快照）讀值
→ 全樹同時更新（cellular-automaton 式）。之後要換成真 NPC 腳本再說。
"""


def build_world():
    return [
        {"name": "counter", "kind": "counter", "value": 0},              # /0 純累加
        {"name": "guard",   "kind": "npc",      "hp": 100, "regen": 5},  # /1 NPC 回血封頂
        {"name": "well",    "kind": "resource",  "amount": 10},          # /2 資源自然消耗
        {"name": "echo",    "kind": "mirror",    "value": 0, "src": 0},  # /3 抄鄰居 /0
    ]


def _rule_counter(elem, snapshot):
    elem["value"] += 1


def _rule_npc(elem, snapshot):
    hp = elem["hp"]
    elem["hp"] = min(100, hp + elem["regen"]) if hp > 0 else 0


def _rule_resource(elem, snapshot):
    elem["amount"] = max(0, elem["amount"] - 1)


def _rule_mirror(elem, snapshot):
    src = elem.get("src", 0)
    if 0 <= src < len(snapshot) and isinstance(snapshot[src], dict):
        elem["value"] = snapshot[src].get("value", elem["value"])


RULES = {"counter": _rule_counter, "npc": _rule_npc,
         "resource": _rule_resource, "mirror": _rule_mirror}


def apply_influence(world, inf):
    """套一條使用者影響：{"target":idx, "field":鍵, "op":"add"|"set", "value":值}。"""
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
    else:
        elem[field] = inf.get("value")
