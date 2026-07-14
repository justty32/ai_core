// llm_schema.hpp — galtxt try_3：「從 struct 反射生 JSON Schema」的共用件。
//
// 兩個地方要從 C++ struct 反射生 schema 送給模型：
//   · llm_tool  的 make_tool<Args>()  —— 工具呼叫的參數 schema
//   · llm_json  的 ask_as<T>()        —— 結構化輸出的回應 schema
// 兩邊要用**同一組 opts**，否則行為會不一致，故抽在這裡（比照 llm.hpp 抽 post／apply_sampling）。
//
// ★★ 為什麼一定要 error_on_missing_keys（2026-07-14 真後端實測血淚）：
//   `glz::write_json_schema<T>()` 用**預設** opts 時，生出的 schema 只有 `properties` 和
//   `additionalProperties:false`，**沒有 `required`**。JSON Schema 語意下，沒列進 required 的欄位
//   一律是**選填**——於是後端的約束解碼只保證「不吐多餘的鍵」，**不保證「每個欄位都吐」**。
//   實測 LM Studio + gemma 對 `struct Character{name,affection,lines}`，**只回了 `{"name":…}`**，
//   另兩個欄位整個省略掉。
//
//   glaze 其實有這個能力，只是要開關（見其 core/common.hpp 的 `requires_key`）：
//     「開了 error_on_missing_keys、且該成員非 nullable」⇒ 該成員列入 required。
//   所以用 kSchemaOpts 生 schema，required 就自動長出來，而且**語意是對的**——
//   `std::optional<…>` 這種本來就該選填的欄位會被正確地**排除**在 required 之外
//   （實測 `struct WithOpt{name; optional<string> nickname; age;}` → `required:["name","age"]`，
//     nickname 的型別還自動變成 `["string","null"]`）。**別自己手動把 properties 全塞進 required**，
//     那樣會把真正選填的欄位也錯標成必填。
//
//   ⚠ 這種缺陷**離線 fixture 驗不出來**——假回應是我們自己寫的，欄位當然齊。只有打真後端才會現形。

#pragma once
#include <string>

#include <glaze/glaze.hpp>

namespace llm {

// 生 schema 專用的 opts：error_on_missing_keys 開著，required 才會生出來（理由見檔頭）。
// ⚠ 只用於「生 schema」這件事；解析回應時用各自的 opts（那邊多半要 error_on_unknown_keys=false）。
inline constexpr glz::opts kSchemaOpts{ .error_on_missing_keys = true };

// 從 struct T 反射生 JSON Schema 字串。生成失敗回 "{}"（空 schema，等同不約束）。
template <class T>
std::string schema_of() {
    auto schema = glz::write_json_schema<T, kSchemaOpts>();
    return schema ? std::move(*schema) : std::string("{}");
}

}  // namespace llm
