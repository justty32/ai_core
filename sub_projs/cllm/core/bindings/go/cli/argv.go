// argv.go — 命令列掃描：把 argv 拆成旗標與位置參數（對齊 cli.cpp 的解析段）。
//
// 固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）特判；
// 反射旗標（連線／取樣，見 flags.go）吃下一個 argv；其餘當位置參數拼 prompt。「-」單獨＝stdin 插入點，
// 其餘 '-' 開頭＝未知旗標。掃描結果收成 parsedArgs；遇 --help 或用法錯回一個退出碼交呼叫端直接返回。
package main

import (
	"fmt"
	"os"
)

// rawFlag＝反射旗標的原始字串值＋其欄位定義（cli.go 轉型時用）。
type rawFlag struct {
	Val  string
	Kind kind
	Flag string
}

// parsedArgs＝argv 掃描結果：旗標收成的各欄位，交給 main 續組 prompt／config／請求。
type parsedArgs struct {
	rawValues     map[string]rawFlag // 反射欄位名 → 原始值
	promptParts   []string
	mediaSpecs    []string
	toolSpecs     []string
	modalitySpecs []string
	schemaText    string
	configPath    string
	mediaOutDir   string
	systemText    string
	hasSchema     bool
	hasConfig     bool
	hasSystem     bool
	stream        bool
}

// parseArgv 掃描 argv，回 (parsedArgs, exit, done)；done=true＝呼叫端直接以 exit 返回（--help／用法錯）。
func parseArgv(args []string) (*parsedArgs, int, bool) {
	p := &parsedArgs{rawValues: map[string]rawFlag{}}
	i, n := 1, len(args)

	// needValue：吃下一個 argv 當旗標的值；缺值回 ("", false)（呼叫端據此回 exitUsage）。
	needValue := func(flag string) (string, bool) {
		if i+1 >= n {
			fmt.Fprintf(os.Stderr, "%s 缺少值（llm --help 看用法）\n", flag)
			return "", false
		}
		i++
		return args[i], true
	}

	noMoreFlags := false
	for i < n {
		a := args[i]
		if noMoreFlags {
			p.promptParts = append(p.promptParts, a)
			i++
			continue
		}
		switch {
		case a == "--":
			noMoreFlags = true
		case a == "--help" || a == "-h":
			printUsage(os.Stderr)
			return nil, exitOK, true
		case a == "--stream":
			p.stream = true
		case a == "--image" || a == "--media":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.mediaSpecs = append(p.mediaSpecs, v)
		case a == "--schema":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.schemaText, p.hasSchema = v, true
		case a == "--system":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.systemText, p.hasSystem = v, true
		case a == "--config":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.configPath, p.hasConfig = v, true
		case a == "--tool":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.toolSpecs = append(p.toolSpecs, v)
		case a == "--modality":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.modalitySpecs = append(p.modalitySpecs, v)
		case a == "--media-out":
			v, ok := needValue(a)
			if !ok {
				return nil, exitUsage, true
			}
			p.mediaOutDir = v
		default:
			if rf, ok := flagToField[a]; ok {
				v, ok := needValue(a)
				if !ok {
					return nil, exitUsage, true
				}
				p.rawValues[rf.Field] = rawFlag{Val: v, Kind: rf.Kind, Flag: a}
			} else if len(a) > 0 && a[0] == '-' && a != "-" { // 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標
				fmt.Fprintf(os.Stderr, "未知旗標：%s（llm --help 看用法）\n", a)
				return nil, exitUsage, true
			} else {
				p.promptParts = append(p.promptParts, a)
			}
		}
		i++
	}
	return p, exitOK, false
}
