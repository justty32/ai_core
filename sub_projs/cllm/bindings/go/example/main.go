// example — cllm Go binding：基本 ask、串流、schema+JSON(stdlib)、shell(CLI) 呼叫。
// 跑：source ~/repo/dev/env.sh 後  go run ./example "$CLLM_FIXTURES"
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

	// ④ shell 呼叫 llm CLI
	out, _ := exec.Command("llm", "你好", "--endpoint", base+"fake/chat/completions").Output()
	fmt.Println("[go] shell(llm) =>", strings.TrimSpace(string(out)))
}
