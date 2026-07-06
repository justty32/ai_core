# impl_remake — defs 的實現（復刻）

> 範圍鐵則：本目錄**只**復刻「`defs/axes.hpp` 九軸描述型別」變成可觀察行為的那一橋，
> **不**搬 `impl/` 的設施金字塔（io / state / cli / serve / shell / http / llm / rate / taming…）。
> 那些是替程式做事的設施層，與 defs 的「描述型別」是兩回事。

## 內容（完成）

| 檔 | 角色 | 對應 |
|---|---|---|
| [`json.hpp`](json.hpp) | 最小 JSON emitter 原語（`ac::json::str/join/obj/field`，Meta 無關） | `impl/json.hpp`（parser）的 emitter 半邊 |
| [`meta_json.hpp`](meta_json.hpp) | `ac::Meta` → `--metadata` JSON 序列化（emit，Meta 專屬塑形） | `impl/meta_json.hpp` |
| [`intercept.hpp`](intercept.hpp) | `intercept(argc,argv,meta)`：命中 `--metadata` 吐 JSON + exit（自足寫 stdout，不拉設施層） | `impl/intercept.hpp` |

三檔合起來把 defs 全九軸（entries / persistent / stateful / resources / interruptible /
guarantee / dry_run / uncertainty + 單一 `extra` 袋）變成可觀察的 `--metadata` 行為。
`register/intercept` 模型：純宣告（`Meta`）零副作用，`--metadata` 生效靠 `main()` 顯式呼叫 `intercept()`。

## 驗證

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -I.. <你的 main>.cpp   # 只需 ../defs/axes.hpp
```

純標準庫、跨平台可編；不依賴任何 POSIX 設施。
