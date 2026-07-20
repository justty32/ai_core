/* bridge.h — Go binding 的 C 側橋接（宣告 only；定義在 bridge.c）。
 *
 * cgo 不能把 Go func 直接塞進 C 函數指標欄，且「含 //export 的 .go 檔前導區只能有宣告」——
 * 故把「拿 Go export 函數的位址、填進 llm_handlers_t」這個定義搬到 bridge.c，這裡只宣告它。
 * cllmGoOnText/cllmGoOnError 的宣告由 cgo 生成的 _cgo_export.h 提供（bridge.c 引它），
 * 不在這裡自己宣告——否則 const 修飾等簽章對不上會衝突。 */
#pragma once
#include <stdint.h>
#include <cllm/cabi.h>

/* 把 Go export 的 handler 掛進 handlers；user 走 uintptr（cgo.Handle 的值）以 void* 不透明往返。*/
void cllm_fill_handlers(llm_handlers_t *h, uintptr_t user);
