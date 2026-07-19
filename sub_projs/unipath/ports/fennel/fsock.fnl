;; fsock.fnl —— 用 LuaJIT FFI 直綁 socket syscall（環境無 luasocket，故走裸系統呼叫）。
;; 僅需 TCP server 端：make-server / accept / recv-exact / send-all / close。

(local ffi (require :ffi))

(ffi.cdef "
int socket(int domain, int type, int protocol);
int setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen);
int bind(int fd, const void *addr, unsigned int len);
int listen(int fd, int backlog);
int accept(int fd, void *addr, unsigned int *len);
long read(int fd, void *buf, unsigned long count);
long write(int fd, const void *buf, unsigned long count);
int close(int fd);
struct sockaddr_in { unsigned short sin_family; unsigned short sin_port;
                     unsigned int sin_addr; unsigned char sin_zero[8]; };
")

(local C ffi.C)
(local AF_INET 2) (local SOCK_STREAM 1)
(local SOL_SOCKET 1) (local SO_REUSEADDR 2)

(fn htons [p] (+ (* (% p 256) 256) (math.floor (/ p 256))))

(fn make-server [port]
  (let [fd (C.socket AF_INET SOCK_STREAM 0)]
    (when (< (tonumber fd) 0) (error "socket() 失敗"))
    (let [one (ffi.new "int[1]" 1)]
      (C.setsockopt fd SOL_SOCKET SO_REUSEADDR one 4))
    (let [addr (ffi.new "struct sockaddr_in")]
      (set addr.sin_family AF_INET)
      (set addr.sin_port (htons port))
      (set addr.sin_addr 0x0100007f)      ; 127.0.0.1（網路位元組序）
      (when (< (tonumber (C.bind fd (ffi.cast "const void*" addr) 16)) 0)
        (error "bind() 失敗（port 被佔用？）"))
      (when (< (tonumber (C.listen fd 8)) 0) (error "listen() 失敗"))
      fd)))

(fn accept [fd] (C.accept fd nil nil))

(fn recv-exact [fd want]
  ;; 讀滿 want 個位元組，回傳 Lua 字串；連線關閉/不足回 nil
  (let [buf (ffi.new "unsigned char[?]" want)]
    (var got 0) (var ok true)
    (while (and ok (< got want))
      (let [r (tonumber (C.read fd (+ buf got) (- want got)))]
        (if (<= r 0) (set ok false) (set got (+ got r)))))
    (if ok (ffi.string buf want) nil)))

(fn send-all [fd s]
  (var rest s)
  (while (> (length rest) 0)
    (let [r (tonumber (C.write fd rest (length rest)))]
      (if (<= r 0) (set rest "") (set rest (string.sub rest (+ r 1)))))))

(fn close [fd] (C.close fd))

{: make-server : accept : recv-exact : send-all : close}
