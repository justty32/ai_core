// server.cpp — TCP（raw socket，無 libfuse）收發迴圈與 main（對應 unipath_9p.py 的 serve）。
#include "ninep.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// 從 socket 精確讀滿 n 個位元組；對端關閉回 false。
static bool readn(int fd, char* buf, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t k = recv(fd, buf + got, n - got, 0);
        if (k <= 0) return false;
        got += (size_t)k;
    }
    return true;
}

static void serveConn(int fd, NineP* np) {
    std::map<uint32_t, Fid> fids;
    for (;;) {
        char hdr[4];
        if (!readn(fd, hdr, 4)) break;
        uint32_t size = (uint8_t)hdr[0] | ((uint8_t)hdr[1] << 8) |
                        ((uint8_t)hdr[2] << 16) | ((uint32_t)(uint8_t)hdr[3] << 24);
        if (size < 7) break;
        std::string body(size - 4, '\0');
        if (!readn(fd, &body[0], size - 4)) break;
        uint8_t mtype = (uint8_t)body[0];
        uint16_t tag = (uint8_t)body[1] | ((uint8_t)body[2] << 8);
        Reader r((const uint8_t*)body.data(), body.size());
        r.i = 3;                       // 跳過 type[1]+tag[2]
        uint8_t rtype; std::string rbody;
        try {
            auto res = np->dispatch(mtype, r, fids);
            rtype = res.first; rbody = res.second;
        } catch (...) {
            rtype = Rerror; rbody.clear(); ps(rbody, "i/o error");
        }
        std::string msg;
        p4(msg, (uint32_t)(4 + 1 + 2 + rbody.size()));
        p1(msg, rtype); p2(msg, tag); msg += rbody;
        if (send(fd, msg.data(), msg.size(), 0) < 0) break;
    }
    close(fd);
}

int main(int argc, char** argv) {
    if (argc != 3 || std::strcmp(argv[1], "serve") != 0) {
        std::fprintf(stderr, "用法: %s serve <port>\n", argv[0]);
        return 2;
    }
    int port = std::atoi(argv[2]);
    Env env; env.startTicker();
    static NineP np(env);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    listen(srv, 8);
    std::printf("unipath-9p (C++) 監聽 127.0.0.1:%d（真 9P2000）\n", port);
    std::fflush(stdout);
    for (;;) {
        int c = accept(srv, nullptr, nullptr);
        if (c < 0) continue;
        std::thread(serveConn, c, &np).detach();
    }
}
