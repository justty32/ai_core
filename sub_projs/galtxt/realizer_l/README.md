# realizer_l —— 八股 realizer ＋ director（Common Lisp，壓縮版）

← [galtxt INDEX](../INDEX.md)｜Fennel 姊妹版：[realizer_f/](../realizer_f/README.md)｜素材：[句子模板庫](../corpus/劇本/最後的櫻花雪/句子模板庫_場景二_河堤.md)

> **realizer 的 Common Lisp 線**（macro 母語，接 comfy/gen_v1_l 的 CL 投資）。同一套八股：
> 固定旁白框、每槽多句子模板、母題軸、兩條護欄。**director 升格**：不再顯式給風格座標，
> 而是**搜**（窮舉模板×填充、成本模型挑最優）；`direct-n` 自動生 N 種台詞流——把最初手寫的
> 「20 種」閉環成搜索。零 LLM/零 RNG、確定性。三檔共 177 行（realizer 90＋director 48＋main 39）。

## 同像性核心（`tmpl` macro）

模板即 list：字串＝字面、**符號＝槽**。`defmacro tmpl` 走訪 parts，**編譯期**折成 `concatenate`：

```lisp
(tmpl d1.1 安定句 "。就算" 會變項 變化動詞 "，" 不變的錨 句尾)
;; → (list "D1.1" ("安定句" "會變項" "變化動詞" "不變的錨" "句尾")
;;         (lambda (s) (concatenate 'string (gethash "安定句" s "") "。就算" …)))
```

執行期不解析字串——這正是 macro 用武之地：schema→code。

## 跑

```sh
cd realizer_l && sbcl --script main.lisp     # load realizer→director，九 demo 全自帶 assert
```

## 檔案

| 檔 | 是什麼 |
|----|--------|
| [realizer.lisp](realizer.lisp) | 核心：`tmpl` macro（模板即 list）＋八股格架＋多模板＋填充物＋護欄＋`line`/`scene-text`（顯式座標）|
| [director.lisp](director.lisp) | **搜索層**：窮舉模板×填充→過護欄→成本挑最優（`prior`＋`intent`＋`history`）；`direct`／`direct-n` |
| [main.lisp](main.lisp) | load 定序＋九 demo |

## 九個 demo

realizer：① 全場生成｜② 確定性逐位元｜③ 段 D 三模板×合法動詞→7 種相異台詞｜④ 護欄 變項×動詞｜⑤ 護欄 收束。
director：**⑦ 搜最優**（無 intent＝缺省稿）｜**⑧ intent 甜 vs 靜 → 搜出不同風格**（取代舊「手動換座標」）｜
**⑨ `direct-n` 自動生 3 種遞減優選台詞流**（history 逼換措辭，呼應最初手寫「20 種」）。

## director：從「顯式座標」到「搜座標」

realizer 的 `scene-text` 要你**給定**每槽用哪個模板；director 把這步**升格成搜索**——
對每個台詞槽窮舉「模板×填充」組合、過護欄、按成本取最小：

```
cost = prior(偏好前面候選/首模板) + icost(intent 風格意圖) + history(用過的重罰)
```

`intent`（`:甜`/`:靜`）＝風格意圖：甜獎勵暖標記（啊／一起／陪）、靜獎勵簡短平白。
`direct-n` 每輪把用過的句丟進 `history`，逼下一輪換措辭——**同一事實層自動長出 N 種不同台詞流**。
（註：此為 realizer 側的「風格搜尋」director，與 gen_v1 設計中上游「搜 act 序列」的 director 同名而異層。）

## 壓縮筆記（壓縮帶來智能）

核心 realizer 從 Fennel 六檔（224 行）→ CL 單檔（90 行），關鍵是**結構性塌縮、非 golf**：

- **兩張 filler 表併成一張 `*fill*`**：值是 list（措辭候選）或 fn（吃母題）——「一個槽＝一個 value-producer」一種抽象兩型，`fill-slot` 塌成一行（`functionp` 分辨）。
- **scene 從具名段落塌成扁平 item 序**：段落是多餘概念，引擎只需有序 items（`:旁` / `:s`）。
- **抽 `say`** 消除 scene-text 巢狀。
- CL 的 `format`／`loop`／`multiple-value-bind`／backquote 讓引擎＋macro 共 ~28 行；其餘是**不可壓的內容**（模板／填充物）與 demo。

## 對映 gen ＋ 已知簡化

段序＝beat 文法、`*tmpl*` 多模板＝`library` 多候選、`tmpl`+`*fill*`＝槽填充、`ck`＝`requires`/`requires_pick`/canon 鎖。
簡化同 [realizer_f](../realizer_f/README.md)：座標顯式給定（未上搜尋，待接 director）；模板節錄代表款；槽型別靠人工分（d1.2 名詞型 `錨名` vs d1.1 子句型 `不變的錨`）。

> **realizer_f（Fennel）vs realizer_l（CL）** 兩版並存，也是 gen 主力語言 [三線收斂](../WAIT_USER.md) 的一個對照點：同一套八股，CL 的 macro/format 讓它更緊。
