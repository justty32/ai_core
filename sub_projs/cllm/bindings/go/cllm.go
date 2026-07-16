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
// 也支援 tools／media／modalities（對齊 C ABI 的 llm_tool_def_t／llm_media_in_t／llm_modality_t）：
//
//	cllm.Ask("東京天氣如何？",
//	    cllm.Tools(cllm.ToolDef{Name: "get_weather", Description: "查天氣", Parameters: `{"type":"object"}`}),
//	    cllm.OnTool(func(tc cllm.ToolCall) bool { fmt.Println(tc.Name, tc.Arguments); return false }))
//	cllm.Ask("說句話", cllm.OnMedia(func(m cllm.MediaOut) bool { fmt.Println(m.Mime, len(m.Bytes)); return false }))
//	cllm.Ask("描述這張圖",
//	    cllm.MediaIn(cllm.Media{URL: "data:image/png;base64,…"}),
//	    cllm.Modalities(cllm.Modality{Name: "audio", Config: `{"voice":"alloy"}`}))
//
// 建置靠 pkg-config 找 libcllm（先 source ~/dev/env.sh，或設 PKG_CONFIG_PATH 指向 cllm.pc）。
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
	tools                                  []ToolDef
	onTool                                 func(ToolCall) bool
	media                                  []Media
	modalities                             []Modality
	onMedia                                func(MediaOut) bool
}

// Option 設定 Ask（未設就不送、交後端默認）。
type Option func(*config)

func Endpoint(s string) Option            { return func(c *config) { c.endpoint = s } }
func APIKey(s string) Option              { return func(c *config) { c.apiKey = s } }
func Model(s string) Option               { return func(c *config) { c.model = s } }
func TimeoutMs(ms int) Option             { return func(c *config) { c.timeoutMs = ms } }
func Temperature(f float32) Option        { return func(c *config) { c.temperature = &f } }
func TopP(f float32) Option               { return func(c *config) { c.topP = &f } }
func PresencePenalty(f float32) Option    { return func(c *config) { c.presence = &f } }
func FrequencyPenalty(f float32) Option   { return func(c *config) { c.frequency = &f } }
func MaxTokens(n int) Option              { return func(c *config) { c.maxTokens = &n } }
func Seed(n int) Option                   { return func(c *config) { c.seed = &n } }
func Stream() Option                      { return func(c *config) { c.stream = true } }
func Schema(json string) Option           { return func(c *config) { c.schema = json } }
func OnDelta(fn func(string) bool) Option { return func(c *config) { c.onDelta = fn } } // 回 true 中止
func OnError(fn func(string)) Option      { return func(c *config) { c.onError = fn } }

// ToolDef＝工具「定義」（送給模型看的：名稱＋描述＋參數 JSON Schema 物件字串）。
type ToolDef struct{ Name, Description, Parameters string }

// Tools 掛工具定義（可重複呼叫疊加；配合 OnTool 收模型回來的工具呼叫）。
func Tools(defs ...ToolDef) Option {
	return func(c *config) { c.tools = append(c.tools, defs...) }
}

// ToolCall＝模型回來要求執行的工具呼叫（id／name／模型產生的 arguments JSON 字串）。
type ToolCall struct{ ID, Name, Arguments string }

// OnTool 收模型的工具呼叫（tool_calls 一律拼完整才交付，不受 Stream 影響）。回 true＝要求中止。
func OnTool(fn func(ToolCall) bool) Option { return func(c *config) { c.onTool = fn } }

// Media＝輸入媒體（圖／音給模型看）。URL 可為 http(s) URL 或 data URI；
// 未給 URL、給 Bytes 時走原始位元組（Mime 必帶）。都給時 URL 優先。
type Media struct {
	URL, Mime string
	Bytes     []byte
}

// MediaIn 掛輸入媒體（可重複呼叫疊加）。
func MediaIn(items ...Media) Option {
	return func(c *config) { c.media = append(c.media, items...) }
}

