// config.go — 三層 config 來源解析（對齊 core/src/cli_config.cpp 的 load_into）。
//
// 來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本檔只處理「config 檔」這層；
// 命令列旗標覆寫在 cli.go。config 檔路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
// 未列鍵靜默忽略（lenient；與 C++ glaze 的嚴格解析刻意分歧，見 core-py README 已知落差）。
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
)

// configKeys＝config 檔允許的鍵（對齊 llm::abi::Client 欄位；未列鍵靜默忽略＝lenient）。
var configKeys = func() map[string]kind {
	m := make(map[string]kind, len(reflectFlags))
	for _, f := range reflectFlags {
		m[f.Field] = f.Kind
	}
	return m
}()

// defaultConfigPath＝~/.config/llm/config.json（對齊 cli_config.cpp：靠 HOME）。
func defaultConfigPath() string {
	home := os.Getenv("HOME")
	if home == "" {
		return ""
	}
	return filepath.Join(home, ".config", "llm", "config.json")
}

// coerceJSON：把 config 檔 JSON 的值依欄位型別轉成 client 要的型別（string／int／float32）。
func coerceJSON(k kind, v interface{}) (interface{}, bool) {
	switch k {
	case kStr:
		s, ok := v.(string)
		return s, ok
	case kInt:
		if f, ok := v.(float64); ok { // encoding/json 數字一律 float64
			return int(f), true
		}
	case kFloat:
		if f, ok := v.(float64); ok {
			return float32(f), true
		}
	}
	return nil, false
}

// loadConfig 走三層 config 前二層（對齊 cli_config.cpp 的 load_into），寫進 client。回退出碼。
func loadConfig(client map[string]interface{}, hasConfig bool, configPath string) int {
	cfgPath, named := configPath, hasConfig
	if !hasConfig {
		if env := os.Getenv(configEnvVar); env != "" {
			cfgPath, named = env, true
		} else {
			cfgPath = defaultConfigPath()
		}
	}
	if cfgPath == "" {
		return exitOK
	}

	body, err := readFileText(cfgPath)
	if err != nil {
		if named { // 明指卻讀不到＝用法錯（點名是誰指的路）
			who := configEnvVar
			if hasConfig {
				who = "--config"
			}
			fmt.Fprintf(os.Stderr, "讀不到檔案：%s（%s 指定的 config 檔）\n", cfgPath, who)
			return exitUsage
		}
		return exitOK // 探測路徑讀不到＝沒設定檔，靜默用預設
	}

	var data map[string]interface{}
	if err := json.Unmarshal([]byte(body), &data); err != nil {
		fmt.Fprintf(os.Stderr, "config JSON 解析失敗（%s）\n", cfgPath)
		return exitUsage
	}
	for k, v := range data {
		if kd, ok := configKeys[k]; ok {
			if cv, ok := coerceJSON(kd, v); ok {
				client[k] = cv
			}
		}
	}
	return exitOK
}
