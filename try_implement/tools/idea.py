#!/usr/bin/env python3
"""idea — 點子捕捉／頭腦風暴軌的 LLM 工具（把 .claude/commands 的「派 agent」換成「打 API」）。

== 由來 ==
`/intake`、`/critique`、`/expand` 三個 slash command 原本靠 Claude Code **agent**（甚至派背景
subagent）去做清稿／彙整／找漏洞／擴展。本工具把那層 agent 換成 ai_core 自己的 **LLM 呼叫
基礎設施**——這正是 roadmap「廉價小模型消費者」的第一個真實 dogfood，也把三元件串成完整一條：

    bind() 打包（元件 2，lib/llm_call）
      → LLM Entry Manager 單例守門 + consume rate（元件 1，tools/llm_entry_manager）
        → 真 backend（OpenAIBackend / AnthropicBackend，lib/llm_call）
          → 真 API

== 子命令（git-style；text 進 text 出，shell 為一等公民）==
    idea clean      WF2 初步整理：去語音錯字/贅字/缺字，最大保留原意（純 filter）
    idea notes      WF3 匯總筆記：把整理後內容彙整成結構化筆記（純 filter）
    idea critique   WF4 找漏洞：嚴格但善意地指出漏洞/風險/矛盾（純 filter）
    idea expand     WF5 擴展：發散延伸、給靈感（純 filter）
    idea ingest     口述一條龍：stdin 逐字 append 到 raw/（不過 LLM），再刷新 cleaned/、notes/

純 filter 子命令都 stdin→stdout，可被 pipe 串接。`ingest` 是給 /intake 即時回應路徑用的
orchestrator：它自己寫檔（守鐵則 1：raw 一字不改、不經 LLM），讓主 agent 一個背景呼叫就搞定。

== backend 路由 ==
預設**經由 LLM Entry Manager**（單例 + consume rate；尊重「LLM 是 singleton 資源」立場）。
`--direct` 則跳過 entry manager 直接呼叫 backend（測試/不需 rate 管理時）。
打哪個真 LLM 由環境變數決定（見 lib/llm_call.backend_from_env）；未設定則 EchoBackend，
離線可跑、煙霧測試友善。

== --metadata（跨元件契約）==
每個 LLM 子命令都宣告第九軸 `nondeterministic: true`（未認證隨機）——這正是 LLM 工具
在治理原則下的預設身分：尚未領證書前，誠實標記自己會吐隨機。
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import socket
import sys
import time
from pathlib import Path

from _common import ensure_ai_core_importable, ensure_lib_importable, repo_root

ensure_ai_core_importable()
ensure_lib_importable()

import ai_core  # noqa: E402
from lib import call, llm_call  # noqa: E402

PY = sys.executable
EM_PATH = str(Path(__file__).resolve().parent / "llm_entry_manager.py")


# ---------------------------------------------------------------------------
# 各階段的 system prompt（繁體中文；把 slash command 的鐵則內化給小模型）
# ---------------------------------------------------------------------------

PROMPTS = {
    "clean": (
        "你是文字初步整理員。輸入是使用者『語音輸入』的口述原文，會有錯字、贅字、缺字。"
        "你的任務：只清除這些語音造成的雜訊，**最大程度保留原意**。"
        "嚴禁潤飾、擴寫、重組論點或加入新內容。只輸出整理後的文字本體，不要任何前後說明。"
    ),
    "notes": (
        "你是筆記彙整員。把輸入的口述內容彙整成結構化的繁體中文筆記（用標題與條列）。"
        "忠實反映原意，不臆測、不擴寫。若內容自相矛盾或與常識衝突，把兩種說法都保留並在該處標上"
        "『⚠️衝突』，不要替使用者下定論。只輸出筆記本體。"
    ),
    "critique": (
        "你是嚴格但善意的審查者。針對輸入的點子，找出：邏輯漏洞、自相矛盾、沒考慮到的前提與"
        "邊界情況、過度樂觀的假設、與現實或常識衝突之處。每條都要：指出具體位置（引用原句）＋"
        "為什麼是問題＋嚴重度（高/中/低）。對事不對人，給的是『值得再想』的點。用繁體中文條列輸出。"
    ),
    "expand": (
        "你是發想夥伴。針對輸入的點子做發散性延伸，幫使用者把點子變大、變多：相鄰應用場景、"
        "可結合的技術/資源、『如果再進一步…』的變體、可類比借鑑的其他領域做法。"
        "AI 新加的靈感用『💡』標記，與使用者原意的延伸區分開。每條附一句『為什麼值得試』。"
        "用繁體中文條列輸出。"
    ),
}

# 第九軸：LLM 工具未領證書前，誠實宣告 nondeterministic=true（未認證隨機）。
_LLM_SUBMETA = {"lifecycle": "one_shot", "state": "stateless", "nondeterministic": True}


# ---------------------------------------------------------------------------
# backend 路由：經由 LLM Entry Manager（單例 + consume rate）
# ---------------------------------------------------------------------------

class EntryManagerBackend(llm_call.Backend):
    """把 completion 經由 LLM Entry Manager（元件 1）打出去，而非直接呼叫 backend。

    entry manager 自己會做 consume-rate 守門、再轉給真 backend——這就是「LLM 是 singleton
    資源、集中經由統一入口」立場的落地。兩種連線模式：

    - **socket 模式**（給 socket_path）：連上一個**長駐** entry manager（`--socket` 啟動的
      Unix socket server）。多個 one-shot `idea` 連同一個 server → **consume rate 跨呼叫
      累計**。這是 README「Gap G」的解法。
    - **subprocess 模式**（不給 socket_path）：每次 completion 新開一個 entry manager
      process 餵一行 NDJSON。零設定即可跑，但 consume rate 是逐次呼叫、不累計（fallback）。
    """

    def __init__(self, socket_path: str | None = None, limit_token: float | None = None):
        self.socket_path = socket_path
        self.limit_token = limit_token

    def complete(self, prompt: str, **opts: object) -> str:
        line = json.dumps({"cmd": "complete", "prompt": prompt, "opts": opts},
                          ensure_ascii=False)
        if self.socket_path:
            return self._via_socket(line)
        return self._via_subprocess(line)

    def _via_socket(self, line: str) -> str:
        last_err: Exception | None = None
        for _ in range(20):  # daemon 可能剛啟動，短重試
            c = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                c.connect(self.socket_path)
            except OSError as exc:
                c.close()
                last_err = exc
                time.sleep(0.05)
                continue
            try:
                c.sendall((line + "\n").encode("utf-8"))
                resp_line = c.makefile("r", encoding="utf-8").readline()
            finally:
                c.close()
            return self._parse(resp_line)
        raise RuntimeError(f"連不上 entry manager socket {self.socket_path!r}：{last_err}")

    def _via_subprocess(self, line: str) -> str:
        cmd = [PY, EM_PATH]
        if self.limit_token is not None:
            cmd += ["--limit-token", str(self.limit_token)]
        out = call.Subprocess(cmd).call(line + "\n")
        for raw in out.splitlines():
            if raw.strip():
                return self._parse(raw)
        raise RuntimeError(f"entry manager 無有效回應：{out!r}")

    @staticmethod
    def _parse(raw: str) -> str:
        resp = json.loads(raw)
        if resp.get("ok") is False:
            raise RuntimeError(f"entry manager 拒絕：{resp.get('error')}")
        if "text" in resp:
            return resp["text"]
        raise RuntimeError(f"entry manager 回應無 text：{raw!r}")


def _stage_fn(stage: str, *, direct: bool, limit_token: float | None,
              socket_path: str | None):
    """產出某階段的 ``f(str)->str``：bind 疊 system prompt + 選擇 backend 路由。"""
    if direct:
        backend: llm_call.Backend = llm_call.backend_from_env()
    else:
        backend = EntryManagerBackend(socket_path=socket_path, limit_token=limit_token)
    return llm_call.bind(system=PROMPTS[stage], backend=backend)


# ---------------------------------------------------------------------------
# ingest：口述一條龍（raw 逐字 → cleaned → notes），寫檔
# ---------------------------------------------------------------------------

def _ideas_dir(override: str | None) -> Path:
    return Path(override) if override else (repo_root() / "ideas")


def _slugify(text: str) -> str:
    s = re.sub(r"[^\w一-鿿]+", "-", text.strip().lower()).strip("-")
    return (s[:40] or "idea")


def cmd_ingest(args: argparse.Namespace) -> int:
    """stdin 逐字 append 到 raw/（不經 LLM），再整份刷新 cleaned/ 與 notes/。"""
    text = sys.stdin.read()
    ts = datetime.datetime.now().strftime("%Y%m%d-%H%M")
    slug = args.slug or _slugify(text)

    base = _ideas_dir(args.ideas_dir)
    # 同一主題（slug）跨多則口述沿用同一份檔，讓 raw 累積、cleaned/notes 整份刷新；
    # 找不到既有檔才用當下時間戳開新檔。**嚴格比對整個檔名「<時間戳>-<slug>.md」**——
    # 不能用 glob(f"*-{slug}.md")，因為 * 會跨連字號，導致某 slug 撞進「以它為連字號後綴」
    # 的另一主題檔（例：slug=idea 會吃到 ...-my-idea.md），違反 /intake 鐵則 1 的主題隔離。
    raw_dir = base / "raw"
    name_re = re.compile(r"\d{8}-\d{4}-" + re.escape(slug) + r"\.md")
    existing = (sorted(p for p in raw_dir.glob("*.md") if name_re.fullmatch(p.name))
                if raw_dir.is_dir() else [])
    fname = existing[0].name if existing else f"{ts}-{slug}.md"
    raw_p = base / "raw" / fname
    cleaned_p = base / "cleaned" / fname
    notes_p = base / "notes" / fname
    for p in (raw_p, cleaned_p, notes_p):
        p.parent.mkdir(parents=True, exist_ok=True)

    # WF1：逐字 append，一字不改（鐵則 1）。不經 LLM。
    if not raw_p.exists():
        raw_p.write_text(f"> 原文・{ts}\n\n", encoding="utf-8")
    with raw_p.open("a", encoding="utf-8") as f:
        f.write(text.rstrip("\n") + "\n\n")

    full_raw = raw_p.read_text(encoding="utf-8")

    # WF2/WF3：整份刷新（過 LLM）
    clean = _stage_fn("clean", direct=args.direct, limit_token=args.limit_token,
                      socket_path=args.socket)
    notes = _stage_fn("notes", direct=args.direct, limit_token=args.limit_token,
                      socket_path=args.socket)
    cleaned_text = clean(full_raw)
    cleaned_p.write_text(f"> 初步整理自 ideas/raw/{fname}\n\n{cleaned_text}\n", encoding="utf-8")
    notes_text = notes(cleaned_text)
    notes_p.write_text(f"> 匯總筆記自 ideas/cleaned/{fname}\n\n{notes_text}\n", encoding="utf-8")

    def _rel(p: Path) -> str:
        try:
            return str(p.relative_to(repo_root()))
        except ValueError:
            return str(p)

    print(json.dumps({
        "ok": True,
        "raw": _rel(raw_p),
        "cleaned": _rel(cleaned_p),
        "notes": _rel(notes_p),
    }, ensure_ascii=False))
    return 0


def cmd_filter(stage: str, args: argparse.Namespace) -> int:
    """純 filter 子命令：stdin → LLM(stage) → stdout。"""
    fn = _stage_fn(stage, direct=args.direct, limit_token=args.limit_token,
                   socket_path=args.socket)
    sys.stdout.write(fn(sys.stdin.read()))
    return 0


# ---------------------------------------------------------------------------
# metadata 宣告 + CLI
# ---------------------------------------------------------------------------

def _setup_metadata() -> None:
    ai_core.register(lifecycle="one_shot", state="stateless")
    for s in PROMPTS:  # clean/notes/critique/expand：one_shot、stateless、nondeterministic
        ai_core.register_subcommand(s, **_LLM_SUBMETA)
    # ingest 多寫三個 ideas/ 目錄 → stateful_external；同樣 nondeterministic（內含 LLM）
    ai_core.register_subcommand("ingest", lifecycle="one_shot",
                                state="stateful_external", nondeterministic=True)


def _add_common(sp: argparse.ArgumentParser) -> None:
    sp.add_argument("--direct", action="store_true",
                    help="跳過 LLM Entry Manager，直接呼叫 backend（測試/不需 rate 管理時）")
    sp.add_argument("--limit-token", type=float, default=None,
                    help="subprocess 模式下的逐次 token 上限（socket 模式由 daemon 自己設）")
    sp.add_argument("--socket", default=os.environ.get("AI_CORE_LLM_SOCKET"),
                    help="連上長駐 entry manager 的 Unix socket 路徑（讓 consume rate 跨呼叫"
                         "累計）；預設取環境變數 AI_CORE_LLM_SOCKET")


def main(argv: list[str] | None = None) -> int:
    _setup_metadata()
    ai_core.intercept(argv)

    p = argparse.ArgumentParser(prog="idea", description="點子捕捉軌的 LLM 工具")
    sub = p.add_subparsers(dest="cmd", required=True)
    for s in PROMPTS:
        _add_common(sub.add_parser(s, help=f"{s} 階段（stdin→stdout）"))
    ing = sub.add_parser("ingest", help="口述一條龍：raw 逐字 → cleaned → notes")
    ing.add_argument("--slug", default=None, help="檔名 slug（省略則依內容自動取）")
    ing.add_argument("--ideas-dir", default=None, help="ideas 根目錄（預設 repo 根的 ideas/）")
    _add_common(ing)

    args = p.parse_args(argv)
    if args.cmd == "ingest":
        return cmd_ingest(args)
    return cmd_filter(args.cmd, args)


if __name__ == "__main__":
    sys.exit(main())
