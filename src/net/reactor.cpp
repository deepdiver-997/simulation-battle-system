// 原生事件循环实现（kqueue / epoll / WSAPoll）。
//
// 平台差异收敛到两处：本文件头部的 socket API 封装（fd 承载 / 错误码 /
// 非阻塞 / 唤醒通道），以及 poller_platform.cpp 的多路复用后端。
// WSAPoll 与 kqueue/epoll 同属 readiness 语义，主循环逻辑三平台共用；
// IOCP（completion）不属于"再写一个 Poller"，见 docs_local/docs/07-工具与测试/
// 网络层设计.md（决策 D5：Windows 用 WSAPoll，不用 IOCP）。

#include <cstdint>
#include <net/reactor.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
// Windows（MinGW/MSYS2 UCRT64）：winsock2 全家桶。
// _WIN32_WINNT 下限 0x0600：WSAPoll 只在 Vista+ 声明。
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>  // FormatMessageA（错误码 → 可读文本）
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace net {
namespace {

constexpr int kMaxEventsPerWait = 256;
// 单次事件回调里最多连续读多少轮，避免一个高速发送方饿死其它连接。
constexpr int kMaxReadBurstPerEvent = 64;
// 单次 accept 最多接收多少连接，理由同上。
constexpr int kMaxAcceptBurst = 128;

#if defined(MSG_NOSIGNAL)
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
// macOS/BSD/Windows 没有 MSG_NOSIGNAL：POSIX 侧靠 SO_NOSIGPIPE（见
// set_common_socket_options）；Windows 上 send 对已关连接返回 WSAECONNABORTED
// 而不是杀进程，无需兜底。
constexpr int kSendFlags = 0;
#endif

#if defined(_WIN32)

// ── winsock 生命周期 ─────────────────────────────────────────────────────────
// WSAStartup 是 winsock 所有调用的前置条件，进程级一次性。
// Reactor 构造时调用：listen() 的 getaddrinfo 之前必然经过这里。
void ensure_winsock() {
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data{};
        const int rc = ::WSAStartup(MAKEWORD(2, 2), &data);
        if (rc != 0) {
            std::fprintf(stderr, "[net] WSAStartup failed: %d\n", rc);
        }
    });
}

// ── fd 承载约定 ──────────────────────────────────────────────────────────────
// 接口层（reactor.h / poller.h）统一用 int 承载 socket。Windows 的 SOCKET 是
// 64 位无符号句柄，但内核分配的 socket 值实践中远小于 2^31，int 足够承载；
// INVALID_SOCKET（全 1）经 static_cast 映射为 -1，与全代码的 fd<0 判定吻合。
inline int socket_to_fd(SOCKET s) { return static_cast<int>(s); }
inline SOCKET fd_to_socket(int fd) { return static_cast<SOCKET>(fd); }

// winsock 错误码家族（WSAE*），与 errno.h 的 E* 取值不同，不能混用。
inline int sock_err() { return static_cast<int>(::WSAGetLastError()); }
constexpr int kSockErrWouldBlock = WSAEWOULDBLOCK;
constexpr int kSockErrInProgress = WSAEINPROGRESS;
constexpr int kSockErrInterrupted = WSAEINTR;
constexpr int kSockErrBadFd = WSAENOTSOCK;
constexpr int kSockErrConnAborted = WSAECONNABORTED;

