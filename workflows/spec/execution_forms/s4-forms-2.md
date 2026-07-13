## §4 函式形式在 terminal 中的對應（續：4.10–4.18）

### 4.10 Suspendable / Resumable（可中斷與恢復）

**定義**：執行到一半可以被掛起（暫停），釋放當下的運算或網路資源，並在未來某個時間點從中斷處繼續執行，而不是從頭開始。

Terminal 表現：
- OS 層級：`Ctrl+Z` 觸發 `SIGSTOP`，後續以 `fg` / `bg` 恢復
- 應用層級：支援斷點續傳的工具（`wget -c`、`rsync --partial`），或使用 CRIU 保存行程記憶體狀態

與 Lazy / Warm 的差異：Lazy 是在等待「下一次新的呼叫」；Resumable 是暫停「當前尚未完成的長期任務」。

---

### 4.11 Interactive Wizard（步進互動式）

**定義**：以收集參數為目的的短期互動。按順序詢問一系列問題，收集完畢後執行一次性任務，然後結束。

Terminal 表現：`npm init`、`ssh-keygen`、`apt-get install`（遇到需配置選項時）。

與 Persistent 子型 A（REPL）的差異：REPL 的生命週期是無窮迴圈（Read-Eval-Print Loop）；Wizard 是有限狀態機，走完特定設定流程後，轉化為 One-shot 執行並結束。

---

### 4.12 Transactional（事務式）

**定義**：一系列步驟要麼全部成功（commit），要麼全部回滾到初始狀態（rollback），不留中間殘局。

Terminal 表現：`git commit`（all-or-nothing）、資料庫 transaction、`rsync` 的原子替換（先寫 temp 再 rename）。

與 Idempotent 的差異：Idempotent 保證「重複執行安全」；Transactional 保證「中途失敗不留殘局」。兩者正交，常一起出現（如 Terraform apply：idempotent + transactional）。

---

### 4.13 Memoized / Cached（快取式）

**定義**：同樣的輸入若已計算過，直接回傳快取結果而不重新執行，計算只發生一次（或快取失效前只發生一次）。

Terminal 表現：`ccache`（C/C++ 編譯快取）、`pip` 的 wheel cache、`make` 的 target 時間戳依賴判斷。

與 Idempotent 的差異：Idempotent 關注「副作用安全性」（重複執行不造成多餘改變）；Memoized 關注「計算的跳過」（輸入相同則完全略過執行）。前者是 correctness 保證，後者是 performance 最佳化。

---

### 4.14 Dry-Run / Simulation（模擬執行 / 試運行）

**定義**：執行完整的邏輯運算與狀態比對，但攔截所有對系統的實際修改（副作用），僅向 stdout 輸出「如果真的執行，將會發生什麼事」。

Terminal 表現：`terraform plan`、`rsync -n`（或 `--dry-run`）、`apt-get install -s`。

與 One-shot 的差異：這是對「有副作用任務」的唯讀鏡像。它回傳的是執行計畫或意圖（Intent），讓呼叫者在實際改變狀態前進行確認，是基礎設施與高風險操作的必備形式。

---

### 4.15 Hook / Callback（掛鉤 / 生命週期回呼）

**定義**：不預期由人類使用者直接呼叫。被「註冊」到另一個宿主（Host）應用程式的生命週期中，在特定事件發生時被系統性地觸發。

Terminal 表現：Git hooks（`.git/hooks/pre-commit`）、Linux 套件管理器 hooks（pacman hooks）、systemd 服務的 `ExecStartPre` / `ExecStopPost`。

執行約束：I/O 與環境變數完全由宿主提供。Exit code 直接決定宿主行程是否能繼續推進（hook 回傳非 0，git commit 即被強制中斷）。

---

### 4.16 Ephemeral / JIT Execution（臨時 / 用後即棄執行）

**定義**：執行前動態拉取依賴、建構環境，執行一次性任務後立刻銷毀整個運行環境，不留任何痕跡。

Terminal 表現：`npx <package>`、`docker run --rm <image>`、`nix run`。

與 One-shot 的差異：One-shot（如 `grep`）假設執行環境與依賴已固定存在；Ephemeral 將「環境的建構與銷毀」封裝進單次呼叫中，保證極致的環境隔離與無狀態性。

---

### 4.17 Proxy / Wrapper（代理 / 包裝器）

**定義**：包裝在目標指令外層，攔截、監控或修改其環境變數、I/O 串流或系統呼叫，然後將控制權透傳（Pass-through）給內部實際運作的行程。

Terminal 表現：`time <command>`（測量耗時）、`strace <command>`（追蹤系統呼叫）、`sudo <command>`（提權）、`env VAR=1 <command>`。

與 Pipeline 中介點的差異：Pipeline 節點只處理純資料；Proxy / Wrapper 處理的是「執行環境的殼」。其生命週期與被包裝的子行程（Child Process）完全綁定。

---

### 4.18 Agentic / Autonomous Loop（自主代理迴圈）

**定義**：高度動態的 Multi-shot 變體。行程啟動後，根據自身的輸出或外部環境的反饋，自主決定下一步的執行路徑，直到滿足某個終止條件。

Terminal 表現：AutoGPT 終端介面、具備 Tool-use / Function Calling 的本地端 LLM 代理腳本。

執行特徵：傳統 Shell Script 或 Pipeline 是預先寫死的有向無環圖（DAG）；Agentic 形式的執行流程是**執行期動態生成**的，具備「觀察 → 思考 → 行動」的非確定性迴圈。
