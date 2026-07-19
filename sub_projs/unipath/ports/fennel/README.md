# unipath 9P2000 server —— Fennel 移植版

> 「語言中立＝講 9P」的證明：這支 server 用 **Fennel**（編譯到 Lua 的 Lisp）
> 重寫 `../../unipath_9p.py` 的 9P2000 線格式，**逐位元組相容**它，於是同一份
> 獨立的 Python 9P client（`unipath_9p.py selftest`）能直接跟它互通——驗到的是
> 「線格式」而非「函式呼叫」。

## 是什麼

一個走 TCP 的真 9P2000 server，服務跟 Python 版**同一棵示範樹**：

- 根是 list：`/0`＝counter(int)、`/1`＝`["alpha","beta","gamma"]`、`/2`＝`{name:"world", hp:100}`
- 每個節點都是目錄，含 Plan 9 慣例葉子 `data`/`ctl`/`status`
- list → 數字子路徑（0-based）、dict → 字串鍵子路徑
- 支援訊息：Tversion / Tattach / Twalk / Topen / Tread（目錄回 stat 項）/ Twrite /
  Tclunk / Tstat / Tflush；`Tcreate`/`Tremove`/`Twstat` 一律回 `EROFS`（樹唯讀，
  但既有葉子 `data` 可寫）
- 整數 little-endian、字串 `size[2]+utf8`、`qid[13]`、stat「雙重 size」全數對齊

## 怎麼跑

```sh
# 啟 server（預設 port 5641）
./serve.sh 5641

# 另開一個終端，用 Python 版的獨立 9P client 驗互通
../../.venv/bin/python ../../unipath_9p.py selftest 5641
```

看到 `SELFTEST OK` ＝ Python client 跟這支 Fennel server 跨語言互通成功。

`serve.sh` 會先把每個 `.fnl` 編成 `.lua`（用 `fennel --compile`），再用 `luajit`
跑 `main.lua`。（`.lua` 是編譯產物，已 gitignore。）

## 需要哪些依賴

- **fennel**（本機驗於 1.6.1）—— 編譯 `.fnl` → `.lua`
- **luajit**（含 FFI）—— 執行；socket 靠 **LuaJIT FFI 直綁 syscall**
  （`socket/bind/listen/accept/read/write/close`，見 `fsock.fnl`），因為本機
  **沒有 luasocket**（`luarocks` 在但未裝），所以走裸系統呼叫，不引入外部 rock。

## 互通結果（實測）

```
Tversion…
  Rversion msize=65536 ver=9P2000
  root qid ok
read /0/data = 0
readdir / = ['data', 'ctl', 'status', '0', '1', '2']
write→read /2/hp/data = 555
SELFTEST OK：真 9P2000 線格式雙向互通
```

（Python `selftest` 行程 exit code = 0。）

## 檔案結構（每檔 < 100 行）

| 檔 | 職責 |
|---|---|
| `codec.fnl` | 線格式編解碼原語：p1/p2/p4/p8/ps + Reader（u1/u2/u4/u8/rs/rbytes） |
| `env.fnl` | 環境核心：示範樹的節點模型 + 路徑導航（resolve/children） |
| `envio.fnl` | 環境對外 API：stat / readdir / read / write + 葉子渲染 |
| `ninep.fnl` | 9P 共用原語：型別常數、qid、stat 編碼、錯誤 |
| `dispatch.fnl` | 逐訊息分派（每個 T… 一段），對照 Python `dispatch()` |
| `fsock.fnl` | LuaJIT FFI socket 綁定（TCP server 端） |
| `server.fnl` | wire 收發迴圈 + accept 迴圈 |
| `main.fnl` | 入口：解析 argv、啟動 |
| `serve.sh` | 編譯 + 執行的一鍵腳本 |

## 老實的簡化（相對 Python 版）

- **無背景 tick**：Python 版有一條 thread 每秒把 counter +1；這裡 LuaJIT 端不起
  thread，counter 恆為初值 `0`（`read /0/data = 0`）。互通驗證不需要它會動，值
  是合法整數字串即可。
- **連線逐條序列處理**：一次服務一條 TCP 連線（不 spawn thread），對 selftest
  （單一 client）足夠；要並發時再補。
- **ctl 寫入簡化**：`ctl` 葉子接受寫入但只回長度、不執行 append/set/del（selftest
  未用到）；`data` 葉子的 write-through 完整可用（實測 `write→read = 555`）。
- 其餘 Python 版本身就有的簡化（qid.version 恆 0、write 忽略 offset＝整段覆寫、
  無認證、Topen mode 不做權限檢查）此處一併沿用，以維持逐位元組相容。
