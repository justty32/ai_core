// main.go — Go 版 llm CLI 進入點（跑：source ~/dev/cllm/env.sh 後  go run ./cli [旗標...] [prompt...]）。
//
// 只做 os.Args 轉呼叫 run，並包 SIGINT → 130（對齊 C++ 的 SIGINT 取消退出碼）。真正的接線在 cli.go。
package main

import (
	"fmt"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	// SIGINT（Ctrl-C）→ 印「已取消」、退出碼 130。cllm.Ask 阻塞在 cgo，靠 signal goroutine 攔截退出。
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT)
	go func() {
		<-sig
		fmt.Fprintln(os.Stderr, "\n已取消")
		os.Exit(exitCancel)
	}()

	os.Exit(run(os.Args))
}
