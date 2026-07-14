// llm.hpp — galtxt try_3：純 C++ 的 LLM「ask 接口」（介面）。
//
// 這條線接口設計的根本主張：**struct＋glaze 編譯期反射＝唯一真相源，不搬 schema 表**。
// try_1/try_2 用 schema 表，是動態語言缺靜態反射的補償拐杖；C++ 直接讓 struct 當契約——
// 送出的請求、收回的回應都是 struct，glaze 反射自動 JSON↔struct。要多一個取樣旋鈕，
// 就在這裡加一個欄位（送/收兩邊自動跟上），不必改任何映射表。
//
// 傳輸走 http 模組（request／stream）。離線可測：endpoint 給 file:// 走 http 的讀檔特例。
// 命名沿用跨三線的 llm/ask 概念詞（try_2 拍板的 ask 縫），型別循 http 模組的 namespace 慣例。

#pragma once
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace llm {

// 串流回呼：每收到一段內容 delta 呼一次；回 false 可中止串流。
// 用 string_view：delta 只在回呼期間有效（記憶體屬串流緩衝），傳視圖＝零複製零配置，
// 且與 http::OnData 一致。回呼若要留存內容，自行複製成 std::string。
using OnDelta = std::function<bool(std::string_view)>;

// 一個設定好的 LLM 呼叫端：持 endpoint＋取樣設定，ask() 發問。
class Client {
public:
    // ── 連線 ──
    std::string endpoint;                 // 後端 URL（真後端 https://…/chat/completions；離線 file://…）
    std::string api_key;                  // 選填：非空則送 Authorization: Bearer <key>
    long timeout_ms = 0;                  // 0＝不設逾時（file:// 離線 fixture 不受影響）；
                                           // from_env() 給真後端一個寬鬆預設（reasoning 模型會慢）

    // ── 取樣屬性（全部選填，未設＝不送、交給後端用它自己的默認）──
    //    用 std::optional：nullopt 時 glaze 自動略過該鍵（skip_null_members 預設開）。
    //    哲學：struct＝唯一真相源，使用者沒設的就別替後端做主。
    //    ⚠ model 例外：本地伺服器（LM Studio／llama.cpp）不送＝用已載入的模型；
    //      但 OpenAI 雲端 API 把 model 列為必填，接雲端時要設。
    std::optional<std::string> model;       // 模型名
    std::optional<float> temperature;       // 越高越發散（不送＝後端默認，OpenAI 為 1.0）
    std::optional<float> top_p;             // 核取樣（不送＝後端默認，OpenAI 為 1.0）
    std::optional<int>   max_tokens;        // 生成上限 token 數
    std::optional<float> presence_penalty;  // 存在懲罰（-2.0 ~ 2.0）：壓已出現的主題
    std::optional<float> frequency_penalty; // 頻率懲罰（-2.0 ~ 2.0）：壓重複用詞
    std::optional<int>   seed;              // 隨機種子：固定可重現輸出

    // prompt＝使用者提問。
    // stream=true 時，內容逐段經 on_delta 餵出（回 false 可中止）；
    // 兩種模式都回「完整組合後的模型答覆文字」。
    // ★ 預設參數必須靠後：on_delta 也給預設（nullptr），才能讓 ask(prompt) 只帶一參。
    std::string ask(std::string_view prompt,
                    bool stream = false,
                    OnDelta on_delta = nullptr);
};

// 從環境變數建一個打「真後端」用的 Client（對齊 try_1/try_2 兩條線既有慣例，三線同名同預設）：
//   AI_CORE_LLM_BASE_URL  預設 http://localhost:1234/v1（LM Studio 本機預設埠；到 /v1 為止，
//                         這裡接上 /chat/completions 組成 endpoint）
//   AI_CORE_LLM_MODEL     預設 local-model（LM Studio 認得這名字，會用「目前已載入」的模型）
//   AI_CORE_LLM_API_KEY   預設空（有值才送 Authorization: Bearer；本機通常留空）
// endpoint 事後仍可手動覆寫（離線 fixture 走 file:// 就是直接另外建 Client、不經過這個工廠）。
// timeout_ms 給一個寬鬆預設：真後端（尤其 reasoning 模型）單次可能要等數十秒。
Client from_env();

// ── 供擴充模組（llm_tool／llm_media）共用的兩塊，讓它們不重造傳輸與取樣搬運 ──

// 把已組好的請求 JSON（非串流）送到 client.endpoint（帶 api_key），回原始回應本體字串。
// 傳輸失敗會 throw。回傳原始 body，讓呼叫端各自用自己的 struct 反射解析。
std::string post(const Client& client, std::string_view request_json);

// 把 client 的取樣屬性（model/temperature/… 全 optional）灌進任意「具同名欄位」的請求
// struct（Body 需有那些欄位）。DRY：不在每個模組重抄一遍那七行 optional 搬運。
template <class Body>
void apply_sampling(const Client& client, Body& body) {
    body.model = client.model;
    body.temperature = client.temperature;
    body.top_p = client.top_p;
    body.max_tokens = client.max_tokens;
    body.presence_penalty = client.presence_penalty;
    body.frequency_penalty = client.frequency_penalty;
    body.seed = client.seed;
}

}  // namespace llm
