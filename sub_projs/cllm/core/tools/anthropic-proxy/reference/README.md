# reference/ — anthropic-proxy 的 Python 原版（封存參考）

← [anthropic-proxy](../README.md)

這裡是 anthropic-proxy 的 **Python 原型**，重寫成 C++ 前的一比一版本。保留供對照翻譯邏輯、離線讀。
**不進建置、非現行實作**——現行是 [`../proxy.cpp`](../proxy.cpp)／[`../translate.cpp`](../translate.cpp)。

| 檔 | 對應 C++ |
|--|--|
| `translate.py` | [`../translate.{hpp,cpp}`](../translate.cpp)（OpenAI⇄Anthropic 翻譯，glaze json_t 港自此）|
| `proxy.py` | [`../proxy.cpp`](../proxy.cpp)（http.server→httpd、urllib→src/http）|
| `test_offline.py` | C++ 版邏輯內建於 translate；端對端測見 [`../README.md`](../README.md) |

跑（純 stdlib，零相依）：`python3 proxy.py`；離線測：`python3 test_offline.py`。
