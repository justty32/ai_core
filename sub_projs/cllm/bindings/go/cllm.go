// Package cllm — cllm 的 Go binding：用 cgo 呼叫 libcllm 的 C ABI（唯一入口 llm_ask）。
//
// API 用 Go 慣用的 functional options，語意對齊其他語言（ask + on_delta 串流回呼）：
//
//	cllm.Ask("你好")                                        // 只給 prompt（走內建 localhost）
//	cllm.Ask("你好", cllm.Endpoint("http://…/chat/completions"))
//	cllm.Ask("你好", cllm.Model("local-model"), cllm.Temperature(0.7))
//	cllm.Ask("數到五", cllm.Stream(),                       // 串流：逐段進 OnDelta
//	    cllm.OnDelta(func(p string) bool { fmt.Print(p); return false }))  // 回 true 可中止
//
// 回「完整組合後的答案字串」；失敗回 ("", *Error)。取消（OnDelta 回 true）回已收到的部分, nil。
//
// 建置靠 pkg-config 找 libcllm（先 source ~/repo/dev/env.sh，或設 PKG_CONFIG_PATH 指向 cllm.pc）。
// 範圍（v1，刻意小）：prompt＋連線/取樣選項＋stream＋schema＋OnDelta/OnError；tools／media 未收。
package cllm

/*
#cgo pkg-config: cllm
#include <stdlib.h>
#include "bridge.h"
*/
import "C"

import (
	"runtime"
	"runtime/cgo"
	"strings"
	"unsafe"
)

// Error＝傳輸/後端錯誤（llm_ask 回 LLM_ERROR）。
type Error struct{ Msg string }

func (e *Error) Error() string { return "cllm: " + e.Msg }

type config struct {
	endpoint, apiKey, model, schema        string
	timeoutMs                              int
	temperature, topP, presence, frequency *float32
	maxTokens, seed                        *int
	stream                                 bool
	onDelta                                func(string) bool
	onError                                func(string)
}

// Option 設定 Ask（未設就不送、交後端默認）。
type Option func(*config)

func Endpoint(s string) Option           { return func(c *config) { c.endpoint = s } }
func APIKey(s string) Option             { return func(c *config) { c.apiKey = s } }
func Model(s string) Option              { return func(c *config) { c.model = s } }
func TimeoutMs(ms int) Option            { return func(c *config) { c.timeoutMs = ms } }
func Temperature(f float32) Option       { return func(c *config) { c.temperature = &f } }
func TopP(f float32) Option              { return func(c *config) { c.topP = &f } }
func PresencePenalty(f float32) Option   { return func(c *config) { c.presence = &f } }
func FrequencyPenalty(f float32) Option  { return func(c *config) { c.frequency = &f } }
func MaxTokens(n int) Option             { return func(c *config) { c.maxTokens = &n } }
func Seed(n int) Option                  { return func(c *config) { c.seed = &n } }
func Stream() Option                     { return func(c *config) { c.stream = true } }
func Schema(json string) Option          { return func(c *config) { c.schema = json } }
func OnDelta(fn func(string) bool) Option { return func(c *config) { c.onDelta = fn } } // 回 true 中止
func OnError(fn func(string)) Option      { return func(c *config) { c.onError = fn } }

type askState struct {
	acc     strings.Builder
	onDelta func(string) bool
	onError func(string)
	errMsg  string
}

//export cllmGoOnText
func cllmGoOnText(t *C.char, n C.size_t, user unsafe.Pointer) C.int {
	st := cgo.Handle(uintptr(user)).Value().(*askState)
	chunk := C.GoStringN(t, C.int(n))
	st.acc.WriteString(chunk)
	if st.onDelta != nil && st.onDelta(chunk) {
		return 1 // 回 true → 中止串流
	}
	return 0
}

//export cllmGoOnError
func cllmGoOnError(m *C.char, n C.size_t, user unsafe.Pointer) {
	st := cgo.Handle(uintptr(user)).Value().(*askState)
	st.errMsg = C.GoStringN(m, C.int(n))
	if st.onError != nil {
		st.onError(st.errMsg)
	}
}

// Ask 問 LLM，回完整答案字串。詳見套件說明。
func Ask(prompt string, opts ...Option) (string, error) {
	var cfg config
	for _, o := range opts {
		o(&cfg)
	}

	var c C.llm_client_t
	var freed []unsafe.Pointer
	cstr := func(s string) *C.char {
		p := C.CString(s)
		freed = append(freed, unsafe.Pointer(p))
		return p
	}
	defer func() {
		for _, p := range freed {
			C.free(p)
		}
	}()

	if cfg.endpoint != "" {
		c.endpoint = cstr(cfg.endpoint)
	}
	if cfg.apiKey != "" {
		c.api_key = cstr(cfg.apiKey)
	}
	if cfg.model != "" {
		c.model = cstr(cfg.model)
	}
	c.timeout_ms = C.long(cfg.timeoutMs)

	var mask C.uint
	if cfg.temperature != nil {
		c.temperature = C.float(*cfg.temperature)
		mask |= C.uint(C.LLM_FIELD_TEMPERATURE)
	}
	if cfg.topP != nil {
		c.top_p = C.float(*cfg.topP)
		mask |= C.uint(C.LLM_FIELD_TOP_P)
	}
	if cfg.presence != nil {
		c.presence_penalty = C.float(*cfg.presence)
		mask |= C.uint(C.LLM_FIELD_PRESENCE_PENALTY)
	}
	if cfg.frequency != nil {
		c.frequency_penalty = C.float(*cfg.frequency)
		mask |= C.uint(C.LLM_FIELD_FREQUENCY_PENALTY)
	}
	if cfg.maxTokens != nil {
		c.max_tokens = C.int(*cfg.maxTokens)
		mask |= C.uint(C.LLM_FIELD_MAX_TOKENS)
	}
	if cfg.seed != nil {
		c.seed = C.int(*cfg.seed)
		mask |= C.uint(C.LLM_FIELD_SEED)
	}
	c.field_mask = mask

	var r C.llm_request_t
	r.prompt = cstr(prompt)
	if cfg.stream {
		r.stream = 1
	}
	// r.schema 指向 sc（Go 記憶體）＝「傳給 C 的 struct 內嵌 Go 指標」，Go 1.21+ 須 pin。
	var pinner runtime.Pinner
	defer pinner.Unpin()
	var sc C.llm_schema_t
	if cfg.schema != "" {
		sc.name = cstr("response")
		sc.json = cstr(cfg.schema)
		pinner.Pin(&sc)
		r.schema = &sc
	}

	st := &askState{onDelta: cfg.onDelta, onError: cfg.onError}
	h := cgo.NewHandle(st)
	defer h.Delete()
	var handlers C.llm_handlers_t
	C.cllm_fill_handlers(&handlers, C.uintptr_t(h))

	switch C.llm_ask(nil, &c, &r, &handlers) {
	case C.LLM_OK, C.LLM_CANCELLED: // 取消（OnDelta 回 true）→ 回已收到的部分
		return st.acc.String(), nil
	default:
		return "", &Error{Msg: st.errMsg}
	}
}
