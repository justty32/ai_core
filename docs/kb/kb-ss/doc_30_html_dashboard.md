# doc_30 — The Dark Mode Dashboard (Vernacular Version)

- **Source**: Various HTML files (overview.html, index.html, etc.)
- **Status**: Visual Snapshot (As of May 25, 2026)
- **TL;DR**: We built a fancy, dark-themed web dashboard to visualize the project state. It’s basically a UI for our markdown docs.

---

## 1. The "Big Picture" in a Browser
We took the roadmap, the architecture docs, and the prototype stats and turned them into a website. It’s great for getting a "vibe check" on the project status without digging through 50 markdown files.

### The Motto:
- **"Let the LLM handle the 'translation of intent' and the 'fuzzy stuff'; the system handles the hard-coded logic."**
- **Design Principles**: KISS, Lightweight, Don't reinvent the wheel, Minimal dependencies.

---

## 2. Dashboard Highlights

### The HL (Heuristic Learning) Map
The dashboard has a unique table showing how our components map to the "Heuristic Learning" philosophy:
- **Author**: Generates the "brain" (functions).
- **Hub**: The library of what the system can do.
- **Dry-run**: The feedback loop that makes sure the "brain" actually works.
- **Metadata**: The only way the AI understands what tools it has.

### Real-time Stats (Snapshot)
At the time of the snapshot:
- **140 Assertions**: All tests in the prototype were green.
- **3 Demos**: Full end-to-end examples were running.
- **15 Decisions**: Open questions we still needed to answer.

---

## 3. Important Reality Check
The dashboard is a "snapshot in time." Some things have changed since it was built:
- **Axes**: The dashboard says we have 8 axes. **Truth**: We now have **9 axes** (we added `nondeterministic`).
- **Tests**: We now have **161 assertions** in the prototype (up from 140) and **84 passed tests** in the core.
- **Milestones**: The dashboard shows 0% completion on M0–M6, but we've already made significant progress on the protocol and core library.

---

## 4. Where to find stuff:
- **Rules**: `core_nature/`
- **Prototypes**: `try_implement/`
- **Blueprints**: `docs/architectures/`
- **Goal**: `roadmap.md`
