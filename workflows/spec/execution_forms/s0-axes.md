## §0 分類框架：多維描述軸

執行單元的描述空間是**多維的**。§4 的平鋪清單是毛料——把各軸上的概念先攤出來——後續重組時應以下列軸為基礎，而非繼續往清單裡追加。

| 軸 | 描述的問題 | 例子 |
|---|---|---|
| **I/O 型態** | 輸入從哪來、輸出往哪去、是否需要執行途中的互動 | 無輸入、stdin、互動式（中途需 stdin）、streaming push、streaming pull |
| **生命週期** | 執行單元從啟動到結束的持續模式 | one-shot、persistent、lazy/warm、detached |
| **跨呼叫狀態** | 單次執行以外是否保有、影響或累積狀態 | stateless、stateful-external、stateful-internal |
| **資源特性** | 對計算資源的消耗或佔用（含時間） | 執行時間、記憶體、CPU、GPU；對應 `--metadata` 的聲明內容 |
| **可中斷性** | 是否可以被暫停、暫停後可否恢復、恢復後副作用為何 | 不可中斷、可暫停（SIGSTOP）、可恢復（checkpoint/restore） |
| **執行保證** | 對系統狀態的承諾，獨立於執行次數或中途失敗 | idempotent、transactional（ACID）、dry-run |
| **組合模式** | 執行單元如何與其他單元構成更大的結構 | fan-out/fan-in、proxy/wrapper、hook/callback、agentic loop |
| **環境模式** | 執行環境是預先存在還是動態建立與銷毀 | 固定環境、ephemeral/JIT |
| **確定性 / 隨機性** | 同輸入是否同輸出；隨機環節（LLM）的認證狀態 | deterministic（預設）、nondeterministic（未認證 / 帶證書）；見 [`axis_spec.md §9`](../axis_spec/README.md) |

> 「確定性 / 隨機性」是繼前八軸之後**新增的第九軸**。前八軸隱含「函式是確定性的」前提，無一能描述
> LLM 這種天生隨機的函式，且此性質無法由任何既有軸隱含——故獨立成軸。它同時承載治理原則的證書
> （見 `roadmap.md §3.4`）。詳見 `axis_spec.md §9` 與 `lib_spec.md §9`。

同一個執行單元可以在多個軸上各有描述，例如：
- `terraform apply`：one-shot（生命週期）× idempotent + transactional（執行保證）× 固定環境（環境模式）
- `docker run --rm`：one-shot × ephemeral/JIT
- LLM Entry Manager：persistent × singleton × stateful-internal × GPU 資源約束

---

## §5 觸發機制（正交軸）

§4 的執行狀態描述「執行後的行為」；**觸發方式**是另一個正交軸，描述「由什麼啟動執行」。

| 觸發機制 | 說明 |
|---|---|
| 直接呼叫 | §4 各節隱含的預設，caller 主動發起 |
| Event-driven | 被外部事件推送觸發（message queue、webhook、filesystem watch） |
| Scheduled | 由時間觸發（cron、systemd timer） |

兩個軸可自由組合：

| 組合範例 | 說明 |
|---|---|
| scheduled + one-shot | 定時跑的批次任務 |
| event-driven + persistent | 收到事件才喚醒的常駐服務 |
| scheduled + multi-shot | 定時累積狀態的定期任務 |

Terminal 實作：

| 觸發機制 | 常見工具 |
|---|---|
| Scheduled | `cron`、`systemd timer` |
| Event-driven | `inotifywait`（filesystem）、消費 message queue 的 consumer process |
