"""step 6（Janet 版）— 規則語言換成嵌入式 Lisp：NPC 的 script 用 Janet 寫、交 janet 直譯。

比 Python `exec` 更貼「規範層語言中立、Lisp 為理想載體」，且天然沙箱（獨立 janet 行程）。
每個 scripted 元素的純量欄位綁成 Janet `var`（跑完收回）；`peer`＝全樹快照(JSON)供讀鄰居。
拿現成 janet CLI 用 subprocess 包裝（JSON 交換；暫不考慮效能）。
"""
import copy
import json
import subprocess

_SCALAR = (int, float, str)


def _coerce(v):
    return int(v) if isinstance(v, float) and v.is_integer() else v


def _run_janet(elem, snap):
    fields = {k: v for k, v in elem.items()
              if isinstance(v, _SCALAR) and k != "script"}
    binds = "\n".join(f'(var {k} (get self "{k}"))' for k in fields)
    backs = "\n".join(f'(put self "{k}" {k})' for k in fields)
    prog = ('(import spork/json)'
            '(def data (json/decode (file/read stdin :all)))'
            '(def self (get data "self"))(def peer (get data "peer"))'
            f'{binds}\n{elem["script"]}\n{backs}'
            '(print (json/encode self))')
    payload = json.dumps({"self": fields, "peer": snap}).encode()
    try:
        out = subprocess.run(["janet", "-e", prog], input=payload,
                             capture_output=True, timeout=5).stdout
        new = json.loads(out)
    except Exception:
        return
    for k in fields:
        if k in new:
            elem[k] = _coerce(new[k])


def tick(world):
    """一回合：先遞迴巢狀 world(rate 拍)，再對有 script 的元素同步跑 Janet 腳本。"""
    for elem in world:
        if isinstance(elem, dict) and isinstance(elem.get("world"), list):
            for _ in range(elem.get("rate", 1)):
                tick(elem["world"])
    snap = copy.deepcopy(world)
    for elem in world:
        if isinstance(elem, dict) and "script" in elem:
            _run_janet(elem, snap)
    return world
