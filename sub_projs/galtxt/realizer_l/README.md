# realizer_l —— 八股 realizer（Common Lisp，壓縮版）

← [galtxt INDEX](../INDEX.md)｜Fennel 姊妹版：[realizer_f/](../realizer_f/README.md)｜素材：[句子模板庫](../corpus/劇本/最後的櫻花雪/句子模板庫_場景二_河堤.md)

> **realizer 的 Common Lisp 線**（macro 母語，接 comfy/gen_v1_l 的 CL 投資）。同一套八股：
> 固定旁白框、每槽多句子模板、母題軸、兩條護欄。`G(母題,風格座標)→整場河堤台詞`，零 LLM/零 RNG、確定性。
> **單檔 109 行**（Fennel 六檔共 224 行的壓縮對照）。

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
cd realizer_l && sbcl --script realizer.lisp     # 六 demo 全自帶 assert
```

## 六個 demo

① 全場生成（母題＝櫻花）｜② 確定性逐位元｜③ 段 D 三模板×合法動詞→7 種相異台詞（多句子模板）｜
④ 護欄一 變項×動詞相配（季節×謝了被擋）｜⑤ 護欄二 收束護欄（錨引入咖啡廳被擋）｜⑥ 換座標＝換風格。

## 壓縮筆記（壓縮帶來智能）

從 Fennel 六檔（224 行）→ CL 單檔（109 行），關鍵是**結構性塌縮、非 golf**：

- **兩張 filler 表併成一張 `*fill*`**：值是 list（措辭候選）或 fn（吃母題）——「一個槽＝一個 value-producer」一種抽象兩型，`fill-slot` 塌成一行（`functionp` 分辨）。
- **scene 從具名段落塌成扁平 item 序**：段落是多餘概念，引擎只需有序 items（`:旁` / `:s`）。
- **抽 `say`** 消除 scene-text 巢狀。
- CL 的 `format`／`loop`／`multiple-value-bind`／backquote 讓引擎＋macro 共 ~28 行；其餘是**不可壓的內容**（模板／填充物）與 demo。

## 對映 gen ＋ 已知簡化

段序＝beat 文法、`*tmpl*` 多模板＝`library` 多候選、`tmpl`+`*fill*`＝槽填充、`ck`＝`requires`/`requires_pick`/canon 鎖。
簡化同 [realizer_f](../realizer_f/README.md)：座標顯式給定（未上搜尋，待接 director）；模板節錄代表款；槽型別靠人工分（d1.2 名詞型 `錨名` vs d1.1 子句型 `不變的錨`）。

> **realizer_f（Fennel）vs realizer_l（CL）** 兩版並存，也是 gen 主力語言 [三線收斂](../WAIT_USER.md) 的一個對照點：同一套八股，CL 的 macro/format 讓它更緊。
