;; scene.fnl — 河堤場景的八股格架：固定段序＋固定旁白框（不變），只有 slot 可換台詞。
;; 每段是有序 items：{:旁 固定旁白字串} 或 {:say 角色 :slot 槽名}。

(local 旁
  {:A "（斜陽把兩人的影子拉長，河水泛著細碎的金光。兩側櫻堤落下最後的櫻花雨。）"
   :B "（她停下腳步，看著飛舞的落櫻，眼神裡有一絲不易察覺的落寞。）"
   :D "（她微微一怔，轉過頭，落寞化作溫緩的笑意。）"
   :E "（她輕輕點頭，與我並肩踏上歸途，兩人的影子在水泥路上重疊在一起。最後的櫻花瓣落在她的髮梢。）"})

(local scene
  [{:name "A" :items [{:旁 旁.A} {:say "秋穗" :slot "a1"} {:say "主角" :slot "a2"}]}
   {:name "B" :items [{:旁 旁.B} {:say "秋穗" :slot "b1"}]}
   {:name "C" :items [{:say "主角" :slot "c1"} {:say "秋穗" :slot "c2"}]}
   {:name "D" :items [{:say "主角" :slot "d1"} {:旁 旁.D} {:say "秋穗" :slot "d2"}]}
   {:name "E" :items [{:say "主角" :slot "e1"} {:旁 旁.E}]}])

{: scene}
