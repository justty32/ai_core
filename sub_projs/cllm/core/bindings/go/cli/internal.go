// internal.go — CLI 共用的退出碼、環境變數鍵、檔案讀取小工具（對齊 core/src/cli_internal.hpp）。
//
// 葉檔：只依標準庫、不依賴 cli/ 其他檔，作為其餘 CLI 檔（flags/config/media/output/cli）的共用底，
// 把依賴收成一張無環 DAG。
package main

import "os"

// 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
const (
	exitOK      = 0
	exitUsage   = 1
	exitRequest = 2
	exitCancel  = 130
)

// configEnvVar 對齊 cli_internal.hpp 的 kConfigEnvVar：只指定 config 檔路徑，不存任何設定值。
const configEnvVar = "LLM_CLI_CONFIG"

// readFileBytes 整檔讀成 raw bytes（媒體圖檔等）。讀不到回 error，由呼叫端轉退出碼。
func readFileBytes(path string) ([]byte, error) {
	return os.ReadFile(path)
}

// readFileText 整檔讀成 UTF-8 文字（config／media 描述子等）。讀不到回 error。
func readFileText(path string) (string, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	return string(b), nil
}
