# archived/ — 封存（非現況）

> 封存於 2026-06-28 的「事實收斂」。這裡的東西**保留設計演化的 WHY**，但**都不是現況**。
> 查現況請看事實基準：描述面 [`../../defs/axes.hpp`](../../defs/axes.hpp)、設施面 [`../../impl/`](../../impl/)。

## 內容

### 各軸被 supersede 的舊輪次（`axis_N_history.md`）
各軸 note 收斂時，把被後續輪次取代的舊討論抽出來放這。對應的「現況版」在上一層 `../axis_N_*.md`，只保留最終拍板。

| 檔 | 收的是什麼 | 現況在 |
|---|---|---|
| `axis_1_history.md` | Round 1–12（enum 路線 R1–9、bool 路線 R10–11、三碼表 R12）+ 過時的 Round 11 描述面殘留 | `../axis_1_entries.md` Round 13 |
| `axis_2_history.md` | Round 1–2（被取代的早期提案/`detail` 版型別） | `../axis_2_lifecycle.md` Round 3+4 |
| `axis_3_history.md` | Round 1（早期提案） | `../axis_3_state.md` Round 2 |
| `axis_4_history.md` | 已過時的 Round 1 描述性欄位提案（軸 4 後重定位為純設施） | `../axis_4_state_dirs.md` Round 2 + `impl/state.hpp` |
| `axis_5_history.md` | Round 1–2（含被砍掉的 `cpu` 欄位） | `../axis_5_resources.md` Round 3 |
| `axis_6_history.md` | Round 1–2（被改判的「有序階梯」框架） | `../axis_6_interruptible.md` Round 3 |

> 軸 7、8、9 的最終拍板就是 Round 1/早期定案、無被取代輪次，故**未封存**——它們的 note 整支留在 `../` 原地（只加了歷史橫幅）。

### `defs_review/`
對 defs 九軸定案的一輪**對抗性審查**（找濃縮掉了什麼、跨軸矛盾）。其提出的懸案已被 2026-06-27 的兩條全域立場（「別管 hub」、「brownfield 走 greenfield-wrap」）逐條裁決結案，故整批封存。它是**已歸檔的審查意見**，不是現況、也不是「定案錯了」的證據。
