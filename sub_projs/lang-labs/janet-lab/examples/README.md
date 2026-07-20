# examples · 可跑的範例

每支都獨立、都實測過。

| 檔 | 主題 | 跑法 |
|----|------|------|
| `ffi-demo.janet` | C FFI（不編譯，直呼共享庫） | `janet examples/ffi-demo.janet` |
| `subcommands.janet` | git 風格子命令 dispatcher | `janet examples/subcommands.janet add https://x.git -n libs` |
| `fibers.janet` | fiber / generator / 錯誤處理 / ev | `janet examples/fibers.janet` |
| `pipeline.janet` | 子行程 / 管線 / 信號 | `janet examples/pipeline.janet` |
| `native-module/` | 用 C 寫 Janet 原生模組 | `cd examples/native-module && jpm build`（見下） |
| `embed/` | 把 Janet 嵌進 C 程式 | `cd examples/embed`（見下） |

## native-module

```sh
cd examples/native-module
jpm build                       # 編出 build/greet.so
JANET_PATH=$PWD/build janet -e '(import greet) (print (greet/add 3 4) (greet/hello "Janet"))'
```

## embed

```sh
cd examples/embed
cc embed.c -I$HOME/.local/include/janet $HOME/.local/lib/libjanet.a \
   -lm -ldl -lpthread -lrt -rdynamic -o embed
./embed
```

對應教學：FFI / native / embed 都在 [docs/10-c-互通.md](../docs/10-c-互通.md)，
fiber 在 [docs/09-fiber.md](../docs/09-fiber.md)，子命令在 [docs/04-cli-argparse.md](../docs/04-cli-argparse.md)。
