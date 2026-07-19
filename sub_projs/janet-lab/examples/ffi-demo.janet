#!/usr/bin/env janet
# FFI 示範：不編譯任何 C，直接呼叫系統共享庫。
# 跑法：janet examples/ffi-demo.janet
# 詳解見 docs/07-c-互通.md

# 1) 載入共享庫
(def libm (ffi/native "libm.so.6"))
(def libc (ffi/native "libc.so.6"))

# 2) 描述函式簽名：(ffi/signature 呼叫慣例 回傳型別 & 參數型別)
(def sig-d->d (ffi/signature :default :double :double))   # double f(double)
(def sig-strlen (ffi/signature :default :size :string))   # size_t f(const char*)

# 3) 查符號 + 呼叫：(ffi/call 指標 簽名 & 參數)
(printf "cos(0)    = %.4f" (ffi/call (ffi/lookup libm "cos") sig-d->d 0.0))
(printf "sqrt(2)   = %.5f" (ffi/call (ffi/lookup libm "sqrt") sig-d->d 2.0))
(printf "strlen    = %d"   (ffi/call (ffi/lookup libc "strlen") sig-strlen "hello"))

# 常用型別關鍵字：:void :bool :int :uint :long :size :float :double
#                 :ptr（指標）:string（傳給 C 的 const char*）
