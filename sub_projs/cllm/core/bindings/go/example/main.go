// example — cllm Go binding：基本 ask、串流、schema+JSON(stdlib)、tools、media 輸出/輸入、shell(CLI) 呼叫。
// 跑：source ~/dev/cllm/env.sh 後  go run ./example "$CLLM_FIXTURES"
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strings"

	"cllm"
)

func main() {
	base := ""
	if len(os.Args) > 1 {
		base = os.Args[1]
	}
	// 有 base 才加 Endpoint（空＝走內建 localhost）。
	ep := func(leaf string, extra ...cllm.Option) []cllm.Option {
		if base != "" {
			return append(extra, cllm.Endpoint(base+leaf))
		}
		return extra
	}

	// ① 基本
	ans, err := cllm.Ask("你好", ep("fake/chat/completions")...)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Println("[go] ask =>", ans)

	// ② 串流 OnDelta
	fmt.Print("[go] 串流 => ")
	cllm.Ask("數到五", ep("fake_stream/chat/completions",
		cllm.Stream(),
		cllm.OnDelta(func(p string) bool { fmt.Printf("[%s]", p); return false }))...)
	fmt.Println()

	// ③ schema → JSON（stdlib encoding/json）
	raw, _ := cllm.Ask("給我角色", ep("fake_json/chat/completions", cllm.Schema(`{"type":"object"}`))...)
	var o struct {
		Name      string   `json:"name"`
		Affection int      `json:"affection"`
		Lines     []string `json:"lines"`
	}
	json.Unmarshal([]byte(raw), &o)
	fmt.Printf("[go] json => name=%s affection=%d lines=%d\n", o.Name, o.Affection, len(o.Lines))

	// ④ tools＋OnTool：fake_tool 回一個 tool_call，OnTool 收、用 stdlib json 解 Arguments
	_, err = cllm.Ask("東京天氣如何？", ep("fake_tool/chat/completions",
		cllm.Tools(cllm.ToolDef{Name: "get_weather", Description: "查某城市天氣", Parameters: `{"type":"object"}`}),
		cllm.OnTool(func(tc cllm.ToolCall) bool {
			var args struct {
				City string `json:"city"`
				Unit string `json:"unit"`
			}
			json.Unmarshal([]byte(tc.Arguments), &args)
			fmt.Printf("[go] tool => %s(city=%s, unit=%s)\n", tc.Name, args.City, args.Unit)
			return false
		}))...)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	// ⑤ media 輸出：fake_media 回 content+audio，OnMedia 收模型產出的媒體
	_, err = cllm.Ask("說句話", ep("fake_media/chat/completions",
		cllm.OnMedia(func(m cllm.MediaOut) bool {
			fmt.Printf("[go] media out => mime=%s bytes=%d\n", m.Mime, len(m.Bytes))
			return false
		}))...)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	// ⑥ media 輸入＋modalities：掛 Media（data URI）＋Modalities 打 fake（body 被忽略，驗的是搬運不炸）
	_, err = cllm.Ask("描述這張圖", ep("fake/chat/completions",
		cllm.MediaIn(cllm.Media{URL: "data:image/png;base64,iVBORw0KGgo="}),
		cllm.Modalities(cllm.Modality{Name: "audio", Config: `{"voice":"alloy","format":"wav"}`}))...)
	if err != nil {
		fmt.Println("[go] media in+modality =>", err)
	} else {
		fmt.Println("[go] media in+modality => ok")
	}

	// ⑦ shell 呼叫 llm CLI
	out, _ := exec.Command("llm", "你好", "--endpoint", base+"fake/chat/completions").Output()
	fmt.Println("[go] shell(llm) =>", strings.TrimSpace(string(out)))
}
