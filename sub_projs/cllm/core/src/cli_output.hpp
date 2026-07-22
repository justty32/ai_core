#pragma once
// cli_output.hpp — `llm` CLI 的出口面（unix filter 的「吐」那半）。
// 文字逐段吐 stdout；tool_calls 一行一則 JSON 吐 stdout（jq 友善）；產出媒體落檔到
// media_out_dir（路徑逐行吐 stdout；空＝明說丟棄）；錯誤帶「請求失敗：」前綴吐 stderr；
// --usage 時 token 用量帶「用量：」前綴吐 stderr（診斷面，不汙染 stdout）。
// orchestrator（cli.cpp）只拿 handlers() 去 ask、事後讀狀態旗標定退出碼。

#include <string>

#include "cabi.hpp" // llm::abi::Handlers

namespace cli::output {

// 一次發問的出口狀態＋回呼工廠。Sink 的存活期須蓋過 ask（handlers 以參考捕捉 this）。
struct Sink {
  std::string media_out_dir; // --media-out；空＝收到媒體時明說丟棄（不無聲吞、也不驚喜寫檔）
  bool show_usage = false;   // --usage；true＝裝 on_usage、用量吐 stderr（串流會多送 stream_options）
  bool wrote_text = false;   // 吐過文字＝成功收尾補換行（tool/media 行自帶換行，不算）
  bool media_err = false;    // 媒體落檔失敗＝資料真掉了（orchestrator 收尾轉退出碼，不吞）
  int media_n = 0;           // 落檔序號（llm-media-<n>.<ext>）

  llm::abi::Handlers handlers(); // 綁定到本 Sink 的出口回呼集
};

} // namespace cli::output
