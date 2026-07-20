;; llm.hy — cllm 的 Hy binding：薄包裝，直接重用 Python binding（../python/llm.py）。
;;
;; Hy 骨子裡是 Python（Hy 原始碼編譯成 Python AST 在 CPython 上跑），所以最誠實、最省的
;; Hy binding 不是重造一份 ctypes，而是**直接載入現成的 Python binding**、再以 Hy 慣例
;; 薄薄包一層。Hy 呼叫時的連字號關鍵字會自動 mangle 成底線，故：
;;
;;     (import llm)
;;     (llm.ask "你好")                                     ; 只給 prompt（走內建 localhost）
;;     (llm.ask "你好" "http://…/chat/completions")          ; prompt + endpoint（位置形式）
;;     (llm.ask "你好" :model "local-model" :temperature 0.7)
;;     (llm.ask "數到五" :stream True                         ; 串流：逐段進 :on-delta
;;              :on-delta (fn [p] (print p :end "" :flush True) False))  ; 回真值可中止
;;
;; :on-delta→on_delta、:api-key→api_key、:max-tokens→max_tokens…（Hy 自動轉），一律直接對上
;; Python binding 的 kwargs。回「完整組合後的答案字串」；失敗時給了 :on-error 就呼叫它並回 None，
;; 否則 raise LLMError。完整選項／進階欄位（tools/on_tool、media/on_media、modalities）語意與
;; Python binding 逐欄一致——真相源就是它，見 ../python/llm.py 的模組 docstring。
;;
;; ⚠ 為何用 importlib 而非 (import llm)：本模組自己就叫 `llm`，import 期已在 sys.modules 佔了
;;    `llm` 這名字；若內部再 (import llm) 想拿 Python 版，會撞回自己（半初始化）→ 自我循環。
;;    故以顯式檔路徑把 ../python/llm.py 載成一個私有名（_cllm_pyllm），與模組名徹底脫鉤。
;;    LIBCLLM 路徑由 Python binding 依它自己的 __file__（core/bindings/python/llm.py）推得
;;    → core/build/libcllm.{dll,dylib,so}，跨平台自動選副檔名；環境變數 LIBCLLM 仍可覆寫。
(import os importlib.util)


(defn _load-pyllm []
  "以顯式檔路徑載入 sibling 的 Python binding（../python/llm.py），回模組物件。"
  (setv here (os.path.dirname (os.path.abspath __file__)))
  (setv pyfile (os.path.join here ".." "python" "llm.py"))
  (setv spec (importlib.util.spec_from_file_location "_cllm_pyllm" pyfile))
  (setv mod (importlib.util.module_from_spec spec))
  (.exec_module spec.loader mod)
  mod)


(setv _pyllm (_load-pyllm))

;; 對外重新導出：Hy 端就用這兩個名字（等同 Python 的 llm.ask / llm.LLMError）。
(setv LLMError _pyllm.LLMError)


(defn ask [prompt [endpoint None] #** kwargs]
  "問 LLM，回完整答案字串。轉呼 Python binding 的 ask；endpoint 可位置或 :endpoint 關鍵字給，
   其餘選項一律 kwargs（連字號自動 mangle 成底線）。詳見本檔頂 docstring 與 ../python/llm.py。"
  (_pyllm.ask prompt endpoint #** kwargs))