// 错误码 → 可读文本（FormatMessage 本地化文案 + 原始码，日志用）。
std::string sock_err_str(int err) {
    char buf[256] = {0};
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                     static_cast<DWORD>(err), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                     sizeof(buf), nullptr);
    std::string text(buf);
    while (!text.empty() &&
           (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    if (text.empty()) {
        text = "winsock error";
    }
    return text + " (code " + std::to_string(err) + ")";
}

void close_socket_fd(int fd) { ::closesocket(fd_to_socket(fd)); }

// O_NONBLOCK/fcntl → ioctlsocket(FIONBIO)。
bool set_nonblocking(int fd) {
    u_long mode = 1;
    return ::ioctlsocket(fd_to_socket(fd), FIONBIO, &mode) == 0;
}

// Windows 没有 pipe()，且多路复用只吃 SOCKET —— 唤醒通道用一对回环 TCP 互联
// socket 等价替代：临时监听 127.0.0.1 → 双向互联 → 关监听。
// TCP_NODELAY 必须关：唤醒字节若被 Nagle+延迟 ACK 拖住，post 的实时性就没了。
bool create_wakeup_pair(int out[2]) {
    SOCKET listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        return false;
    }
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(0x7F000001u);  // 127.0.0.1（INADDR_LOOPBACK 并非所有 MinGW 头都有）
    addr.sin_port = 0;
    if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(listener, 1) != 0) {
        ::closesocket(listener);
        return false;
    }
    sockaddr_in bound;
    std::memset(&bound, 0, sizeof(bound));
    int bound_len = sizeof(bound);
    if (::getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &bound_len) != 0) {
        ::closesocket(listener);
        return false;
    }
    SOCKET write_end = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (write_end == INVALID_SOCKET) {
        ::closesocket(listener);
        return false;
    }
    if (::connect(write_end, reinterpret_cast<sockaddr*>(&bound), sizeof(bound)) != 0) {
        ::closesocket(write_end);
        ::closesocket(listener);
        return false;
    }
    SOCKET read_end = ::accept(listener, nullptr, nullptr);
    ::closesocket(listener);
    if (read_end == INVALID_SOCKET) {
        ::closesocket(write_end);
        return false;
    }

    int one = 1;
    const char* one_p = reinterpret_cast<const char*>(&one);
    ::setsockopt(write_end, IPPROTO_TCP, TCP_NODELAY, one_p, sizeof(one));
    ::setsockopt(read_end, IPPROTO_TCP, TCP_NODELAY, one_p, sizeof(one));
    u_long nonblocking = 1;
    ::ioctlsocket(write_end, FIONBIO, &nonblocking);
    ::ioctlsocket(read_end, FIONBIO, &nonblocking);

    out[0] = socket_to_fd(read_end);   // loop 线程读端
    out[1] = socket_to_fd(write_end);  // post/stop 写端
    return true;
}

// 唤醒通道走 socket（send/recv）；连接收发同型。长度 int（winsock 签名如此）。
inline int wakeup_notify(int fd, const void* buf, std::size_t len) {
    return ::send(fd_to_socket(fd), static_cast<const char*>(buf), static_cast<int>(len), 0);
}
inline int wakeup_drain_once(int fd, void* buf, std::size_t len) {
    return ::recv(fd_to_socket(fd), static_cast<char*>(buf), static_cast<int>(len), 0);
}
inline int sock_send(int fd, const char* buf, int len, int flags) {
    return ::send(fd_to_socket(fd), buf, len, flags);
}
inline int sock_recv(int fd, char* buf, int len, int flags) {
    return ::recv(fd_to_socket(fd), buf, len, flags);
}

#else  // POSIX

// POSIX：SOCKET == int，恒等映射，保证与 Windows 分支共用同一套调用点。
inline int socket_to_fd(int fd) { return fd; }
inline int fd_to_socket(int fd) { return fd; }

inline int sock_err() { return errno; }
constexpr int kSockErrWouldBlock = EAGAIN;
constexpr int kSockErrInProgress = EINPROGRESS;
constexpr int kSockErrInterrupted = EINTR;
constexpr int kSockErrBadFd = EBADF;
constexpr int kSockErrConnAborted = ECONNABORTED;

std::string sock_err_str(int err) { return std::strerror(err); }

void close_socket_fd(int fd) { ::close(fd); }

bool set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool create_wakeup_pair(int out[2]) {
    if (::pipe(out) != 0) {
        return false;
    }
    set_nonblocking(out[0]);
    set_nonblocking(out[1]);
    return true;
}

