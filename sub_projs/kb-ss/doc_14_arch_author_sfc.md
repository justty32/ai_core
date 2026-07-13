# doc_14_arch_author_sfc (Vernacular Version)

- **Source**: `docs/architectures/05_author_sfc.md`
- **Status**: Legacy Arch Doc (Partially superseded by the new roadmap)
- **TL;DR**: This is about `ai-core-author` (a one-stop shop for generating, dry-running, and registering functions) and `Small Function Center (SFC)` (a way to group tiny functions so your hub doesn't get cluttered).

---

## 8. ai-core-author: Letting AI (or you) write more functions

The idea is simple: You tell the tool "I want a function that does X", and it handles the boilerplate, metadata, and validation until it's ready to use in `funcs/`.

### How it works:
1. **The Ask**: You provide a name, description, and some input/output examples.
2. **The Build**: It generates the script (bash/python) and the metadata JSON.
3. **The Dry-Run (Mandatory)**: It actually runs the code against your examples. If it fails, it asks the LLM to fix it (up to N times).
4. **Registration**: Once it passes, it saves it to `funcs/` and verifies the metadata protocol.

### Why force a dry-run?
Because AI hallucinations are real. We don't want broken scripts in the registry. If it hasn't passed its own examples, it doesn't get in.

### CLI Usage:
- `--spec spec.json`: The main way to define what you want.
- `--language python`: If you want something other than bash.
- `--target sfc`: Instead of a standalone file, shove it into an SFC.

---

## 9. Small Function Center (SFC)

### The Problem:
If you have 50 tiny one-liner functions (like `uppercase`, `reverse`, `is-valid-email`), your main hub list becomes a nightmare to navigate.

### The Solution:
Group them into an **SFC**. It's just a CLI tool that acts as a dispatcher. Instead of 50 files, you have one tool that takes a subcommand or a flag to run the sub-function.

### The Minimum "Contract":
To be a good SFC, you just need two things:
1. `my-sfc --call <sub-func> --input <data>`: A way to run the internal functions.
2. `my-sfc --metadata`: A way to describe the SFC itself.

### Pro-tips for SFCs:
- **Implement `--list`**: So the hub scanner can find all your sub-functions.
- **Support sub-function metadata**: If someone runs `my-sfc --call foo --metadata`, they should get the JSON for *that specific* sub-function. 
- **Don't hardcode**: Try to make it so adding a new sub-function just requires dropping a file in a folder, not editing the dispatcher's core logic.

### Hub Integration:
If an SFC is "smart" (supports `--list`), the hub will list each sub-function as an independent tool. If it's "dumb", the hub just lists the SFC itself as one big tool.