// Modality＝想要的輸出模態＋生成參數（如 {"name":"audio","config":`{"voice":"alloy"}`}）。
type Modality struct{ Name, Config string }

// Modalities 掛想要的輸出模態（NULL/未掛＝只文字）。
func Modalities(ms ...Modality) Option {
	return func(c *config) { c.modalities = append(c.modalities, ms...) }
}

// MediaOut＝模型產出的媒體（如 audio），純位元組（無 url，種類看 Mime）。
type MediaOut struct {
	Mime  string
	Bytes []byte
}

// OnMedia 收模型產出的媒體（模型每產一則呼一次）。回 true＝要求中止。
func OnMedia(fn func(MediaOut) bool) Option { return func(c *config) { c.onMedia = fn } }

type askState struct {
	acc     strings.Builder
	onDelta func(string) bool
	onError func(string)
	onTool  func(ToolCall) bool
	onMedia func(MediaOut) bool
	errMsg  string
}

// goStr：C 字串轉 Go 字串，容忍 NULL（tool_call／media_out 的欄位在 impl 端保證 NUL 結尾，
// 但 mime 等選填欄位可能是 NULL——C.GoString 對 NULL 會炸，這裡先擋）。
func goStr(p *C.char) string {
	if p == nil {
		return ""
	}
	return C.GoString(p)
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

//export cllmGoOnTool
func cllmGoOnTool(call *C.llm_tool_call_t, user unsafe.Pointer) C.int {
	st := cgo.Handle(uintptr(user)).Value().(*askState)
	if st.onTool == nil {
		return 0
	}
	tc := ToolCall{ID: goStr(call.id), Name: goStr(call.name), Arguments: goStr(call.arguments)}
	if st.onTool(tc) {
		return 1 // 回 true → 中止
	}
	return 0
}

//export cllmGoOnMedia
func cllmGoOnMedia(media *C.llm_media_out_t, user unsafe.Pointer) C.int {
	st := cgo.Handle(uintptr(user)).Value().(*askState)
	if st.onMedia == nil {
		return 0
	}
	mo := MediaOut{Mime: goStr(media.mime), Bytes: C.GoBytes(unsafe.Pointer(media.data), C.int(media.len))}
	if st.onMedia(mo) {
		return 1
	}
	return 0
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

	// 同理：tools／media／modalities 陣列也是「傳給 C 的 struct（r）內嵌 Go 指標」，須 pin。
	if n := len(cfg.tools); n > 0 {
		defs := make([]C.llm_tool_def_t, n)
		for i, t := range cfg.tools {
			defs[i].name = cstr(t.Name)
			defs[i].description = cstr(t.Description)
			defs[i].parameters = cstr(t.Parameters)
		}
		pinner.Pin(&defs[0])
		r.tools = &defs[0]
		r.tools_count = C.size_t(n)
	}
	if n := len(cfg.media); n > 0 {
		items := make([]C.llm_media_in_t, n)
		for i, m := range cfg.media {
			switch {
			case m.URL != "":
				items[i].url = cstr(m.URL)
				if m.Mime != "" {
					items[i].mime = cstr(m.Mime)
				}
			case len(m.Bytes) > 0:
				items[i].mime = cstr(m.Mime)
				p := C.CBytes(m.Bytes)
				freed = append(freed, p)
				items[i].data = (*C.char)(p)
				items[i].len = C.size_t(len(m.Bytes))
			}
		}
		pinner.Pin(&items[0])
		r.media = &items[0]
		r.media_count = C.size_t(n)
	}
	if n := len(cfg.modalities); n > 0 {
		mods := make([]C.llm_modality_t, n)
		for i, m := range cfg.modalities {
			mods[i].name = cstr(m.Name)
			if m.Config != "" {
				mods[i].config = cstr(m.Config)
			}
		}
		pinner.Pin(&mods[0])
		r.modalities = &mods[0]
		r.modalities_count = C.size_t(n)
	}

	st := &askState{onDelta: cfg.onDelta, onError: cfg.onError, onTool: cfg.onTool, onMedia: cfg.onMedia}
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
