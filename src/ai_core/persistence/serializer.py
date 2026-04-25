"""JSON serialization for a CoreService.

File format (version 0.1.0):

    {
        "version": "0.1.0",
        "functions": [
            {
                "type": "llm",
                "id": "...",
                "closure": {...},
                "metadata": {...}
            },
            {
                "type": "shell",
                ...
            },
            {
                "type": "composite",
                "id": "...",
                "closure": {"func_chain": [...]},
                "metadata": {...}
            }
        ]
    }

Load order:
    Composite functions reference other functions, so they MUST be loaded
    last. The loader does a two-pass approach:
      1. Load all non-composite functions first.
      2. Then load composites, binding the registry into each.
"""

import json
import logging
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ai_core.service.core import CoreService

# Bump when changing the on-disk format incompatibly.
SAVE_FORMAT_VERSION = "0.1.0"

logger = logging.getLogger(__name__)


def save_core(core: "CoreService", path: str) -> None:
    """Serialize a CoreService to a JSON file."""
    from ai_core.functions.calculate import CalculateFunction

    functions_data = []
    for func_id in core.list_functions():
        func = core.registry.get(func_id)
        if isinstance(func, CalculateFunction):
            logger.warning("Skipping calculate function %s during save", func_id)
            continue
        functions_data.append(func.to_dict())

    payload = {"version": SAVE_FORMAT_VERSION, "functions": functions_data}
    with open(path, "w") as f:
        json.dump(payload, f, indent=2)


def load_core(path: str) -> "CoreService":
    """Deserialize a CoreService from a JSON file."""
    from ai_core.functions.composite import CompositeFunction
    from ai_core.functions.llm import LLMFunction
    from ai_core.functions.shell import ShellFunction
    from ai_core.service.core import CoreService

    with open(path) as f:
        data = json.load(f)

    if data.get("version") != SAVE_FORMAT_VERSION:
        raise ValueError(
            f"Unsupported save format version: {data.get('version')}; "
            f"expected {SAVE_FORMAT_VERSION}"
        )

    core = CoreService()

    # First pass: non-composites
    type_dispatch = {
        "llm": LLMFunction.from_dict,
        "shell": ShellFunction.from_dict,
    }
    composite_entries = []

    for entry in data["functions"]:
        t = entry["type"]
        if t == "composite":
            composite_entries.append(entry)
        elif t == "calculate":
            logger.warning("Cannot restore calculate function %s", entry.get("id"))
        elif t in type_dispatch:
            core.register(type_dispatch[t](entry))
        else:
            raise ValueError(f"Unknown function type: {t}")

    # Second pass: composites (need registry)
    for entry in composite_entries:
        core.register(CompositeFunction.from_dict(entry, registry=core.registry))

    return core
