# 03 · JSON（spork/json）

`spork/json` 是官方標準庫裡的 JSON 模組——**原生 C 實作**，快。已隨 spork 裝好。

```janet
(import spork/json)
```

## 為什麼 Janet 處理 JSON 特別順

因為 **Janet 的字面語法幾乎就是 JSON**（見 [01 括號家族](01-語言速成.md)）：

| JSON | Janet |
|------|-------|
| object `{}` | struct `{}` / table `@{}` |
| array `[]` | tuple `[]` / array `@[]` |
| `"key"` | keyword `:key` |
| string / number / `true` / `false` | 同名 |
| `null` | keyword `:null`（或解碼時轉 `nil`） |

所以你可以**直接手寫像 JSON 的 Janet 資料**，還能內嵌計算值，再一行編成 JSON：

```janet
(def name "Bob") (def ids [1 2 3])
(json/encode {:user name :ids ids :meta {:active true :score (+ 40 2)}} "  " "\n")
# {
#   "user": "Bob",
#   "ids": [ 1, 2, 3 ],
#   "meta": { "active": true, "score": 42 }
# }
```

## encode（Janet → JSON）

簽名：`(json/encode x &opt tab newline buf)`

```janet
(json/encode {:name "Bob" :n 3})          # 緊湊：{"name":"Bob","n":3}
(json/encode data "  " "\n")              # tab="  " newline="\n" → 縮排好讀
(json/encode data "" "" @"prefix:")       # 第 4 參 buf：附加到既有 buffer，回傳該 buffer
```

**注意**：要用第 4 個參數 buf，前面 tab / newline 不能省成 `nil`（會報型別錯），傳空字串 `"" ""`。

## decode（JSON → Janet）

簽名：`(json/decode json-source &opt keywords nils)`

```janet
(json/decode "{\"name\":\"Bob\"}")        # => @{"name" "Bob"}   key 是「字串」
(json/decode "{\"name\":\"Bob\"}" true)   # => @{:name "Bob"}    key 轉 keyword（建議）
```

兩個開關：
- **`keywords` (第 2 參)** 傳 `true`：物件的 key 從字串轉成 keyword，取值就能 `(d :name)`，
  跟你自己寫的 Janet 一致。**建議一律傳 `true`。**
- **`nils` (第 3 參)** 傳 `true`：JSON 的 `null` 解成 Janet 的 `nil`；否則預設解成 keyword `:null`。

```janet
(json/decode "null")                      # => :null      （預設）
(json/decode "null" false true)           # => nil        （nils=true）
```

⚠ **地雷**：table 不能存 `nil` 值——若某鍵的值是 `null` 且你開了 `nils=true`，那個**鍵會直接消失**：

```janet
(json/decode "{\"a\":1,\"b\":null}" true true)   # => @{:a 1}   （:b 不見了！）
```

要保留「這個鍵存在但值是空」的語意，就別開 `nils`，讓它留成 `:null` 自己判斷。

## 檔案 round-trip

```janet
(def cfg @{:debug true :ports @[8080 8081]})
(spit "cfg.json" (json/encode cfg "  " "\n"))       # 寫檔（含縮排）
(def back (json/decode (slurp "cfg.json") true))    # 讀回，keyword key
(back :debug)                                       # => true
```

`spit` / `slurp` 是 Janet 內建的整檔寫 / 讀。

## 常見操作

**改一個欄位再寫回**：

```janet
(def j (json/decode (slurp "cfg.json") true))
(put j :debug false)
(spit "cfg.json" (json/encode j "  " "\n"))
```

**改巢狀的值**——用 `update-in`（走一條 key/index 路徑，用函式更新）：

```janet
(def j (json/decode "{\"a\":{\"b\":[10,20,30]}}" true))
(update-in j [:a :b 1] |(* $ 100))        # j => @{:a @{:b @[10 2000 30]}}
```

**取巢狀的值**——`get-in`（別用 `(j :a :b)`，第二參是預設值不是下一層）：

```janet
(get-in j [:a :b 1])                      # => 2000
```

**遍歷 / 轉換**：decode 出來就是普通 array / table，直接 `map` / `filter` / `each`：

```janet
(def users (json/decode "[{\"n\":\"A\"},{\"n\":\"B\"}]" true))
(map |($ :n) users)                       # => @["A" "B"]
```

## 小抄

| 想做 | 怎麼寫 |
|------|--------|
| Janet → 緊湊 JSON | `(json/encode x)` |
| Janet → 縮排 JSON | `(json/encode x "  " "\n")` |
| JSON → Janet（keyword key） | `(json/decode s true)` |
| JSON null → nil | `(json/decode s true true)` |
| 印真 JSON | `(print (json/encode x))` |
| 改巢狀 | `(update-in j path f)` / `(put-in j path v)` |
| 取巢狀 | `(get-in j path)` |

下一步：[04-cli-argparse.md](04-cli-argparse.md)。
