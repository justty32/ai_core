# unipath 9P2000 server — C++ 埠

這是 `unipath_9p.py`（Python 真 9P2000 server）的 **C++ 移植**。目的只有一個：
**證明「語言中立＝講 9P」**——只要逐位元組相容 9P2000 線格式，換一種語言重寫 server，
原本那份 Python 手刻 9P client 不改一行就能與它對話成功。

- 傳輸：TCP，raw socket（BSD socket API），**不依賴 libfuse**。
- 服務的樹與 Python 版完全同一棵（邏輯對照 `unipath_env.py`）：
  - `/0` ＝ counter（int，背景 thread 每秒 +1）
  - `/1` ＝ list `["alpha","beta","gamma"]`
  - `/2` ＝ dict `{name:"world", hp:100}`
  - 每個節點是目錄，含 `data` / `ctl` / `status` 三約定葉子；
    list → 數字子路徑、dict → 字串鍵子路徑。
- 支援訊息：Tversion / Tattach / Twalk / Topen / Tread（含目錄回 stat 項）/
  Twrite（write-through 改活值）/ Tclunk / Tstat / Tflush；
  Tcreate / Tremove / Twstat 一律回 `Rerror(EROFS)`（樹結構唯讀，同 Python 版）。

## 檔案結構（每檔 < 100 行，按職責拆開）

| 檔 | 職責 |
|---|---|
| `value.hpp` | 值模型（int/str/list/dict）＋ Python 風格 repr()/str() 渲染 |
| `codec.hpp` | 9P 線格式編解碼：`p1/p2/p4/p8/ps` 與 `Reader`（LE 整數、size[2]+utf8 字串） |
| `env.hpp` / `env.cpp` / `env_io.cpp` | 活執行態環境：建樹、路徑導航、stat/readdir/read/write、背景 tick |
| `ninep.hpp` / `ninep.cpp` | 9P 常數、qid[13] 生成、stat 結構編碼（含「雙重 size」） |
| `ninep_dispatch.cpp` | 逐訊息分派（對應 Python 的 `dispatch()`） |
| `server.cpp` | TCP accept 迴圈、每連線一 thread 的收發框架、`main` |

## Build

需 g++（C++17）。已在 `g++ (GCC) 16.1.1` 驗過。

```sh
make                      # 產出 ./unipath_9p_cpp
# 或手動：
g++ -std=c++17 -O2 -Wall -pthread \
    server.cpp ninep.cpp ninep_dispatch.cpp env.cpp env_io.cpp -o unipath_9p_cpp
```

## 跑

```sh
./unipath_9p_cpp serve 5640        # 監聽 127.0.0.1:5640
```

理論上也可被 Linux 核心 v9fs 直接掛載（與 Python 版同線格式）：

```sh
sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000,uname=me 127.0.0.1 mnt
```

## 互通測試結果（主要驗收）

用**現有的 Python 9P client** 打這個 C++ server：

```sh
./unipath_9p_cpp serve 5640 &
../../.venv/bin/python ../../unipath_9p.py selftest 5640
```

實際輸出：

```
Tversion…
  Rversion msize=65536 ver=9P2000
Tattach fid=0…
  root qid ok
read /0/data = 0
readdir / = ['data', 'ctl', 'status', '0', '1', '2']
write→read /2/hp/data = 555
SELFTEST OK：真 9P2000 線格式雙向互通
```

`SELFTEST OK` ＝ Python client 與 C++ server 全程對話成功（version 協商、attach 拿根、
walk 查路徑、讀活 counter、讀目錄項、跨語言 write-through 改活值再讀回）。
**跨語言 9P 互通成立。**

### 進一步：逐位元組比對 C++ vs Python 參考 server

另用一支探針 client 對兩個 server 跑同一串請求（讀 `/1/data`、`/2/data`、
`/1/0/data`、`/2/name/data` 的 Rread 與 Rstat，以及 `/2` 目錄項），比對回應原始位元組：
**除了一個欄位外，全部 byte-identical**——包含 list repr `['alpha', 'beta', 'gamma']`、
dict repr `{'name': 'world', 'hp': 100}` 都逐位元組一致。

唯一差異：目錄列表中 `status` 子項的 `length` 欄位差 1 個 byte。原因是 `status` 葉子內容
含 `id=<物件位址>`（Python 用 `id()`、C++ 用指標值），位址位數不同導致該葉子長度差一位數。
這是**設計上就任意**的欄位（Python 版筆記也標為誠實簡化），不影響線格式相容性。

## 已知取捨 / 疑點

- **`ctl` 寫入**：此埠對 `ctl` 葉子「接收即忽略」（回報寫入位元組數但不執行 append/set/del
  動詞）。acceptance selftest 不涉及 ctl；`data` 葉子的 write-through 已完整實作。
- **`data` 寫入的字面值解析**：實作 `ast.literal_eval` 的最小子集（整數、單/雙引號字串，
  其餘退化為原字串），足夠 selftest 的 `"555" → 555`。未支援寫入 list/dict 字面值。
- **`status` 的 `id`**：用 C++ 指標值代替 Python `id()`，數值不同（如上，僅影響該葉子長度）。
- **qid.version 恆 0**、**write 忽略 offset（整段覆寫）**、**無認證**、**fid 不跨連線共享**
  ——皆刻意與 Python 參考實作對齊的簡化。
