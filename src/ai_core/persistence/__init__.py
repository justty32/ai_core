"""Core persistence — save/load a CoreService to/from JSON.

Limitations:
    - CalculateFunction is NOT serialized (Python callables can't safely
      round-trip through JSON). Save logs a warning and skips them.
"""

from ai_core.persistence.serializer import load_core, save_core

__all__ = ["save_core", "load_core"]
