"""FunctionRegistry — stores and indexes all registered functions.

Responsibilities:
    - register / unregister functions
    - lookup by ID, type, or tag
    - track reference relationships (composite -> components)
    - prevent removing a function while it's referenced

The registry is owned by `CoreService`. Composite functions hold a back-ref
to their owning registry so they can resolve chained function IDs at call time.
"""

from ai_core.functions.base import BaseFunction


class FunctionRegistry:
    """In-memory function store with type/tag indexes and reference tracking."""

    def __init__(self) -> None:
        """Initialize empty stores and indexes."""
        # Primary store: id -> function instance
        self._functions: dict[str, BaseFunction] = {}

        # Reverse references: func_id -> {ids of composites that reference it}
        self._references: dict[str, set[str]] = {}

        # Indexes
        self._by_type: dict[str, list[str]] = {}
        self._by_tag: dict[str, list[str]] = {}

    # ---------- Mutation ----------

    def register(self, func: BaseFunction) -> None:
        """Register a function and update all indexes.

        Steps:
            1. If func.id already in _functions: raise FunctionAlreadyExistsError.
            2. Store: _functions[func.id] = func.
            3. Update _by_type using func.metadata["type"].
            4. Update _by_tag using each entry in func.metadata["tags"].
            5. If func is a CompositeFunction (check by metadata["type"] == "composite"):
                 for each id in func.closure["func_chain"]:
                     _references[id] = _references.get(id, set()) | {func.id}
        """
        raise NotImplementedError

    def unregister(self, func_id: str) -> None:
        """Remove a function.

        Steps:
            1. If func_id not in _functions: raise FunctionNotFoundError.
            2. If _references.get(func_id) is non-empty: raise FunctionInUseError
               with the referrer set.
            3. Remove from _functions, _by_type, _by_tag.
            4. If the removed function was a composite, also remove its entries
               from _references for each id in its func_chain.
        """
        raise NotImplementedError

    # ---------- Lookup ----------

    def get(self, func_id: str) -> BaseFunction:
        """Get a function by ID. Raises FunctionNotFoundError if missing."""
        raise NotImplementedError

    def has(self, func_id: str) -> bool:
        """Return True if `func_id` is registered."""
        raise NotImplementedError

    def list_all(self) -> list[str]:
        """Return all registered function IDs (sorted, for stable output)."""
        raise NotImplementedError

    def find_by_type(self, func_type: str) -> list[BaseFunction]:
        """Return all functions whose metadata['type'] equals `func_type`."""
        raise NotImplementedError

    def find_by_tag(self, tag: str) -> list[BaseFunction]:
        """Return all functions whose metadata['tags'] contains `tag`."""
        raise NotImplementedError

    # ---------- References ----------

    def get_references(self, func_id: str) -> set[str]:
        """Return the IDs of functions that reference `func_id`.

        For a non-referenced or non-existent ID, returns an empty set.
        Does NOT raise if `func_id` is unregistered.
        """
        raise NotImplementedError
