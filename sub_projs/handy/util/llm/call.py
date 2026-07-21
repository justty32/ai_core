"""call.py — 把 pllm 形狀的參數翻成 litellm.completion 的 kwargs。

這裡是「介面翻譯」的去程：組請求／傳輸的活全給 litellm，本檔只負責把舊 pllm 的
參數名與慣例對上 litellm 的。messages 那段在 msg.py，回程（回應→回呼）在 resp.py。

⚠ 三處刻意分歧（住戶看得到，見 handy/README.md）：
  · endpoint 仍收完整 URL（…/v1/chat/completions），這裡剝成 litellm 要的
    api_base（…/v1）。
  · model 自動加 openai/ 前綴，走 OpenAI-compatible 路徑；否則 litellm 會把
    google/gemma-… 的 google/ 誤判成 provider。設 LLM_RAW_MODEL=1 可關掉。
  · drop_params 開著：後端不吃的取樣參數自動丟棄而非炸掉。
"""
import json
import os

from .errors import LLMError
from .msg import build_messages

PLACEHOLDER_MODEL = "openai/gpt-3.5-turbo"   # 沒給 model 時的佔位

SAMPLING = ["temperature", "top_p", "presence_penalty",
            "frequency_penalty", "max_tokens", "seed"]


def load_litellm():
    """延遲載入 litellm：沒裝也不該讓 import util.llm 或 --help 爆掉。"""
    try:
        import litellm
    except ImportError as e:
        raise LLMError("litellm 未安裝（pip install litellm）") from e
    litellm.drop_params = True
    litellm.suppress_debug_info = True
    return litellm


def as_obj(x):
    """「JSON 物件」統一成 dict：字串→json.loads；已是 dict→原樣；空→{}。"""
    if x is None:
        return {}
    if isinstance(x, str):
        if not x.strip():
            return {}
        return json.loads(x)
    return x


def api_base(url):
    """完整 endpoint URL → litellm 的 api_base（剝掉尾巴 /chat/completions）。"""
    tail = "/chat/completions"
    if url.endswith(tail):
        return url[:-len(tail)]
    return url


def model_id(model):
    """model → litellm 模型名（預設加 openai/ 前綴，見模組 docstring）。"""
    if not model:
        return PLACEHOLDER_MODEL
    if os.environ.get("LLM_RAW_MODEL"):
        return model
    if model.startswith("openai/"):
        return model
    return "openai/" + model


def _add_sampling(kw, opts):
    """取樣參數：None 者不放＝不送、交後端默認（對齊 pllm）。"""
    for name in SAMPLING:
        if opts.get(name) is not None:
            kw[name] = opts[name]


def _add_tools(kw, tools):
    """--tool 收來的定義 → OpenAI 的 tools 陣列。"""
    defs = []
    for t in tools:
        defs.append({"type": "function", "function": {
            "name": t.get("name", ""),
            "description": t.get("description", "") or "",
            "parameters": as_obj(t.get("parameters"))}})
    kw["tools"] = defs


def _add_modalities(kw, modalities):
    """名字進 modalities 陣列；有 config 者另拼成頂層鍵（如 audio:{…}）。"""
    names = []
    for m in modalities:
        name = m.get("name")
        if not name:
            continue
        names.append(name)
        if m.get("config"):
            kw[name] = as_obj(m["config"])
    if names:
        kw["modalities"] = names


def _add_schema(kw, schema):
    """--schema → response_format.json_schema（結構化輸出）。"""
    kw["response_format"] = {"type": "json_schema", "json_schema": {
        "name": "response", "strict": True, "schema": as_obj(schema)}}


def build_kwargs(prompt, url, opts):
    """組 litellm.completion 的完整 kwargs。opts＝ask() 收到的具名參數 dict。"""
    messages = build_messages(prompt, opts.get("system"), opts.get("media"))
    kw = {"model": model_id(opts.get("model")),
          "messages": messages,
          "api_base": api_base(url),
          "stream": bool(opts.get("stream"))}

    if opts.get("api_key"):
        kw["api_key"] = opts["api_key"]
    if opts.get("timeout_ms") is not None:
        kw["timeout"] = float(opts["timeout_ms"]) / 1000.0   # 毫秒→秒
    _add_sampling(kw, opts)
    if opts.get("schema") is not None:
        _add_schema(kw, opts["schema"])
    if opts.get("tools"):
        _add_tools(kw, opts["tools"])
    if opts.get("modalities"):
        _add_modalities(kw, opts["modalities"])
    return kw
