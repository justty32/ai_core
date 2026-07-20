# cllm/ — cllm 傘（產物 ＋ 周邊）

← [sub_projs](../README.md)

cllm 相關的東西全收這把傘底下（2026-07-20 結構重整）：**產物本體、它的多語言應用移植、它的綁定遊樂場**。三者各自成一攤、各有入口。

| 目錄 | 是什麼 | 入口 |
|------|--------|------|
| [core/](core/) | **cllm 產物本體**——對外 C ABI 共享庫 `libcllm.so`／`.dll`（唯一入口 `llm_ask`）＋`llm` unix filter CLI。純 C++、CMake+Ninja+vcpkg+glaze。這是「真相源」，apps／lab 都建在它上。 | [AGENTS.md](core/AGENTS.md) |
| [apps/](apps/) | **handy 四工具多語言移植**（原 `cllm-apps`）：`llme`/`zhtw`/`wf`/`mail` 以 shell-out 移到各語言，轉呼 `llm`/`claude`/sibling。 | [README.md](apps/README.md) |
| [lab/](lab/) | **cllm 綁定十語言遊樂場**（原 `cllm-lab`）：用任何語言呼叫 `libcllm` 的 C ABI，`play.*` 起手檔。真相源＝core 的 `bindings/<lang>/example.*`。 | [README.md](lab/README.md) |

> 動手做某件事、或想看產物技術全貌 → 進 [core/](core/AGENTS.md)。要拿 cllm 當庫在別的語言玩 → [lab/](lab/README.md)。要看 handy 工具的各語言版 → [apps/](apps/README.md)。
