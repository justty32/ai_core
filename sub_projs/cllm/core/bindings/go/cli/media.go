// media.go — --image/--media 的 MIME 對照與三分流取值。
//
// mimeOf／extOf 對齊 cli_config.cpp 的同名對照表。buildMediaItem 是 CLI 特有的取值分流（比照 core-py）：
// 讓 --media 除了讀二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次重算
// base64。回 cllm.Media（URL 直通／原始 bytes）。
package main

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"cllm"
)

// mimeOf：副檔名 → MIME（對齊 cli_config.cpp 的 mime_of）。
func mimeOf(path string) string {
	ext := strings.ToLower(strings.TrimPrefix(filepath.Ext(path), "."))
	m := map[string]string{
		"png": "image/png", "jpg": "image/jpeg", "jpeg": "image/jpeg",
		"gif": "image/gif", "webp": "image/webp", "wav": "audio/wav",
		"mp3": "audio/mpeg",
	}
	if v, ok := m[ext]; ok {
		return v
	}
	return "application/octet-stream"
}

// extOf：MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of）。
func extOf(mime string) string {
	m := map[string]string{
		"image/png": "png", "image/jpeg": "jpg", "image/gif": "gif",
		"image/webp": "webp", "audio/wav": "wav", "audio/mpeg": "mp3",
	}
	if v, ok := m[mime]; ok {
		return v
	}
	return "bin"
}

// mediaDescriptor＝.json 描述子的兩形：{"url":…} 或 {"mime":…,"data":<base64>}。
type mediaDescriptor struct {
	URL  string `json:"url"`
	Mime string `json:"mime"`
	Data string `json:"data"`
}

// buildMediaItem：--image/--media 一個值 → cllm.Media（三分流）。成功回 (media, true)，失敗印 stderr 回 false。
//
//  1. data:／http(s):// URL → 直接當 media 的 url 送、不編碼（帶 url 就不帶 bytes）。
//  2. .json 副檔名（用副檔名判定、不嗅探內容）→ 讀該檔 json.Unmarshal，當「預先算好的 media 描述子」
//     直通、不重算 base64。接受 {"url":…} 或 {"mime":…,"data":<base64>}。
//  3. 其餘（二進位圖檔路徑）→ 讀檔 + base64（交內核；mime 由副檔名推）。
func buildMediaItem(spec string) (cllm.Media, bool) {
	low := strings.ToLower(spec)
	if strings.HasPrefix(spec, "data:") || strings.HasPrefix(low, "http://") || strings.HasPrefix(low, "https://") {
		return cllm.Media{URL: spec}, true // URL 直通
	}
	if strings.HasSuffix(low, ".json") { // 預先算好的 media 描述子
		body, err := readFileText(spec)
		if err != nil {
			fmt.Fprintf(os.Stderr, "讀不到檔案：%s（--image/--media 描述子）\n", spec)
			return cllm.Media{}, false
		}
		var d mediaDescriptor
		if err := json.Unmarshal([]byte(body), &d); err != nil {
			fmt.Fprintf(os.Stderr, "media 描述子 JSON 解析失敗（%s）\n", spec)
			return cllm.Media{}, false
		}
		if d.URL != "" {
			return cllm.Media{URL: d.URL}, true // 直通 url、不編碼
		}
		if d.Mime != "" && d.Data != "" {
			raw, err := base64.StdEncoding.DecodeString(d.Data) // decode 回 raw、走 bytes 路
			if err != nil {
				fmt.Fprintf(os.Stderr, "media 描述子的 data 不是合法 base64（%s）\n", spec)
				return cllm.Media{}, false
			}
			return cllm.Media{Mime: d.Mime, Bytes: raw}, true
		}
		fmt.Fprintf(os.Stderr, "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（%s）\n", spec)
		return cllm.Media{}, false
	}
	raw, err := readFileBytes(spec) // 二進位圖檔：讀檔 + base64
	if err != nil {
		fmt.Fprintf(os.Stderr, "讀不到檔案：%s（--image/--media）\n", spec)
		return cllm.Media{}, false
	}
	return cllm.Media{Mime: mimeOf(spec), Bytes: raw}, true
}
