// output.go — cllm.Ask 的四個出口回呼打包成 Sink（對齊 core/src/cli_output.cpp 的 Sink）。
//
// 把「輸出接線 + 其共享狀態」收成一個物件：文字吐 stdout、tool_calls 一行一則 JSON、產出媒體落檔
// --media-out、錯誤吐 stderr。回呼一律回 false（不主動中止）。收尾狀態（WroteText/HadError/MediaErr）
// 由 cli.go 讀來定退出碼。
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"

	"cllm"
)

// Sink＝cllm.Ask 的 handlers 與其共享狀態。把 sink.On* 綁定方法傳給對應回呼選項。
type Sink struct {
	mediaOutDir string
	WroteText   bool // 有無吐過文字（決定要不要補尾端換行）
	HadError    bool // OnError 被呼叫過＝請求真失敗
	MediaErr    bool // 媒體落檔失敗＝結果真掉了
	mediaN      int  // 已落檔媒體數（供編號檔名）
}

func newSink(mediaOutDir string) *Sink { return &Sink{mediaOutDir: mediaOutDir} }

// OnDelta：文字段吐 stdout（非串流時內核也會一次性呼此吐全文，對齊 core-py）。
func (s *Sink) OnDelta(piece string) bool {
	fmt.Print(piece)
	s.WroteText = true
	return false
}

// toolLine＝tool_calls 一行一則 JSON：{"tool","id","arguments"}（arguments 原樣內嵌為物件）。
type toolLine struct {
	Tool string          `json:"tool"`
	ID   string          `json:"id"`
	Args json.RawMessage `json:"arguments"`
}

// OnTool：模型的工具呼叫 → 一行一則 JSON 吐 stdout。
func (s *Sink) OnTool(call cllm.ToolCall) bool {
	args := call.Arguments
	if args == "" {
		args = "{}"
	}
	if !json.Valid([]byte(args)) { // 後端理應保證 JSON；壞了就當字串包起來
		b, _ := json.Marshal(args)
		args = string(b)
	}
	line := toolLine{Tool: call.Name, ID: call.ID, Args: json.RawMessage(args)}
	b, _ := json.Marshal(line)
	fmt.Println(string(b))
	return false
}

// OnMedia：模型產出媒體 → 落檔 --media-out（沒給即丟棄並警告）。
func (s *Sink) OnMedia(m cllm.MediaOut) bool {
	if s.mediaOutDir == "" {
		fmt.Fprintf(os.Stderr, "收到產出媒體（%s，%d bytes）但沒給 --media-out，已丟棄\n", m.Mime, len(m.Bytes))
		return false
	}
	s.mediaN++
	path := filepath.Join(s.mediaOutDir, fmt.Sprintf("llm-media-%d.%s", s.mediaN, extOf(m.Mime)))
	if err := os.WriteFile(path, m.Bytes, 0o644); err != nil {
		fmt.Fprintf(os.Stderr, "媒體落檔失敗：%s\n", path)
		s.MediaErr = true
		return false
	}
	fmt.Println(path)
	return false
}

// OnError：請求失敗訊息吐 stderr，並記下 HadError。
func (s *Sink) OnError(msg string) {
	s.HadError = true
	fmt.Fprintf(os.Stderr, "請求失敗：%s\n", msg)
}
