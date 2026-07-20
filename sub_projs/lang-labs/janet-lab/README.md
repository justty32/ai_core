# janet-lab

Janet 開發環境試驗場 —— 一個能跑的 jpm 專案 + 一整套中文教學。

Janet（1.41.2）、jpm、spork、janet-lsp 都裝在 `~/.local`（原始碼編譯、不用 sudo）。
環境細節見 [docs/00-環境與工具鏈.md](docs/00-環境與工具鏈.md)。

## 馬上試

```sh
janet bin/main.janet -n Alice -n Bob 二次元    # 打招呼（純文字）
janet bin/main.janet -n alice --upper --json   # JSON 輸出
janet bin/main.janet --help                    # argparse 自動生成的說明
jpm test                                        # 跑測試
jpm build && ./build/janet-lab --json -n world  # 編成單一執行檔再跑
```

`bin/main.janet` 一支就示範了你最常用的四樣：**CLI 參數（spork/argparse）、JSON
（spork/json）、陣列 `@[]`、雜湊表 `@{}`**。核心函式在 `janet-lab/init.janet`。

## 教學

- 分篇 markdown：[docs/](docs/README.md)（00 環境 → 06 編輯器，逐篇遞進）
- 單頁速查：**`janet-cheatsheet.html`**（把語言 + 兩個庫濃縮成一頁，開瀏覽器即看）

## 結構

```
project.janet        專案宣告（依賴、要編的執行檔）
janet-lab/init.janet 核心模組（純函式）
bin/main.janet       CLI 進入點（argparse 實例）
test/basic.janet     測試
docs/                分篇教學
janet-cheatsheet.html 單頁速查表
```
