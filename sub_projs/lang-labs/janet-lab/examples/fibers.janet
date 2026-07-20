#!/usr/bin/env janet
# Fiber 示範：Janet 的協程 / generator / 錯誤處理都是同一個東西。
# 跑法：janet examples/fibers.janet
# 詳解見 docs/09-fiber.md

# 1) fiber 當 generator：yield 產值，resume 取值
(def f (fiber/new (fn [] (yield 1) (yield 2) (yield 3))))
(printf "resume 三次：%q %q %q" (resume f) (resume f) (resume f))
(printf "status = %q" (fiber/status f))

# 2) 用 each / loop :in 直接走訪一個 fiber
(def squares (fiber/new (fn [] (for i 0 5 (yield (* i i))))))
(prin "squares: ")
(each x squares (prin x " "))
(print)

# 3) 錯誤處理：try 是包在 fiber 上的糖
(print (try (error "boom") ([err] (string "抓到：" err))))

# 4) protect：把 (成功? 值) 包成 tuple 回傳，不 throw
(printf "protect error => %q" (protect (error "bad")))   # (false "bad")
(printf "protect ok    => %q" (protect (+ 1 2)))         # (true 3)

# 5) 手動用信號遮罩攔截 error（:e = 攔 error 信號）
(def g (fiber/new (fn [] (error "manual-error")) :e))
(def r (resume g))
(printf "status=%q value=%s" (fiber/status g) r)          # :error manual-error

# 6) ev：內建的 fiber 排程器，做非同步 / 並發
(ev/spawn (print "背景 A"))
(ev/spawn (print "背景 B"))
(ev/sleep 0)
(print "主線最後")
