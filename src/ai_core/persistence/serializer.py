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

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ai_core.service.core import CoreService

# Bump when changing the on-disk format incompatibly.
SAVE_FORMAT_VERSION = "0.1.0"


def save_core(core: "CoreService", path: str) -> None:
    """Serialize a CoreService to a JSON file.

    Steps:
        1. Iterate over all functions in core's registry.
        2. For each:
             - skip CalculateFunction with a warning (logging.warning).
             - call func.to_dict() to get the per-function payload.
        3. Wrap into the top-level envelope:
             {"version": SAVE_FORMAT_VERSION, "functions": [...]}.
        4. Write to `path` as JSON (indent=2 for readability).
    """
    raise NotImplementedError


def load_core(path: str) -> "CoreService":
    """Deserialize a CoreService from a JSON file.

    Steps:
        1. Read the JSON file.
        2. Validate top-level "version" matches SAVE_FORMAT_VERSION.
        3. Build a fresh CoreService.
        4. Two-pass load:
             a. First pass: instantiate all non-composite functions
                from their dicts (dispatch on entry["type"]) and register them.
             b. Second pass: instantiate composites, passing the now-built
                registry into CompositeFunction.from_dict(data, registry).
        5. Return the assembled CoreService.

    Type dispatch table (subclass selection by `type`):
        "llm"       -> LLMFunction.from_dict
        "shell"     -> ShellFunction.from_dict
        "composite" -> CompositeFunction.from_dict (needs registry)
        "calculate" -> log warning, skip (can't restore)
    """
    raise NotImplementedError
