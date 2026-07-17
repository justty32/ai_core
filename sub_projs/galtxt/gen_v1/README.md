# gen_v1 —— 事實層地基・玩具版（Lua）

← [galtxt INDEX](../INDEX.md)｜設計定調：[00_設計.md](00_設計.md)｜前身：[gen_v0/](../gen_v0/README.md)

> **核心手術：事實與敘事風格分離。** 本夾先立**事實庫**——帶型別邊的分層 LOD 網，
> `facts → director → act → realizer → 台詞` 的第一塊地基。素材沿用河鹿堂定點。
> director（planner 升格）與 realizer（敘事風格層）尚未動工。

## 跑

```sh
cd gen_v1 && lua main.lua     # Lua 5.4+；示範 ①～⑨ 全數自帶 assert
```

## 檔案

| 檔 | 是什麼 |
|----|--------|
| [00_設計.md](00_設計.md) | **設計定調**：四柱（分層／知識視圖／LOD＋canon 鎖／網）＋查詢介面草案 |
| [facts.lua](facts.lua) | **事實庫**：平表儲存、分層網語義；六動詞（resolve/match/trace/backrefs/speculate/refine）；兩道寫入門（編譯期 add+refine、執行期 act commit），不變式全架在門上 |
| [main.lua](main.lua) | 九個示範（見下） |

## 九個示範各釘一條定調

| # | 示範 | 證明什麼 |
|---|------|----------|
| ① | 節點 0／連結 1／連結的連結 2；壞 refs 被擋；「?」佔位合法 | 分層網＋接地不變式＋懸空連結 |
| ② | "90年代"→refine 1996→resolve 拿細值；再取點被擋 | 節點 LOD：粗＝約束、細化＝取點 |
| ③ | 「她心動」粗邊展開 銘記→解圍 兩段；壞鏈被擋 | 連結 LOD：多段不推翻粗邊語義 |
| ④ | 她 hides 查得；同一 ID 她看真相、他看誤會變體、外人不可見 | 知識視圖＝overlay 三態 |
| ⑤ | 他引用真相邊被「矛盾防線」擋；commit 後 canon 鎖、未呈現可改 | 執行期寫入門＋呈現才鎖 |
| ⑥ | 洩底：hides 清空、他 knows 增加、誤會變體被真相替換 | 洩底＝知識轉移、誤會拆除＝結構操作 |
| ⑦ | 在架 canon 後，已售被排斥邊擋 | 矛盾檢查＝圖結構查詢 |
| ⑧ | begin→apply→check→rollback 後快照逐位元相同 | speculate＝DFS 分支不落盤；查詢確定性 |
| ⑨ | 告白級地點解不出→掛起工單；補「櫻花樹下」後解出 | 約束細化＋編譯期 LLM 崗位 |

## 已知簡化（＝下一步工作清單）

1. **director 未動工**：gen_v0 的 v1 引擎還沒升格到「搜尋 act 序列」——speculate 介面已為它備好咬合點。
2. **realizer 未動工**：act → 字面的渲染層（敘事風格），使用者定調之後再談。
3. 視圖是「knows∪hides 可見、其餘不可見」的粗版；「知道誰知道」只做到變體一階。
4. refine 的節點取點只支援數值區間約束；掛起工單只入列、還沒有出列流程（人審清單）。
5. speculate 的 check 只驗不寫，delta 疊加是靠「commit 前不落盤」——facts 若長出快取需改真 delta 堆疊。
