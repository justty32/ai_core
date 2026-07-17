# realizer_f —— 八股 realizer 玩具版（Fennel ＋ Lisp macro）

← [galtxt INDEX](../INDEX.md)｜素材出處：[劇本/最後的櫻花雪/句子模板庫_場景二_河堤.md](../corpus/劇本/最後的櫻花雪/句子模板庫_場景二_河堤.md)＋[八股拆解](../corpus/劇本/最後的櫻花雪/模板_場景二_河堤_八股拆解.md)

> **敘事風格層（realizer）的第一塊真程式碼。** 把河堤場景的「八股」——固定旁白框、切五段、
> 每槽多句子模板、母題軸、兩條承接護欄——做成能跑的生成器：`G(母題, 風格座標) → 整場台詞`。
> 執行期零 LLM、零 RNG、確定性（同 gen_v0 精神）。**用 Fennel＋macro，正中「同像性：模板即程式」的初衷。**

## 同像性核心：模板就是一個 list（不是要 parse 的字串）

`macros.fnl` 的 `tmpl` macro——句子模板寫成 **vector**：字串＝字面、**符號＝槽**。macro 走訪這個 list，
**編譯期**把它折成直接字串接合，不做任何 runtime 解析：

```fennel
(tmpl :a1.1 ["結果，還是" 收束動詞 收束地 "了呢。"])
;; 編譯成（Lua）：
;;   {id="a1.1", slots={"收束動詞","收束地"},
;;    fill=function(st) return ("結果，還是" .. st["收束動詞"] .. st["收束地"] .. "了呢。") end}
```

編出來的 `fill` 是純 `..` 接合、**零 `gsub`**（`fennel --compile engine.fnl | grep gsub` ＝ 0）。
這就是 Lisp macro 的用武之地：模板即 AST，schema→code 由 macro 生成。

## 跑

```sh
cd realizer_f && fennel main.fnl     # Fennel 1.5+；靠預設 ./?.fnl 找 require 與 import-macros
```

## 檔案（單檔 ≤120 行慣例）

| 檔 | 是什麼 |
|----|--------|
| [macros.fnl](macros.fnl) | **同像性核心**：`tmpl` macro——模板 list（字串/符號）編譯期折成填充函式 |
| [scene.fnl](scene.fnl) | 八股格架：固定段序 A→E＋固定旁白框（不變），每段有序 items（旁白｜台詞槽）|
| [templates.fnl](templates.fnl) | 每槽的**多種句子模板**（d1 三結構：讓步反問／正反對比／先陪伴後讓步）＝gen 句庫多候選 |
| [fillers.fnl](fillers.fnl) | 槽填充物＋母題軸＋兩條相配資料（變項×動詞 map、非 canon 地點表）|
| [engine.fnl](engine.fnl) | 填槽→過護欄→組台詞；`scene-text`（整場）＋`enum-line`（某槽多模板窮舉）|
| [main.fnl](main.fnl) | 六個 demo，全自帶 assert |

## 六個 demo 各釘一條

| # | demo | 證明 |
|---|------|------|
| ① | 母題＝櫻花、缺省座標 → 整場台詞 | `G(母題,座標)→台詞` 走通、旁白框固定、母題貫串 |
| ② | 同輸入再跑逐位元相同 | 確定性（無 RNG、固定序、缺省取首）|
| ③ | 段 D 三模板×合法動詞 → 7 種相異台詞 | **多句子模板**生成；選哪模板＝realizer 風格維度 |
| ④ | 母題=季節硬塞「謝了」被擋 | 護欄一：變項×動詞相配（gen `requires` 翻版）|
| ⑤ | 錨引入「咖啡廳」被擋 | 護欄二：收束護欄（canon 鎖翻版，不得長新場景）|
| ⑥ | 換母題＋換模板座標 → 不同稿 | 風格座標＝realizer 的搜尋維度 |

## 對映 gen（realizer 怎麼接回引擎）

| 本夾 | gen_v0 對應 |
|------|-------------|
| 固定段序＋旁白框（scene.fnl）| beat 文法＋beat 骨架 |
| 每槽多模板（templates.fnl）| `library[beat]` 多候選 |
| `tmpl` 折出的 `fill`＋fillers | 槽填充 `${slot}`＋填充表 |
| 母題同軸／變項×動詞／收束護欄（engine.fnl `check`）| `requires`／`requires_pick`／canon 鎖 |
| 風格座標 coord | L1/L2 的候選挑選（此處為顯式給定，未上搜尋）|

## 已知簡化（＝下一步）

1. 座標是**顯式給定**，還沒像 gen_v0 那樣上「窮舉＋成本」搜尋最優風格——下一步可加 director 層搜 act 序列後接本 realizer。
2. 模板庫只節錄代表款（d1 全收）；完整 20/多模板在 [句子模板庫](../corpus/劇本/最後的櫻花雪/句子模板庫_場景二_河堤.md)。
3. 不同模板需不同**型別**的槽（d1.2 要名詞型 `錨名`、d1.1 要子句型 `不變的錨`）——目前靠人工分槽，未做槽型別系統。
4. 母題只推 `會變項/變樣句/外界觀察` 三槽；更細的母題→措辭映射待擴。
5. 只此一場（河堤）；跨場景復用要抽出「旁白框＋槽表」的通用結構。
