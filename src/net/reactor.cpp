// 原生事件循环实现（POSIX：kqueue / epoll）。
//
// 本轮不覆盖 Windows：IOCP 是 completion 语义，不是"再写一个 Poller"能塞进来的，
// 需要替换下面的等待/投递机制。宁可暂时不给未验证的实现，也不要留一份看起来
// 能用、实际没在 Windows 上编过的代码。见 docs_local/docs/07-工具与测试/网络层设计.md。

#include <net/reactor.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#else
#error "net 层当前只实现 POSIX（kqueue/epoll）。Windows 需要另写 IOCP 后端，见网络层设计文档。"
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
// macOS/BSD 没有 MSG_NOSIGNAL，靠 SO_NOSIGPIPE（见 set_common_socket_options）。
constexpr int kSendFlags = 0;
#endif

bool set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void set_common_socket_options(int fd) {
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
#if defined(SO_NOSIGPIPE)
    // 没有这个的话，向已被对端关闭的连接 write() 会直接给进程发 SIGPIPE 把服务打死。
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
}

std::string format_peer(const sockaddr* sa, socklen_t len) {
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
        ::close(fd_);
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
        const ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
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
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
            const ssize_t n = ::send(fd_, p, left, kSendFlags);

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
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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
    poller_ = Poller::create();

    if (::pipe(wakeup_pipe_) == 0) {
        set_nonblocking(wakeup_pipe_[0]);
        set_nonblocking(wakeup_pipe_[1]);
    } else {
        wakeup_pipe_[0] = wakeup_pipe_[1] = -1;
    }
}

Reactor::~Reactor() {
    stop();

    for (auto& kv : connections_) {
        const int fd = kv.second->fd_;
        if (!kv.second->closed_.load(std::memory_order_acquire) && fd >= 0) {
            ::close(fd);
            kv.second->closed_.store(true, std::memory_order_release);
        }
    }
    connections_.clear();

    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    for (int i = 0; i < 2; ++i) {
        if (wakeup_pipe_[i] >= 0) {
            ::close(wakeup_pipe_[i]);
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
        std::fprintf(stderr, "[net] getaddrinfo(%s:%s) failed: %s\n", host.c_str(), port_str.c_str(),
                     ::gai_strerror(rc));
        return false;
    }

    int fd = -1;
    for (struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef SO_REUSEPORT
        // 多 acceptor 形态：几个 reactor 各自 bind 同一端口（BSD / Linux 3.9+），
        // 内核把新连接哈希分给某个监听 fd。单 acceptor 时设了也无害。
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
#endif
        if (::bind(fd, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(res);

    if (fd < 0) {
        std::fprintf(stderr, "[net] bind(%s:%s) failed: %s\n", host.c_str(), port_str.c_str(),
                     std::strerror(errno));
        return false;
    }
    if (::listen(fd, backlog) != 0) {
        std::fprintf(stderr, "[net] listen failed: %s\n", std::strerror(errno));
        ::close(fd);
        return false;
    }
    if (!set_nonblocking(fd)) {
        ::close(fd);
        return false;
    }

    struct sockaddr_storage bound;
    socklen_t bound_len = sizeof(bound);
    if (::getsockname(fd, reinterpret_cast<struct sockaddr*>(&bound), &bound_len) == 0) {
        if (bound.ss_family == AF_INET) {
            local_port_ = ntohs(reinterpret_cast<struct sockaddr_in*>(&bound)->sin_port);
        } else if (bound.ss_family == AF_INET6) {
            local_port_ = ntohs(reinterpret_cast<struct sockaddr_in6*>(&bound)->sin6_port);
        }
    }

    if (!poller_->control(fd, true, false, PollOp::kAdd)) {
        std::fprintf(stderr, "[net] poller register listen fd failed: %s\n", std::strerror(errno));
        ::close(fd);
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
        // EBADF 之类通常来自"关闭 fd 与注销 poller 之间的竞态"被上层收敛掉后的残留，
        // 不致命；日志留痕即可，循环继续。
        if (errno != EINTR && errno != EBADF) {
            std::fprintf(stderr, "[net] poller wait failed: %s\n", std::strerror(errno));
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
    ssize_t n = 0;
    do {
        n = ::write(wakeup_pipe_[1], &byte, 1);
    } while (n < 0 && errno == EINTR);
    // EAGAIN 说明管道已满 → loop 线程马上就会醒，无需重试。
}

void Reactor::drain_wakeup() {
    if (wakeup_pipe_[0] < 0) {
        return;
    }
    char buf[256];
    for (;;) {
        const ssize_t n = ::read(wakeup_pipe_[0], buf, sizeof(buf));
        if (n > 0) {
            continue;
        }
        if (n < 0 && errno == EINTR) {
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
        const int cfd = ::accept(listen_fd_, reinterpret_cast<struct sockaddr*>(&ss), &ss_len);
        if (cfd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED) {
                std::fprintf(stderr, "[net] accept failed: %s\n", std::strerror(errno));
            }
            return;
        }

        if (!set_nonblocking(cfd)) {
            ::close(cfd);
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
            std::fprintf(stderr, "[net] adopt: poller register failed: %s\n", std::strerror(errno));
            conn->do_close();
            return;
        }
        register_connection(conn);

        // accept 回调在注册之后、同一个投递批次里同步执行。
        // 顺序保证：poller 的变更表要等下一次 wait() 才真正提交进内核，而本批次
        // 跑完之前不会有 wait() —— 所以 handler 一定先于这条 fd 的任何读写事件就位，
        // 不存在"事件到了但 handler 还没挂上"的窗口。
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
        ::close(fd);
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
