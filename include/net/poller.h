#ifndef NET_POLLER_H
#define NET_POLLER_H

// 平台 IO 多路复用原语的最小抽象。
//
// 存在的理由：整个 net 层里**只有这里**需要区分 kqueue / epoll / IOCP。
// 上层 Reactor 只认这套接口，所以平台差异被压到一个文件里。
//
// 语义约定（三者统一采用水平触发 / readiness）：
//   - control() 只允许在 Reactor 所在线程调用（跨线程一律走 Reactor::post）。
//   - wait() 返回的就绪集合是**水平触发**的：只要可读/可写没处理干净，
//     下一次 wait() 还会报同一个 fd。上层因此可以简单地"读到 EAGAIN 为止"。
//   - 为什么要 readiness 而不是 completion：epoll/kqueue 本身是 readiness，
//     IOCP 是 completion。二者不可通约——所以 IOCP 后端不是"再写一个 Poller"，
//     而是替换 Reactor 的等待/投递机制（见 docs_local/docs/07-工具与测试/网络层设计.md 的说明）。
//     本文件只覆盖 readiness 家族（macOS/Linux），Windows 不在本轮范围。

#include <cstdint>
#include <memory>

namespace net {

enum class PollOp {
    kAdd,  // 新注册
    kMod,  // 改关注集合
    kDel,  // 注销（fd 关闭前调用；调用后不得再对该 fd 调 control）
};

struct PollEvent {
    int fd = -1;
    bool readable = false;
    bool writable = false;
    bool error = false;  // 需要直接关闭的错误（EPOLLERR/HUP 等）
};

class Poller {
public:
    virtual ~Poller() = default;

    // 注册/修改/注销关注。失败返回 false（errno 保留）。
    virtual bool control(int fd, bool want_read, bool want_write, PollOp op) = 0;

    // 阻塞等待就绪，最多 max_events 个，timeout_ms < 0 表示永久阻塞。
    // 返回就绪个数，-1 表示出错（errno 保留，EINTR 由实现内部消化）。
    virtual int wait(PollEvent* out, int max_events, int timeout_ms) = 0;

    // 当前平台实现；无可用后端时返回 nullptr。
    static std::unique_ptr<Poller> create();

    // 平台后端名（日志用）："kqueue" / "epoll" / "none"
    static const char* backend_name();
};

}  // namespace net

#endif  // NET_POLLER_H