// 唤醒通道在 POSIX 上是 pipe（write/read；send 不认 pipe fd）。
inline ssize_t wakeup_notify(int fd, const void* buf, std::size_t len) {
    return ::write(fd, buf, len);
}
inline ssize_t wakeup_drain_once(int fd, void* buf, std::size_t len) {
    return ::read(fd, buf, len);
}
inline ssize_t sock_send(int fd, const char* buf, std::size_t len, int flags) {
    return ::send(fd, buf, len, flags);
}
inline ssize_t sock_recv(int fd, char* buf, std::size_t len, int flags) {
    return ::recv(fd, buf, len, flags);
}

#endif  // platform

void set_common_socket_options(int fd) {
    int one = 1;
    // setsockopt 第 4 参：POSIX 是 const void*，Windows 是 const char*——
    // 统一按 char* 转发，两边都成立。
    const char* one_p = reinterpret_cast<const char*>(&one);
    ::setsockopt(fd_to_socket(fd), IPPROTO_TCP, TCP_NODELAY, one_p, sizeof(one));
    ::setsockopt(fd_to_socket(fd), SOL_SOCKET, SO_KEEPALIVE, one_p, sizeof(one));
#if defined(SO_NOSIGPIPE)
    // 没有这个的话，向已被对端关闭的连接 write() 会直接给进程发 SIGPIPE 把服务打死。
    ::setsockopt(fd_to_socket(fd), SOL_SOCKET, SO_NOSIGPIPE, one_p, sizeof(one));
#endif
}

std::string format_peer(const sockaddr* sa, socklen_t len) {
#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif
#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif
    char host[NI_MAXHOST] = {0};
    char serv[NI_MAXSERV] = {0};
    if (::getnameinfo(sa, len, host, sizeof(host), serv, sizeof(serv),
                      NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        return "unknown";
    }
    return std::string(host) + ":" + serv;
}

}  // namespace

// ---------------------------------------------------------------- Connection

Connection::Connection(Reactor* reactor, int fd, std::string peer)
    : reactor_(reactor), fd_(fd), peer_(std::move(peer)) {}

Connection::~Connection() {
    // 正常路径下 request_close 已经释放过 fd；这里是兜底，防止构造后立刻失败的路径泄漏。
    if (!closed_.load(std::memory_order_acquire) && fd_ >= 0) {
        close_socket_fd(fd_);
    }
}

bool Connection::is_in_loop_thread() const {
    return reactor_ != nullptr && reactor_->in_loop_thread();
}

std::size_t Connection::pending_write_bytes() const {
    std::lock_guard<std::mutex> lk(write_mutex_);
    return queued_bytes_;
}

void Connection::touch() {
    last_active_ = std::chrono::steady_clock::now();
}

bool Connection::write(std::string bytes, WriteDoneHandler on_done) {
    if (closed_.load(std::memory_order_acquire)) {
        return false;
    }

    const std::size_t max_backlog = reactor_->options().max_write_queue_bytes;
    bool overflow = false;
    std::size_t backlog_snapshot = 0;
    std::size_t attempted = 0;

    {
        std::lock_guard<std::mutex> lk(write_mutex_);
        if (closed_.load(std::memory_order_relaxed)) {
            return false;
        }
        // 上限约束的是**积压**，不是单条消息长度：对端读得慢时积压才会涨。
        // 若把单条消息也算进去，一条合法的、比上限还长的快照就永远发不出去。
        // （服务端自己决定单条写多大，所以这里不对单条长度设限。）
        if (queued_bytes_ >= max_backlog) {
            overflow = true;
            backlog_snapshot = queued_bytes_;
            attempted = bytes.size();
        } else {
            OutEntry entry;
            entry.bytes = std::move(bytes);
            entry.on_done = std::move(on_done);
            queued_bytes_ += entry.bytes.size();
            out_queue_.push_back(std::move(entry));
        }
    }

    if (overflow) {
        // 对端长时间不消费（或故意灌数据）→ 直接断开，不让单个连接把服务端内存拖垮。
        std::fprintf(stderr, "[net] write backlog %zu >= %zu (msg %zu) on %s, closing\n",
                     backlog_snapshot, max_backlog, attempted, peer_.c_str());
        close();
        return false;
    }

    reactor_->schedule_flush(shared_from_this());
    return true;
}

