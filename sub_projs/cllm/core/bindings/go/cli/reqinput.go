// reqinput.go — 把 CLI 的請求類旗標驗證／組成 cllm.Ask 的請求輸入（cli.cpp 組請求段）。
//
// ⚠ 與 C++ llm 刻意分歧（比照 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON 文字」（同
// --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
// --image/--media 的三分流取值在 media.go（buildMediaItem）。本檔把這些旗標收成 schema／tools／
// modalities／media 四份輸入，並驗 --media-out 目錄。
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"

	"cllm"
)

// requestInputs＝cllm.Ask 的請求輸入四件組。
type requestInputs struct {
	schemaBody string
	hasSchema  bool
	toolDefs   []cllm.ToolDef
	modalities []cllm.Modality
	media      []cllm.Media
}

// toolSpec＝--tool 的字面 JSON 形狀（parameters 原樣保留為 RawMessage，再轉字串交內核）。
type toolSpec struct {
	Name        string          `json:"name"`
	Description string          `json:"description"`
	Parameters  json.RawMessage `json:"parameters"`
}

// buildRequestInputs 從 parsedArgs 組請求輸入，回 (inputs, exit, done)；done=true＝驗證失敗、以 exit 返回。
func buildRequestInputs(p *parsedArgs) (*requestInputs, int, bool) {
	r := &requestInputs{}

	if p.hasSchema {
		if !json.Valid([]byte(p.schemaText)) { // 只驗合法；字面原樣交內核嵌入
			fmt.Fprintln(os.Stderr, "--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）")
			return nil, exitUsage, true
		}
		r.schemaBody, r.hasSchema = p.schemaText, true
	}

	for _, spec := range p.toolSpecs { // 字面 tool def JSON；需 name 與非空 parameters
		var td toolSpec
		if err := json.Unmarshal([]byte(spec), &td); err != nil {
			fmt.Fprintln(os.Stderr, "--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）")
			return nil, exitUsage, true
		}
		if td.Name == "" || len(td.Parameters) == 0 {
			fmt.Fprintln(os.Stderr, "--tool 缺 name 或 parameters")
			return nil, exitUsage, true
		}
		r.toolDefs = append(r.toolDefs, cllm.ToolDef{
			Name: td.Name, Description: td.Description, Parameters: string(td.Parameters)})
	}

	for _, spec := range p.modalitySpecs { // 「名」或「名=<字面JSON>」
		name, cfg, eq := strings.Cut(spec, "=")
		if name == "" {
			fmt.Fprintln(os.Stderr, "--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）")
			return nil, exitUsage, true
		}
		if eq && !json.Valid([]byte(cfg)) {
			fmt.Fprintln(os.Stderr, "--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）")
			return nil, exitUsage, true
		}
		m := cllm.Modality{Name: name}
		if eq {
			m.Config = cfg
		}
		r.modalities = append(r.modalities, m)
	}

	for _, spec := range p.mediaSpecs { // 三分流（見 media.go）
		item, ok := buildMediaItem(spec)
		if !ok {
			return nil, exitUsage, true
		}
		r.media = append(r.media, item)
	}

	if p.mediaOutDir != "" {
		if info, err := os.Stat(p.mediaOutDir); err != nil || !info.IsDir() {
			fmt.Fprintf(os.Stderr, "--media-out 不是可用目錄：%s\n", p.mediaOutDir)
			return nil, exitUsage, true
		}
	}
	return r, exitOK, false
}
