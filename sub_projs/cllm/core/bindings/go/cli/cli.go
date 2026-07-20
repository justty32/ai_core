// cli.go — 薄 CLI 外殼：把命令列組成一次 cllm.Ask 的發問（鏡像 C++ 的 `llm` CLI）。
//
// 只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給 binding 的 cllm.Ask。周邊拆到
// 姊妹檔（皆對齊 C++ 分檔，比照 core-py）：internal.go（cli_internal.hpp）＝退出碼／env 鍵／檔案讀取；
// flags.go（cli_flags.cpp）＝反射旗標表＋printUsage；argv.go＝argv 掃描（cli.cpp 解析段）；config.go
// （cli_config.cpp）＝三層 config 解析；media.go＝--media 三分流；reqinput.go＝請求類旗標組成；output.go
// （cli_output.cpp）＝出口 handlers（Sink）。本檔（對齊 cli.cpp）只留 main 接線。
//
// 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
package main

import (
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"

	"cllm"
)

// run＝CLI 主流程（對齊 cli.cpp）；回退出碼。
func run(args []string) int {
	// ── (1) 掃描 argv ──
	p, ec, done := parseArgv(args)
	if done {
		return ec
	}

	// ── (2) prompt：位置參數與導管 stdin 可合體 ──
	prompt, ec, done := buildPrompt(p)
	if done {
		return ec
	}

	// ── (3) 組 client 設定：內建預設 → config → 反射旗標覆寫 ──
	client := map[string]interface{}{}
	if ec := loadConfig(client, p.hasConfig, p.configPath); ec != exitOK {
		return ec
	}
	if ec := applyFlagOverrides(client, p); ec != exitOK {
		return ec
	}

	// ── (4) 組請求輸入 ＋ 呼叫內核 ──
	r, ec, done := buildRequestInputs(p)
	if done {
		return ec
	}

	sink := newSink(p.mediaOutDir)
	opts := buildOptions(p, client, r, sink)
	cllm.Ask(prompt, opts...)

	ok := !sink.HadError
	if sink.WroteText && ok { // 補尾端換行（stdout 收尾乾淨）
		fmt.Println()
	}
	if !ok {
		return exitRequest
	}
	if sink.MediaErr { // 媒體落檔失敗＝結果真掉了
		return exitRequest
	}
	return exitOK
}

// buildPrompt：位置參數與導管 stdin 合體（對齊 cli.cpp）——「-」＝stdin 插入點；沒寫「-」而兩者都有＝
// prompt＋空行＋stdin；只給其一＝用那一個；互動終端不讀 stdin（避免卡住）。回 (prompt, exit, done)。
func buildPrompt(p *parsedArgs) (string, int, bool) {
	hasDash := false
	for _, part := range p.promptParts {
		if part == "-" {
			hasDash = true
		}
	}
	stdinText := ""
	if !stdinIsTTY() { // 只在被導管/檔案餵入時整段讀
		b, _ := io.ReadAll(os.Stdin)
		stdinText = strings.TrimRight(string(b), "\r\n")
	} else if hasDash {
		fmt.Fprintln(os.Stderr, "「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（llm --help 看用法）")
		return "", exitUsage, true
	}

	pieces := make([]string, len(p.promptParts))
	for i, part := range p.promptParts {
		if part == "-" {
			pieces[i] = stdinText
		} else {
			pieces[i] = part
		}
	}
	prompt := strings.Join(pieces, " ")
	if !hasDash && stdinText != "" { // 沒寫「-」而兩者都有＝prompt＋空行＋stdin
		if prompt == "" {
			prompt = stdinText
		} else {
			prompt = prompt + "\n\n" + stdinText
		}
	}
	if prompt == "" {
		fmt.Fprintln(os.Stderr, "缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）")
		return "", exitUsage, true
	}
	return prompt, exitOK, false
}

// stdinIsTTY：stdin 是否為互動終端（非導管/檔案）。純標準庫，免外部 term 套件。
func stdinIsTTY() bool {
	info, err := os.Stdin.Stat()
	if err != nil {
		return false
	}
	return (info.Mode() & os.ModeCharDevice) != 0
}

// applyFlagOverrides：命令列反射旗標覆寫 client（對齊 cli.cpp 的旗標覆寫段）。型別錯回 exitUsage。
func applyFlagOverrides(client map[string]interface{}, p *parsedArgs) int {
	for field, rf := range p.rawValues {
		switch rf.Kind {
		case kStr:
			client[field] = rf.Val
		case kInt:
			v, err := strconv.Atoi(rf.Val)
			if err != nil {
				fmt.Fprintf(os.Stderr, "%s 需要整數（給了「%s」）\n", rf.Flag, rf.Val)
				return exitUsage
			}
			client[field] = v
		case kFloat:
			v, err := strconv.ParseFloat(rf.Val, 32)
			if err != nil {
				fmt.Fprintf(os.Stderr, "%s 需要小數（給了「%s」）\n", rf.Flag, rf.Val)
				return exitUsage
			}
			client[field] = float32(v)
		}
	}
	return exitOK
}