void Connection::close() {
    if (closed_.load(std::memory_order_acquire)) {
        return;
    }
    if (is_in_loop_thread()) {
        do_close();
        return;
    }
    // 跨线程关闭：交给 loop 线程做 fd 注销，避免并发动 poller。
    auto self = shared_from_this();
    reactor_->post([self] { self->do_close(); });
}

void Connection::do_close() {
    bool expected = false;
    if (!closed_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;  // 已经有人关过
    }
    reactor_->request_close(shared_from_this());
}

void Connection::handle_readable() {
    if (closed_.load(std::memory_order_acquire)) {
        return;
    }

    char buf[64 * 1024];
    for (int burst = 0; burst < kMaxReadBurstPerEvent; ++burst) {
        const auto n = sock_recv(fd_, buf, static_cast<int>(sizeof(buf)), 0);
        if (n > 0) {
            touch();
            if (read_handler_) {
                read_handler_(shared_from_this(), buf, static_cast<std::size_t>(n));
            }
            if (closed_.load(std::memory_order_acquire)) {
                return;  // 回调里关了连接，立刻停手
            }
            continue;
        }
        if (n == 0) {
            do_close();  // 对端有序关闭
            return;
        }
        const int err = sock_err();
        if (err == kSockErrInterrupted) {
            continue;
        }
        if (err == kSockErrWouldBlock || err == kSockErrInProgress) {
            return;  // 读干净了
        }
        do_close();  // 真错误
        return;
    }
}

void Connection::flush() {
    if (closed_.load(std::memory_order_acquire)) {
        return;
    }

    std::vector<WriteDoneHandler> completed;
    bool need_write_watch = false;
    bool fatal = false;
    bool backlog_exceeded = false;

    {
        std::lock_guard<std::mutex> lk(write_mutex_);
        while (!out_queue_.empty()) {
            OutEntry& entry = out_queue_.front();
            const char* p = entry.bytes.data() + entry.sent;
            const std::size_t left = entry.bytes.size() - entry.sent;
            const auto n = sock_send(fd_, p, static_cast<int>(left), kSendFlags);

            if (n > 0) {
                const std::size_t sent = static_cast<std::size_t>(n);
                entry.sent += sent;
                queued_bytes_ -= sent;
                if (entry.sent == entry.bytes.size()) {
                    if (entry.on_done) {
                        completed.push_back(std::move(entry.on_done));
                    }
                    out_queue_.pop_front();
                }
                continue;
            }
            const int err = sock_err();
            if (err == kSockErrInterrupted) {
                continue;
            }
            if (err == kSockErrWouldBlock || err == kSockErrInProgress) {
                need_write_watch = true;  // 内核缓冲满了，订阅可写后再继续
                break;
            }
            fatal = true;
            break;
        }

        // 积压的**持续**检查：write() 入口只能拦住"已经超限还来写"的调用，
        // 拦不住"每次进来都在上限之下、队列却在多次调用间爬过上限"——
        // 对端不消费时积压单调上涨，超过硬顶就必须断开，否则无界内存 +
        // 与对端互相僵死（台账记录的整测挂死正是这条路径，2026-09-22 抓到：
        // 客户端阻塞在 send、服务端写队列越顶却无人触发断开）。
        if (!fatal && queued_bytes_ >= reactor_->options_.max_write_queue_bytes) {
            backlog_exceeded = true;
        }
    }

    if (!fatal && need_write_watch != want_write_registered_) {
        // 读方向恒关注；这里只切写方向。
        if (reactor_->poller_->control(fd_, true, need_write_watch, PollOp::kMod)) {
            want_write_registered_ = need_write_watch;
        }
    }

    // 回调放在锁外：on_done 里再调 write() 不会自锁。
    for (auto& cb : completed) {
        cb();
    }

    if (fatal || backlog_exceeded) {
        if (backlog_exceeded) {
            std::fprintf(stderr, "[net] write backlog exceeded %zu during flush on %s, closing\n",
                         reactor_->options_.max_write_queue_bytes, peer_.c_str());
        }
        do_close();
    }
}

// ------------------------------------------------------------------- Reactor

Reactor::Reactor() : Reactor(Options{}) {}

