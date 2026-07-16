-- conditions.lua — G(條件) 的唯一輸入。純資料、無隨機種子。
-- 這張 table 就是「條件向量」：生成器是編譯器，這裡是源碼。
-- 欄位分五組：version / world / cast / relation / scene / history。
-- 同一張 table 餵進去，永遠得到同一段台詞（確定性鐵則）。

return {

  -- 表也是條件：規則表/句庫的版本。換版本＝換輸出是合法的；
  -- 同版本不同輸出＝bug。
  version = { tables = "2026-07-17a" },

  -- 世界狀態（可由更上游推導：日期→季節、天氣……v0 直接明給）
  world = {
    season = "盛夏",
    time   = "黃昏",
    place  = "河鹿堂",
  },

  -- 卡司（dossier 的鍵）
  cast = {
    heroine     = "柏木秋穗",
    protagonist = "佐伯悠",
  },

  -- 關係狀態（02 敬語與稱謂階的狀態機快照）
  relation = {
    stage   = 2,      -- 稱謂階：她→「佐伯」、他→「柏木」由階表查出，不在這裡手寫
    quarrel = false,  -- 吵架中＝暫退半階（v0 未用）
  },

  -- 這一場戲想寫什麼（呼叫方的「劇本意圖」）
  scene = {
    coordinate = {
      topic  = "A1",   -- 話題入口（02 軸表：A1 推理文庫）
      give   = "B1",   -- 誰跨半步（B1 她讓渡）
      gap    = "C2",   -- 反差格（C2 口頭禪遮掩）
      ending = nil,    -- 收尾：nil ＝ 交給 D≈f(A) 由話題溫度派生
    },
    emotion = { from = "平靜", to = "被在意到" },  -- 本場情緒行程（不跳階）
    narrative_budget = 0,  -- 敘事能量預算：0＝量產模式，✕ 格買不起
  },

  -- 歷史入條件：重複感的確定性解。
  -- 已用過的句子在這裡登記，生成器施以確定性懲罰——函數仍純。
  history = {
    used_lines = {},   -- 例：{ ["B4B5/她讓書·籃底抽出"] = true }
  },
}
