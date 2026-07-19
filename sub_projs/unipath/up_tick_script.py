"""step 6 進階 tick：NPC＝可定址腳本、巢狀 tick、影響走路徑樹（無旁路佇列）。

- 規則不再硬編：每個元素帶一段 `script`（純量欄位當變數 exec），tick 執行它算下一態。
  script 就住在路徑樹裡（`/idx/script/data`），可 `cat`/`echo` 讀改 → NPC 行為＝可定址的資料。
- 巢狀 tick：元素含 `world`(子元素 list)＋`rate` → 父跑一拍，子世界跑 rate 拍。
- 影響走路徑樹：外部先用 Env.write 改樹（`echo > …/data`），tick 只做轉移、無佇列（借樹＝影響）。
"""
import copy

SAFE = {"min": min, "max": max, "abs": abs, "int": int, "len": len}


def _run_script(elem, snap):
    """把 elem 的純量欄位當變數 exec 它的 script，再寫回；`peer`＝全樹快照（讀鄰居用）。"""
    ns = {k: v for k, v in elem.items() if isinstance(v, (int, float, str))}
    ns["peer"] = snap
    try:
        exec(elem["script"], {"__builtins__": {}, **SAFE}, ns)
    except Exception:
        pass
    for k in list(elem):
        if k != "peer" and k in ns and isinstance(elem[k], (int, float, str)):
            elem[k] = ns[k]


def tick(world):
    """對 world 跑一回合：先遞迴巢狀子世界（rate 拍），再對有 script 的元素同步套腳本。"""
    for elem in world:
        if isinstance(elem, dict) and isinstance(elem.get("world"), list):
            for _ in range(elem.get("rate", 1)):
                tick(elem["world"])
    snap = copy.deepcopy(world)      # 同步更新：script 讀 peer 都是本回合開頭值
    for elem in world:
        if isinstance(elem, dict) and "script" in elem:
            _run_script(elem, snap)
    return world