Reactor::Reactor(Options options) : options_(options) {
#if defined(_WIN32)
    ensure_winsock();
#endif
    poller_ = Poller::create();

    if (!create_wakeup_pair(wakeup_pipe_)) {
        wakeup_pipe_[0] = wakeup_pipe_[1] = -1;
        return;
    }
    // 唤醒 fd 必须注册进 poller：跨线程 post()/Connection::write()/stop() 都靠往管道写字节
    // 打断 run 循环的 poll 等待；不注册的话 wait() 永远等不到它，只能靠下一个定时器
    // （idle sweep，默认 1s）超时兜底——跨线程投递延迟上限 1s（2026-10-25 修复上游既有问题）。
    if (!poller_->control(wakeup_pipe_[0], true, false, PollOp::kAdd)) {
        std::fprintf(stderr, "[net] wakeup fd poller registration failed\n");
    }
}

Reactor::~Reactor() {
    stop();

    for (auto& kv : connections_) {
        const int fd = kv.second->fd_;
        if (!kv.second->closed_.load(std::memory_order_acquire) && fd >= 0) {
            close_socket_fd(fd);
            kv.second->closed_.store(true, std::memory_order_release);
        }
    }
    connections_.clear();

    if (listen_fd_ >= 0) {
        close_socket_fd(listen_fd_);
        listen_fd_ = -1;
    }
    for (int i = 0; i < 2; ++i) {
        if (wakeup_pipe_[i] >= 0) {
            close_socket_fd(wakeup_pipe_[i]);
            wakeup_pipe_[i] = -1;
        }
    }
}

bool Reactor::listen(const std::string& host, std::uint16_t port, int backlog) {
    if (!poller_) {
        std::fprintf(stderr, "[net] no poller backend on this platform\n");
        return false;
    }
    if (listen_fd_ >= 0) {
        return false;
    }

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    const std::string port_str = std::to_string(port);
    struct addrinfo* res = nullptr;
    const int rc = ::getaddrinfo(host.empty() ? nullptr : host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0 || res == nullptr) {
#if defined(_WIN32)
        // gai 错误码是 Windows 专属 11xxx 家族，直接给数字（gai_strerror 各头版本参差）。
        std::fprintf(stderr, "[net] getaddrinfo(%s:%s) failed: code %d\n", host.c_str(),
                     port_str.c_str(), rc);
#else
        std::fprintf(stderr, "[net] getaddrinfo(%s:%s) failed: %s\n", host.c_str(), port_str.c_str(),
                     ::gai_strerror(rc));
#endif
        return false;
    }

    int fd = -1;
    for (struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        fd = socket_to_fd(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
        if (fd < 0) {
            continue;
        }
        int one = 1;
        ::setsockopt(fd_to_socket(fd), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one),
                     sizeof(one));
#ifdef SO_REUSEPORT
        // 多 acceptor 形态：几个 reactor 各自 bind 同一端口（BSD / Linux 3.9+），
        // 内核把新连接哈希分给某个监听 fd。单 acceptor 时设了也无害。Windows 无此选项。
        ::setsockopt(fd_to_socket(fd), SOL_SOCKET, SO_REUSEPORT, reinterpret_cast<const char*>(&one),
                     sizeof(one));
#endif
        if (::bind(fd_to_socket(fd), ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)) == 0) {
            break;
        }
        close_socket_fd(fd);
        fd = -1;
    }
    ::freeaddrinfo(res);

    if (fd < 0) {
        std::fprintf(stderr, "[net] bind(%s:%s) failed: %s\n", host.c_str(), port_str.c_str(),
                     sock_err_str(sock_err()).c_str());
        return false;
    }
    if (::listen(fd_to_socket(fd), backlog) != 0) {
        std::fprintf(stderr, "[net] listen failed: %s\n", sock_err_str(sock_err()).c_str());
        close_socket_fd(fd);
        return false;
    }
    if (!set_nonblocking(fd)) {
        close_socket_fd(fd);
        return false;
    }

    struct sockaddr_storage bound;
    socklen_t bound_len = sizeof(bound);
    if (::getsockname(fd_to_socket(fd), reinterpret_cast<struct sockaddr*>(&bound), &bound_len) == 0) {
        if (bound.ss_family == AF_INET) {
            local_port_ = ntohs(reinterpret_cast<struct sockaddr_in*>(&bound)->sin_port);
        } else if (bound.ss_family == AF_INET6) {
            local_port_ = ntohs(reinterpret_cast<struct sockaddr_in6*>(&bound)->sin6_port);
        }
    }

    if (!poller_->control(fd, true, false, PollOp::kAdd)) {
        std::fprintf(stderr, "[net] poller register listen fd failed: %s\n",
                     sock_err_str(sock_err()).c_str());
        close_socket_fd(fd);
        return false;
    }

    listen_fd_ = fd;
    return true;
}

