;; main.fnl — realizer 玩具版 demo：G(母題,風格座標)→整場台詞。全 assert 自檢。
;; 跑：fennel main.fnl（需 FENNEL_PATH/FENNEL_MACRO_PATH 指到本夾——見 README）

(local {: scene-text : enum-line : line : check} (require :engine))
(local {: templates} (require :templates))

(fn hr [s] (print (.. "\n========== " s " ==========")))

;; ① 全場生成（母題＝櫻花，缺省座標）
(hr "① 全場生成：母題＝櫻花，缺省風格座標")
(local s1 (scene-text "櫻花" {}))
(print s1)
(assert (string.find s1 "河堤" 1 true) "應含入場旁白")
(assert (string.find s1 "沒關係的。就算櫻花變了" 1 true) "d1 應填出安定句＋相配的變化動詞")
(assert (string.find s1 "走吧" 1 true) "應收在段 E 走向車站")

;; ② 確定性：同輸入再跑一次，逐位元相同
(hr "② 確定性自檢")
(assert (= s1 (scene-text "櫻花" {})) "確定性破功：同輸入出了不同稿")
(print "同輸入 → 逐位元相同 ✓")

;; ③ 多模板：段 D 主角托底句，三種結構各生一版（證實「多句子模板」）
(hr "③ 多模板生成：段 D（d1）三種句型結構")
(local ds (enum-line "d1" "櫻花"))
(each [_ e (ipairs ds)] (print (.. "  [" e.id "｜" e.動 "] " e.text)))
(local seen {})
(each [_ e (ipairs ds)] (tset seen e.text true))
(var n 0) (each [_ (pairs seen)] (set n (+ n 1)))
(assert (>= n 3) "三種模板應生出至少三種相異台詞")
(print (.. "相異台詞數：" n))

;; ④ 護欄一：變項×動詞不相配（母題=季節，硬塞「謝了」）
(hr "④ 護欄：變項×動詞相配")
(local bad (line (. (. templates "d1") 1) "季節" {"變化動詞" 2}))  ; 自由填變化動詞[2]=謝了
(assert (not bad.ok) "季節×謝了 應被擋")
(print (.. "被擋 ✓：" bad.why))

;; ⑤ 護欄二：收束護欄（錨引入非 canon 場景）
(hr "⑤ 護欄：收束護欄（錨不得引入新場景）")
(local (ok why) (check {"不變的錨" "我們改天去咖啡廳坐坐"} "櫻花"))
(assert (not ok) "非 canon 錨應被擋")
(print (.. "被擋 ✓：" why))

;; ⑥ 換座標＝換風格：母題=風景，a1 用第二模板、d1 用正反對比(第二)
(hr "⑥ 換風格座標：母題＝風景，a1→模板2、d1→正反對比")
(local s2 (scene-text "風景" {"a1" {:t 2} "d1" {:t 2}}))
(print s2)
(assert (not (= s1 s2)) "不同座標應出不同稿")

(print "\n全部 demo 通過——八股 realizer（Fennel＋macro）立住了。")
