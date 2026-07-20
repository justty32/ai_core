# examples · 可跑的範例

每支都獨立、都實測過。有依賴的（ffi/subcommands）開頭自己載 Quicklisp，所以 `--script` 直接能跑。

| 檔 | 主題 | 跑法 |
|----|------|------|
| `ffi-demo.lisp` | C FFI（cffi，不編譯直呼共享庫） | `sbcl --script examples/ffi-demo.lisp` |
| `subcommands.lisp` | git 風格子命令（clingon） | `sbcl --script examples/subcommands.lisp add https://x.git -n libs` |
| `conditions.lisp` | 條件 / restart / unwind-protect | `sbcl --script examples/conditions.lisp` |
| `macros.lisp` | `` ` `` `,` `,@`、gensym、macroexpand | `sbcl --script examples/macros.lisp` |

對應教學：
- FFI → [docs/09-c-ffi.md](../docs/09-c-ffi.md)
- 子命令 → [docs/04-cli-clingon.md](../docs/04-cli-clingon.md)
- 條件系統 → [docs/08-conditions.md](../docs/08-conditions.md)
- macro → [docs/07-macro.md](../docs/07-macro.md)

> 提醒：`sbcl --script` **不讀 `~/.sbclrc`**，所以需要 Quicklisp 的範例開頭都有一行
> `(load .../quicklisp/setup.lisp)`。互動開發時走 REPL（`scripts/run.sh repl`）則不需要。