bool Reactor::in_loop_thread() const {
    return std::this_thread::get_id() == loop_thread_id_;
}

std::size_t Reactor::connection_count() const {
    return connections_.size();
}

void Reactor::run() {
    loop_thread_id_ = std::this_thread::get_id();
    running_.store(true, std::memory_order_release);

    if (options_.idle_sweep_interval_ms > 0) {
        // 内部定时器：空闲连接清扫。和用户定时器走同一条链。
        run_every(options_.idle_sweep_interval_ms, [this] { sweep_idle(); });
    }

    while (!stop_requested_.load(std::memory_order_acquire)) {
        run_once(-1);
    }

    // 收尾 drain：最后一轮 wait() 阻塞期间，其它线程可能刚 post 了闭包
    // （唤醒字节已写进管道，但 loop 检测到 stop 后不再进入下一轮 run_once）。
    // 不补这一趟，这些闭包就滞留在 posted_ 里直到 Reactor 析构——闭包若持有
    // Reactor 自身的 shared_ptr（service 停机路径正是这种形态），生命周期就悬在
    // "loop 线程已退出、对象未亡"的窗口上（台账记录过一次整测挂死，2026-09-21）。
    // drain_posted 是 FIFO，这趟把已投递的消化完；stop 之后才 post 的属于调用方
    // 错误，留在队列里随析构丢弃。
    drain_posted();

    running_.store(false, std::memory_order_release);
}

void Reactor::run_once(int timeout_hint) {
    drain_posted();
    sweep_timers();

    if (stop_requested_.load(std::memory_order_acquire)) {
        sweep_closed();
        return;
    }

    int timeout_ms = next_timeout_ms();
    if (timeout_hint >= 0 && (timeout_ms < 0 || timeout_hint < timeout_ms)) {
        timeout_ms = timeout_hint;
    }

    if (!poller_) {
        return;
    }

    PollEvent events[kMaxEventsPerWait];
    const int n = poller_->wait(events, kMaxEventsPerWait, timeout_ms);
    if (n < 0) {
        // 坏 fd 之类通常来自"关闭 fd 与注销 poller 之间的竞态"被上层收敛掉后的残留，
        // 不致命；日志留痕即可，循环继续。
        const int err = sock_err();
        if (err != kSockErrInterrupted && err != kSockErrBadFd) {
            std::fprintf(stderr, "[net] poller wait failed: %s\n", sock_err_str(err).c_str());
        }
        sweep_closed();
        return;
    }

    for (int i = 0; i < n; ++i) {
        const int fd = events[i].fd;

        if (fd == wakeup_pipe_[0]) {
            drain_wakeup();
            continue;
        }
        if (fd == listen_fd_) {
            accept_ready();
            continue;
        }

        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            continue;
        }
        const ConnectionPtr& conn = it->second;
        if (conn->closed_.load(std::memory_order_acquire)) {
            continue;
        }

        if (events[i].error) {
            conn->do_close();
            continue;
        }
        if (events[i].readable) {
            conn->handle_readable();
            if (conn->closed_.load(std::memory_order_acquire)) {
                continue;
            }
        }
        if (events[i].writable) {
            conn->flush();
        }
    }

    sweep_closed();
}

void Reactor::stop() {
    stop_requested_.store(true, std::memory_order_release);
    wakeup();
}

