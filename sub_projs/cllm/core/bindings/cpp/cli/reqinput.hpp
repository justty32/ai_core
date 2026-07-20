#pragma once
// reqinput.hpp — 把 CLI 的請求類旗標驗證／組成 abi::Request 的欄位（對齊 core-py 的 reqinput.py／
//   cli.cpp 組請求段）。
//
// ⚠ 與原生 C++ `llm` 刻意分歧（比照 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON
// 文字」（同 --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯。
// --image/--media 的三分流取值在 media.hpp。本檔把這些旗標收成 schema／tools／modalities／media
// 四份輸入，並驗 --media-out 目錄。

#include <optional>
#include <vector>

#include <cllm/llm.hpp> // llm::abi::Schema / ToolDef / MediaIn / Modality

#include "argv.hpp"

namespace cli::reqinput {

// core.ask 的請求輸入四件組（schema／tools／modalities／media）。
struct RequestInputs {
  std::optional<llm::abi::Schema> schema;
  std::vector<llm::abi::ToolDef> tools;
  std::vector<llm::abi::MediaIn> media;
  std::vector<llm::abi::Modality> modalities;
};

// 從 ParsedArgs 組請求輸入，回 kExitOk；驗證失敗印 stderr 回用法錯碼。
int build_request_inputs(const argv::ParsedArgs &p, RequestInputs &out);

} // namespace cli::reqinput
