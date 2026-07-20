// options.go — 把組好的 client／請求輸入／Sink 收成 binding 的 functional options（cli.cpp 組請求段延伸）。
//
// 連線／取樣欄位逐項對應 flags.go 的反射表 → cllm.Endpoint/Model/Temperature…；system／stream／schema／
// tools／media／modalities 與四個出口回呼一併掛上。真正做事的是 binding 的 cllm.Ask，本檔只搬運。
package main

import "cllm"

// buildOptions：組出一次 cllm.Ask 的 options 切片。
func buildOptions(p *parsedArgs, client map[string]interface{}, r *requestInputs, sink *Sink) []cllm.Option {
	var opts []cllm.Option

	// 連線／取樣欄位（型別在 config／flag 層已正規化為 string／int／float32）。
	if v, ok := client["endpoint"].(string); ok && v != "" {
		opts = append(opts, cllm.Endpoint(v))
	}
	if v, ok := client["api_key"].(string); ok && v != "" {
		opts = append(opts, cllm.APIKey(v))
	}
	if v, ok := client["model"].(string); ok && v != "" {
		opts = append(opts, cllm.Model(v))
	}
	if v, ok := client["timeout_ms"].(int); ok {
		opts = append(opts, cllm.TimeoutMs(v))
	}
	if v, ok := client["temperature"].(float32); ok {
		opts = append(opts, cllm.Temperature(v))
	}
	if v, ok := client["top_p"].(float32); ok {
		opts = append(opts, cllm.TopP(v))
	}
	if v, ok := client["presence_penalty"].(float32); ok {
		opts = append(opts, cllm.PresencePenalty(v))
	}
	if v, ok := client["frequency_penalty"].(float32); ok {
		opts = append(opts, cllm.FrequencyPenalty(v))
	}
	if v, ok := client["max_tokens"].(int); ok {
		opts = append(opts, cllm.MaxTokens(v))
	}
	if v, ok := client["seed"].(int); ok {
		opts = append(opts, cllm.Seed(v))
	}

	// 固定旗標與請求輸入。
	if p.hasSystem {
		opts = append(opts, cllm.System(p.systemText))
	}
	if p.stream {
		opts = append(opts, cllm.Stream())
	}
	if r.hasSchema {
		opts = append(opts, cllm.Schema(r.schemaBody))
	}
	if len(r.toolDefs) > 0 {
		opts = append(opts, cllm.Tools(r.toolDefs...))
	}
	if len(r.media) > 0 {
		opts = append(opts, cllm.MediaIn(r.media...))
	}
	if len(r.modalities) > 0 {
		opts = append(opts, cllm.Modalities(r.modalities...))
	}

	// 四個出口回呼（對齊 Sink）。
	opts = append(opts,
		cllm.OnDelta(sink.OnDelta),
		cllm.OnTool(sink.OnTool),
		cllm.OnMedia(sink.OnMedia),
		cllm.OnError(sink.OnError))
	return opts
}