void Reactor::post(std::function<void()> fn) {
    if (!fn) {
        return;
    }
    {
        std::lock_guard<std::mutex> lk(posted_mutex_);
        posted_.push_back(std::move(fn));
    }
    wakeup();
}

void Reactor::wakeup() {
    if (wakeup_pipe_[1] < 0) {
        return;
    }
    const char byte = 1;
    for (;;) {
        const auto n = wakeup_notify(wakeup_pipe_[1], &byte, 1);
        if (n >= 0) {
            return;
        }
        if (sock_err() == kSockErrInterrupted) {
            continue;
        }
        // EAGAIN/WSAEWOULDBLOCK 说明缓冲已满 → loop 线程马上就会醒，无需重试。
        return;
    }
}

void Reactor::drain_wakeup() {
    if (wakeup_pipe_[0] < 0) {
        return;
    }
    char buf[256];
    for (;;) {
        const auto n = wakeup_drain_once(wakeup_pipe_[0], buf, sizeof(buf));
        if (n > 0) {
            continue;
        }
        if (n < 0 && sock_err() == kSockErrInterrupted) {
            continue;
        }
        return;  // EAGAIN 或错误
    }
}

void Reactor::drain_posted() {
    std::deque<std::function<void()>> batch;
    {
        std::lock_guard<std::mutex> lk(posted_mutex_);
        batch.swap(posted_);
    }
    for (auto& fn : batch) {
        fn();
    }
}

TimerId Reactor::run_after(int delay_ms, std::function<void()> fn) {
    TimerEntry entry;
    entry.id = next_timer_id_++;
    entry.interval_ms = 0;
    entry.fn = std::move(fn);

    const auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms < 0 ? 0 : delay_ms);
    auto it = timers_.emplace(when, std::move(entry));
    timer_index_[it->second.id] = it;
    return it->second.id;
}

TimerId Reactor::run_every(int interval_ms, std::function<void()> fn) {
    TimerEntry entry;
    entry.id = next_timer_id_++;
    entry.interval_ms = interval_ms > 0 ? interval_ms : 1;
    entry.fn = std::move(fn);

    const auto when = std::chrono::steady_clock::now() + std::chrono::milliseconds(entry.interval_ms);
    auto it = timers_.emplace(when, std::move(entry));
    timer_index_[it->second.id] = it;
    return it->second.id;
}

void Reactor::cancel_timer(TimerId id) {
    auto idx = timer_index_.find(id);
    if (idx == timer_index_.end()) {
        return;
    }
    timers_.erase(idx->second);
    timer_index_.erase(idx);
}

void Reactor::sweep_timers() {
    const auto now = std::chrono::steady_clock::now();
    while (!timers_.empty() && timers_.begin()->first <= now) {
        auto it = timers_.begin();
        TimerEntry entry = it->second;
        timers_.erase(it);

        if (entry.interval_ms > 0) {
            // 先重新排期再执行：这样回调里 cancel_timer(id) 能生效（否则会被复活）。
            const auto next = now + std::chrono::milliseconds(entry.interval_ms);
            auto rearmed = timers_.emplace(next, entry);
            timer_index_[entry.id] = rearmed;
        } else {
            timer_index_.erase(entry.id);
        }

        if (entry.fn) {
            entry.fn();
        }
    }
}

int Reactor::next_timeout_ms() const {
    if (timers_.empty()) {
        return -1;  // 永久阻塞
    }
    const auto delta = timers_.begin()->first - std::chrono::steady_clock::now();
    if (delta <= std::chrono::steady_clock::duration::zero()) {
        return 0;
    }
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
    return static_cast<int>(ms == 0 ? 1 : ms);
}

void Reactor::sweep_idle() {
    if (!options_.idle_sweep_interval_ms) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    for (auto& kv : connections_) {
        Connection* c = kv.second.get();
        if (c->closed_.load(std::memory_order_acquire) || c->idle_timeout_ms_ <= 0) {
            continue;
        }
        const auto idle = std::chrono::duration_cast<std::chrono::milliseconds>(now - c->last_active_).count();
        if (idle > c->idle_timeout_ms_) {
            c->do_close();
        }
    }
}

