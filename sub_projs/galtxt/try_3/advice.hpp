#pragma once
// advice.hpp — galtxt try_3：對外 C ABI 的「設計提案」（給你 review
// 討論用，非落地建置檔）。
//
// 這份只放 [L1] C ABI（llm_cabi）：一支 llm_ask 吃下
// prompt＋schema＋tools＋media＋modalities＋stream。 包在它外的 [L2] modern-C++
// facade 已抽到同資料夾的 advice_cpp.hpp。 兩層底下再由 llm_ask 的 .cpp
// 實作接到既有 L0 核心（Client::ask／ask_tools/…；本檔不含那段）。
//
// ★ 這份設計我已實作＋離線 fixture 驗過會動（含 C ABI 純 C 客戶端連 .so
// 全綠），再撤回實作、只留介面供你審。
//
// ── 我替你做的判斷（都可回退，逐條在對應處也有註）──
//  1. 扁平 11 參數的 llm_ask → 收成 llm_request_t（輸入）＋
//  llm_handlers_t（出口回呼）兩個 struct。
//  2. 你的 tool_t{id,name,arguments} 混了輸入/輸出 → 拆成
//  tool_def_t（送）／tool_call_t（收）。
//  3. 回呼都補 void* user（C 回呼攜狀態的必備；原 str_handler_t
//  沒有＝等於沒有閉包）。
//  4. 未設旗標：model/endpoint/api_key 是指標、NULL＝未設不佔 bit，遮罩只管 6
//  個數值取樣欄位；
//     欄位命名 has → field_mask（statx stx_mask 慣例），helper 叫
//     llm_client_set_field/has_field。
//  5. media 三分：輸入 llm_media_in_t／想要的輸出 llm_modality_t／產出
//  llm_media_out_t（種類看 mime、無 kind）。
//  6. C ABI 不配置任何要呼叫端釋放的記憶體：本地媒體檔走 llm_media_in_t 的
//  data/len＋mime 直接傳
//     （impl 內部 base64），故沒有 llm_free、也沒有配置器。
//  7. 非同步/取消：llm_ask 維持「阻塞」（在呼叫端 thread 跑完、handler 也在該
//  thread 觸發），
//     但收一個可選的 llm_context_t*——trivial（兩個 int）、呼叫端自持免
//     new/free，另一 thread 用 llm_cancel()/llm_phase()
//     戳它取消、讀階段（原子性封在函數裡）。傳 NULL＝退回零負擔呼叫。
//  8. llm_ask 回傳從裸 int →
//  llm_status_t（OK/ERROR/CANCELLED），讓「被取消」跟「出錯」分得開。
//  9. stream 與 tools 正交：stream 對 text/schema/media 一直逐段有效，不因
//  tools 存在而關；
//     tools 的唯一差別是——tool_calls 永遠「拼完整才交給
//     on_tool」（一次性，arguments 不逐段吐）。 schema 串流半截 JSON
//     的風險由呼叫端自負（對得上 OpenAI：結構化輸出本就能串流）。

#include <cstddef>

