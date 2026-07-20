# reference/ — llm-login 的 Python 原版（封存參考）

← [llm-login](../README.md)

llm-login 的 **Python 原型**，重寫成 C++（C-ABI lib）前的一比一版本。保留供對照 OAuth／PKCE 邏輯。
**不進建置、非現行實作**——現行是 [`../login.cpp`](../login.cpp) 等（`liblogin.so`＋`llm-login` CLI）。

| 檔 | 對應 C++ |
|--|--|
| `oauth.py` | [`../oauth.{hpp,cpp}`](../oauth.cpp)（PKCE／authorize URL／換刷 token）＋[`../crypto.cpp`](../crypto.cpp)（SHA-256/base64url）|
| `store.py` | [`../store.{hpp,cpp}`](../store.cpp)（token 存放／patch config）|
| `llm_login.py` | [`../login.cpp`](../login.cpp)（編排＋C-ABI）＋[`../login_cli.cpp`](../login_cli.cpp)（CLI）|
| `test_offline.py` | [`../test_offline.cpp`](../test_offline.cpp) |

跑（純 stdlib，零相依）：`python3 llm_login.py login`；離線測：`python3 test_offline.py`。