void Reactor::accept_ready() {
    for (int burst = 0; burst < kMaxAcceptBurst; ++burst) {
        struct sockaddr_storage ss;
        socklen_t ss_len = sizeof(ss);
        const int cfd = socket_to_fd(
            ::accept(fd_to_socket(listen_fd_), reinterpret_cast<struct sockaddr*>(&ss), &ss_len));
        if (cfd < 0) {
            const int err = sock_err();
            if (err == kSockErrInterrupted) {
                continue;
            }
            if (err != kSockErrWouldBlock && err != kSockErrInProgress &&
                err != kSockErrConnAborted) {
                std::fprintf(stderr, "[net] accept failed: %s\n", sock_err_str(err).c_str());
            }
            return;
        }

        if (!set_nonblocking(cfd)) {
            close_socket_fd(cfd);
            continue;
        }
        set_common_socket_options(cfd);

        // 归属路由：路由器决定这条连接归哪个 reactor（多 worker 轮询分发）。
        // 没配路由器（单 reactor）就留在本 reactor。此刻 fd 还**没有**注册进任何
        // poller —— 旧实现是立即注册进 acceptor 自己的 kqueue，多 worker 形态下
        // 必须由属主 reactor 在自己的线程注册（见 adopt_connection）。
        Reactor* owner = this;
        if (connection_router_) {
            if (Reactor* picked = connection_router_()) {
                owner = picked;
            }
        }

        auto conn = ConnectionPtr(new Connection(owner, cfd, format_peer(reinterpret_cast<sockaddr*>(&ss), ss_len)));
        owner->adopt_connection(conn);
    }
}

void Reactor::adopt_connection(const ConnectionPtr& conn) {
    auto do_adopt = [this, conn] {
        if (conn->closed_.load(std::memory_order_acquire)) {
            return;  // 移交路上已被关掉（防御；正常路径不会发生）
        }
        if (!poller_->control(conn->fd_, true, false, PollOp::kAdd)) {
            std::fprintf(stderr, "[net] adopt: poller register failed: %s\n",
                         sock_err_str(sock_err()).c_str());
            conn->do_close();
            return;
        }
        register_connection(conn);

        // accept 回调在注册之后、同一个投递批次里同步执行。
        // 顺序保证：注册动作在本批次跑完之前不会有下一次 wait() 重新发现事件 ——
        // 所以 handler 一定先于这条 fd 的任何读写事件就位，不存在"事件到了但
        // handler 还没挂上"的窗口。
        if (accept_handler_) {
            accept_handler_(conn);
        } else {
            conn->do_close();
        }
    };

    if (in_loop_thread()) {
        do_adopt();
        return;
    }
    // 跨线程移交：注册与 accept 回调都必须在属主线程做（poller 单线程纪律）。
    // 移交窗口内到达的数据躺在内核接收缓冲里，水平触发模式注册后自然可见 —— 不丢。
    post(do_adopt);
}

void Reactor::schedule_flush(const ConnectionPtr& conn) {
    if (in_loop_thread()) {
        conn->flush();
        return;
    }
    // 跨线程写：把"该刷了"这件事投回 loop 线程，poller 永远只被 loop 线程碰。
    post([conn] { conn->flush(); });
}

void Reactor::request_close(const ConnectionPtr& conn) {
    const int fd = conn->fd_;
    if (fd >= 0) {
        poller_->control(fd, false, false, PollOp::kDel);
        close_socket_fd(fd);
        closing_.push_back(fd);
    }

    if (!conn->close_handler_fired_) {
        conn->close_handler_fired_ = true;
        if (conn->close_handler_) {
            conn->close_handler_(conn);
        }
    }
}

void Reactor::sweep_closed() {
    if (closing_.empty()) {
        return;
    }
    for (int fd : closing_) {
        connections_.erase(fd);
    }
    closing_.clear();
}

void Reactor::register_connection(const ConnectionPtr& conn) {
    if (conn) {
        connections_[conn->fd()] = conn;
    }
}

}  // namespace net
