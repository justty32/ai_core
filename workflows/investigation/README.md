# investigation — 技術/可行性調查的家

← [WORKFLOWS](../../WORKFLOWS.md)｜[INDEX](../../INDEX.md)

**技術/可行性調查**的落點：某個技術「能不能做／怎麼做／有哪些選項與代價」的**事實性查證**——有別於 [ideas/research/](../ideas/research/README.md) 的「論文 × 北極星火花表」（那是靈感碰撞、提案層），這裡是**摸清事實與底層機制**，供設計/規範時取用。

## 什麼進這裡

- 「某個 OS／語言／工具能怎麼做 X」的 how-to 查證（附**可跑的最小範例**）。
- 選型比較（A vs B 的 API／權限／可攜性代價）。
- 可行性勘查（做某功能前先摸清底層機制）。

不進這裡：論文碰撞火花 → [ideas/research/](../ideas/research/README.md)；定案規範 → [spec/](../spec/)；施工計畫 → [plans/](../plans/README.md)。

## 形式

每份調查一個**資料夾**（撐大再拆／小的可單檔），內含 `README.md`（總覽＋速查表）＋分類細節檔。**事實盡量在真機驗證過再寫**（附驗證方式），文檔照結構整理原則守 ≤8192 bytes/檔。

## 目前的調查

| 調查 | 主題 |
|------|------|
| [linux-file-metadata/](linux-file-metadata/README.md) | Linux 檔案中繼資料怎麼設（**C API × CLI**）：權限/擁有者/時間戳、延伸屬性 xattr/ACL、inode flags/安全上下文/file capabilities |
