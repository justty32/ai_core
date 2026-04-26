import json
from pathlib import Path


class Session:
    """
    Turn-based session state: persist a dict across process invocations.
    Each set() immediately writes to disk (atomic rename).
    """

    def __init__(self, path="session.json"):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.data = json.loads(self.path.read_text()) if self.path.exists() else {}

    def get(self, key, default=None):
        return self.data.get(key, default)

    def set(self, key, value):
        self.data[key] = value
        self._save()

    def delete(self, key):
        self.data.pop(key, None)
        self._save()

    def clear(self):
        self.data = {}
        self._save()

    def all(self):
        return dict(self.data)

    def _save(self):
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(self.data, ensure_ascii=False, indent=2))
        tmp.rename(self.path)
