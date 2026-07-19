;; codec.fnl —— 9P2000 線格式的編解碼原語（純 Fennel，無外部依賴）。
;; 整數一律 little-endian；字串＝size[2] + UTF-8 位元組（長度即定界）。
;; 對照 unipath_9p.py 第 46～61 行的 p2/p4/p8/ps 與 Reader。

;; ---- 打包（數值 → 位元組字串）----
(fn ple [v n]
  ;; 把整數 v 編成 n 個 little-endian 位元組
  (var out "")
  (var x v)
  (for [_ 1 n]
    (set out (.. out (string.char (% x 256))))
    (set x (math.floor (/ x 256))))
  out)

(fn p2 [v] (ple v 2))
(fn p4 [v] (ple v 4))
(fn p8 [v] (ple v 8))
(fn p1 [v] (string.char (% v 256)))
(fn ps [s] (.. (p2 (length s)) s))   ; size[2] + bytes

;; ---- 拆包（Reader：對一段位元組字串維護游標，i 為 1-based 下一位）----
(fn new-reader [buf]
  {:b buf :i 1})

(fn rle [r n]
  ;; 從游標讀 n 個 little-endian 位元組成整數，游標前進
  (var v 0)
  (var mul 1)
  (for [k 0 (- n 1)]
    (set v (+ v (* mul (string.byte r.b (+ r.i k)))))
    (set mul (* mul 256)))
  (set r.i (+ r.i n))
  v)

(fn u1 [r] (rle r 1))
(fn u2 [r] (rle r 2))
(fn u4 [r] (rle r 4))
(fn u8 [r] (rle r 8))

(fn rbytes [r n]
  ;; 讀 n 個原始位元組
  (let [s (string.sub r.b r.i (+ r.i n -1))]
    (set r.i (+ r.i n))
    s))

(fn rs [r]
  ;; 讀 size[2] 再切對應長度字串
  (let [n (u2 r)]
    (rbytes r n)))

{: p1 : p2 : p4 : p8 : ps
 : new-reader : u1 : u2 : u4 : u8 : rbytes : rs}