// ⚠ 落地成真正跨語言的 .so 時，這一整段要換成 `#ifdef __cplusplus extern "C" {`
// 包起、去掉 namespace
//    （C 沒有 namespace，符號不能被 mangle）；並把 <cstddef> 換成
//    <stddef.h>。這裡用 namespace llm_cabi 純為討論方便，好讓 advice_cpp.hpp 的
//    facade 直接引用同型別。
namespace llm_cabi {

// 取樣屬性「哪些有設」的位旗標（bit 值，可 | 起來）。
// ── 你原稿問「has 命名會不會太簡短、混淆」——會。裸 `has`
// 當結構欄位讀起來像動詞、語意空。
//    改法沿用 statx 的 `stx_mask` + `STATX_*` 慣例：欄位名
//    field_mask（名詞、明確），旗標 LLM_FIELD_*。 （另一常見流派＝protobuf-C
//    的「每欄位一個 has_temperature:1 位欄」，最直白但欄位變多、不用遮罩；
//      這裡取遮罩版，因欄位不多且呼叫端能一句 field_mask = A|B 設好。）
enum {
  LLM_FIELD_TEMPERATURE = 1u << 0,
  LLM_FIELD_TOP_P = 1u << 1,
  LLM_FIELD_PRESENCE_PENALTY = 1u << 2,
  LLM_FIELD_FREQUENCY_PENALTY = 1u << 3,
  LLM_FIELD_MAX_TOKENS = 1u << 4,
  LLM_FIELD_SEED = 1u << 5,
};

// 呼叫端設定：連線 + 取樣。數值取樣欄位是否生效看 field_mask（用 LLM_FIELD_*
// 標）。
typedef struct llm_client_t {
  const char
      *endpoint; // NULL＝用內建預設 http://localhost:1234/v1/chat/completions
  const char *api_key; // NULL／""＝不送 Authorization: Bearer
  const char *model;   // NULL＝不送 model（本地後端＝用已載入模型）
  long timeout_ms;     // 0＝不設逾時
  unsigned field_mask; // 哪些數值取樣欄位有設（LLM_FIELD_* 的 OR）
  float temperature, top_p, presence_penalty, frequency_penalty;
  int max_tokens, seed;
} llm_client_t;

// field_mask 的設/查（呼叫端也可直接 c->field_mask = LLM_FIELD_TEMPERATURE |
// LLM_FIELD_SEED）。
static inline void llm_client_set_field(llm_client_t *c, unsigned field,
                                        int on) {
  if (on)
    c->field_mask |= field;
  else
    c->field_mask &= ~field;
}
static inline int llm_client_has_field(const llm_client_t *c, unsigned field) {
  return (c->field_mask & field) != 0;
}

// 工具「定義」（你送出去給模型看的）。
typedef struct llm_tool_def_t {
  const char *name;
  const char *description;
  const char *parameters; // 參數的 JSON Schema（物件字串）
} llm_tool_def_t;

// 工具「呼叫」（模型回來要你執行的）。
typedef struct llm_tool_call_t {
  const char *id;
  const char *name;
  const char *arguments; // 模型產生的 arguments（JSON 字串）
} llm_tool_call_t;

// 結構化輸出的 schema。
typedef struct llm_schema_t {
  const char *name; // NULL＝"response"
  const char *json; // JSON Schema「物件」字串
} llm_schema_t;

// 三個角色三個型（你點出的關鍵：別讓「輸入媒體／想要的輸出／產出媒體」共用一個名）。
// 種類一律從 mime 看（image/png、audio/wav…），不設 kind。

// (a) 輸入媒體＝請求的輸入內容（圖／音給模型看）。url 或 data 二選一（都給時
// url 優先）：
//     · url ＝"https://…"（後端自己抓）或 "data:<mime>;base64,…"（已內嵌的 data
//     URI）。 · data/len ＝直接給原始位元組（本地檔讀進來就傳，免自己
//     base64；impl 內部轉 data URI）。
typedef struct llm_media_in_t {
  const char *url;  // http url 或 data URI
  const char *mime; // 種類（走 data URI 時已內含可 NULL；走 data 時必帶）
  const char *data; // 或直接給原始位元組（免先 base64）
  size_t len;       // data 的位元組長度（走 url 時為 0）
} llm_media_in_t;

// (c)
// 想要的輸出模態＝請求的「指令」（不是媒體！）——要模型產哪些模態、各帶什麼生成參數。
//     ★泛化 audio/image/video：name＝"text"/"audio"/"image"/"video"，
//       config＝該模態生成參數的 JSON 物件（NULL/""＝默認）。
//     例：audio → {"voice":"alloy","format":"wav"}；image →
//     {"size":"1024x1024"}。 模態名就在 name 欄，故不另設 modalities 陣列。
typedef struct llm_modality_t {
  const char *name;
  const char *config; // 該模態生成參數（JSON 物件字串）
} llm_modality_t;

// (b) 產出媒體＝回應的輸出內容（模型生成的 audio…）。純位元組，無 url。
typedef struct llm_media_out_t {
  const char *mime; // 種類
  const char *data; // 原始位元組
  size_t len;
} llm_media_out_t;

// ── 出口回呼（都帶 void* user 以攜狀態）──
// 文字：串流逐段／非串流整段一次。text 非 NUL 結尾保證，長度看 len。回非
// 0＝要求中止串流。
typedef int (*llm_text_handler)(const char *text, size_t len, void *user);
// 工具呼叫：模型每要求一個工具呼一次。回非 0＝要求中止（與 on_text/on_media
// 一致）。
typedef int (*llm_tool_handler)(const llm_tool_call_t *call, void *user);
// 媒體輸出：模型每產出一則媒體（如
// audio）呼一次；media->data/len＝原始位元組。回非 0＝中止。
typedef int (*llm_media_handler)(const llm_media_out_t *media, void *user);
// 錯誤：傳輸失敗／後端回錯時呼叫一次。message 只在回呼期間有效。
typedef void (*llm_error_handler)(const char *message, size_t len, void *user);

// 一次發問的輸入（schema／tools／media／modalities 可任意組合；stream 與 tools
// 正交—— text/schema/media 皆可串流，tool_calls 一律拼完整才交給 on_tool，不受
// stream 影響）。
typedef struct llm_request_t {
  const char *prompt;         // 必填
  const llm_schema_t *schema; // NULL＝不用結構化輸出
  const llm_tool_def_t *tools;
  size_t tools_count;
  const llm_media_in_t *media; // (a) 輸入媒體（圖／音…）
  size_t media_count;
  const llm_modality_t
      *modalities; // (c) 想要的輸出模態＋參數；NULL/0＝只文字（默認）
  size_t modalities_count;
  int stream; // 非 0＝串流（text/schema/media 逐段；tool_calls 仍一次性交付）
} llm_request_t;

// 出口回呼集（任一 handler 可為 NULL）。
typedef struct llm_handlers_t {
  llm_text_handler on_text;
  void *text_user;
  llm_tool_handler on_tool;
  void *tool_user;
  llm_media_handler on_media; // 模型產出的媒體（如 audio）
  void *media_user;
  llm_error_handler on_error;
  void *error_user;
} llm_handlers_t;

// ── 非同步控制（可選）：跨 thread 的取消 + 階段觀測 ──
// llm_ask 本身「阻塞」：在呼叫端這條 thread 跑完整條交換、handler 也在這條
// thread 上被呼叫。 想要非同步，就由呼叫端自己開 thread 跑 llm_ask；另一條
// thread 拿著同一個 ctx 來取消/觀測。 ★ llm_context_t 是 trivial 的（就兩個
// int）：呼叫端自行配置（堆疊/靜態/內嵌皆可）、{0} 或
//   memset 歸零即用，library 不配置任何東西 → 沒有要你 free 的東西。
//   ★ 別直接碰欄位——一律透過 llm_cancel()/llm_phase()
//   存取（正確的原子存取封在那兩個函數裡；
//     裸寫單一 int 在 x86/ARM 實務上也會動＝無數 C 程式的 volatile flag
//     老把戲，但我們走正牌的）。
typedef struct llm_context {
  int cancel; // 0＝進行中；非 0＝已請求取消
  int phase;  // 目前階段，值域＝llm_phase_t（由跑 llm_ask 那條 thread 更新）
} llm_context_t;

// 單一可觀測生命週期：傳輸階段 + 終局狀態。watcher thread 只讀這一個就懂全貌。
typedef enum {
  LLM_PHASE_IDLE,      // 尚未開始
  LLM_PHASE_CONNECT,   // 連線中
  LLM_PHASE_UPLOAD,    // 上傳請求中（body 還在送）
  LLM_PHASE_WAIT,      // 送完、等模型（server 處理中）
  LLM_PHASE_STREAM,    // 接收回應中
  LLM_PHASE_DONE,      // 正常完成
  LLM_PHASE_ERROR,     // 失敗
  LLM_PHASE_CANCELLED, // 被取消
} llm_phase_t;

// 任一 thread 可呼叫：請求取消（下個安全點——curl progress
// callback——會乾淨收掉連線送 FIN）。
void llm_cancel(llm_context_t *ctx);
// 任一 thread 可呼叫：原子讀當前階段。
llm_phase_t llm_phase(const llm_context_t *ctx);

// llm_ask 的回傳：把「被取消」跟「出錯」分開。
typedef enum {
  LLM_CANCELLED = -1, // 被取消（phase 也會落在 LLM_PHASE_CANCELLED）
  LLM_OK = 0,         // 成功
  LLM_ERROR = 1,      // 傳輸/後端錯誤（on_error 已被呼叫）
} llm_status_t;

// ★ 統一發問（阻塞）。ctx 可為 NULL（不需要取消/觀測時，退回零負擔呼叫）。
//   回 LLM_OK＝成功；LLM_ERROR＝失敗（若有 on_error
//   會被呼叫一次帶訊息）；LLM_CANCELLED＝被取消。 ── 對照你原稿的扁平版：`void
//   llm_ask(ctx, client, prompt, text_handler, err_handler, …)`——
//      同樣的資訊，只是把「輸入」聚成 llm_request_t、「出口」聚成
//      llm_handlers_t、「取消/觀測」 聚成 llm_context_t（Go ctx
//      那個味道），長參數列不易讀錯位。要扁平也行。
llm_status_t llm_ask(llm_context_t *ctx, const llm_client_t *client,
                     const llm_request_t *req, const llm_handlers_t *handlers);

// ★ 本 ABI 刻意「不配置任何要呼叫端釋放的記憶體」——所以沒有
// llm_free、也沒有配置器。
//   本地媒體檔不需要專用函式：讀進 buffer，用 llm_media_in_t 的 data/len＋mime
//   直接傳即可 （impl 內部負責 base64 成 data URI）。曾考慮的
//   llm_media_from_file＋llm_alloc_fn 就此不需要。

// ── 關於 llm_context_t 的設計取捨（你問過「模仿 golang？別人寫 C
// 系統程式怎麼做？」）──
//   它就是「取消（cancellation，Go context 的本命）」到位時才引入的那個
//   context——現在有料了： cancel flag + phase。但刻意做成 trivial（兩個
//   int、呼叫端自持），而非 curl 那種不透明 handle
//   （CURL*＋CURLcode＋curl_easy_strerror），因為我們只需要「跨 thread
//   戳一下」，不需要可重用的 連線池/keep-alive。把原子性封進
//   llm_cancel()/llm_phase()，就同時要到「trivial 免 free」和 「跨 thread
//   正確」。等未來真要連線重用，再考慮升級成持有 CURL* 的不透明 handle 不遲。

} // namespace llm_cabi
