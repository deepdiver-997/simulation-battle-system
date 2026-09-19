#ifndef NET_REACTOR_H
#define NET_REACTOR_H

// 原生事件循环（reactor）：TCP 监听 / 连接 / 非阻塞读写 / 定时器 / 跨线程投递。
//
// 定位：替换原来的 Boost.Asio 使用面。原代码只用到 asio 的
// accept + async_read_some + async_write + thread_pool::post，属于 asio 能力面的
// 极小一角（解析器、协程、超时组合器一个都没用），所以自持一个 reactor 的
// 代码量和维护成本都远低于引入整个 Boost。
//
// 线程模型（刻意保持简单，够用且不容易错）：
//   - 一个 Reactor 绑定一个线程；所有 poller 操作都在该线程内完成。
//   - 跨线程只允许两件事：post(fn) 和 Connection::write()。二者内部都会
//     通过唤醒管道把工作交回 loop 线程，不存在对 poller 的并发访问。
//   - 业务侧（战斗计算）在 Reactor 之外的线程池上跑，算完调 Connection::write()
//     即可，不需要知道 loop 在哪。
//
// 生命周期：
//   - Reactor 用 shared_ptr 持有所有 Connection；连接关闭时先从 poller 注销、
//     close(fd)，本轮流结束后再从表里摘除（延迟摘除避免在事件回调里迭代中失效）。
//   - Connection 的 read/close 回调通过 shared_ptr 保证"回调执行期间对象存活"。

#include <net/poller.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace net {

class Reactor;
class Connection;

using ConnectionPtr = std::shared_ptr<Connection>;

// 收到数据（可能是半包，由上层分帧）。
using ReadHandler = std::function<void(const ConnectionPtr&, const char* data, std::size_t len)>;
// 连接关闭，保证最多触发一次；触发后不得再对该连接 write。
using CloseHandler = std::function<void(const ConnectionPtr&)>;
// 新连接接入。
using AcceptHandler = std::function<void(const ConnectionPtr&)>;
// 写队列把一条消息完整交给内核后触发。
using WriteDoneHandler = std::function<void()>;

using TimerId = std::uint64_t;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int fd() const { return fd_; }
    // "ip:port"，仅用于日志。
    const std::string& peer() const { return peer_; }
    bool closed() const { return closed_.load(std::memory_order_acquire); }

    // 待发送字节数（含未消费部分）。背压观测用。
    std::size_t pending_write_bytes() const;

    void set_read_handler(ReadHandler h) { read_handler_ = std::move(h); }
    void set_close_handler(CloseHandler h) { close_handler_ = std::move(h); }

    // 空闲超时（毫秒，0 = 不限）。超时后由 Reactor 主动关闭，close 回调会触发。
    void set_idle_timeout_ms(int ms) { idle_timeout_ms_ = ms; }
    int idle_timeout_ms() const { return idle_timeout_ms_; }

    // 追加一段待发送数据。线程安全。
    // 返回 false 表示连接已关闭或超出写队列上限（调用方应认为连接不可再用）。
    bool write(std::string bytes, WriteDoneHandler on_done = {});

    // 幂等关闭。线程安全（非 loop 线程时转投回 loop）。
    void close();

    // 当前 loop 线程内可用的"立即关闭"（不投递）；由 Reactor 回调使用。
    bool is_in_loop_thread() const;

private:
    friend class Reactor;

    Connection(Reactor* reactor, int fd, std::string peer);

    struct OutEntry {
        std::string bytes;
        std::size_t sent = 0;
        WriteDoneHandler on_done;
    };

    // 由 Reactor 在 loop 线程调用
    void touch();
    void handle_readable();
    void flush();
    void do_close();

    Reactor* reactor_ = nullptr;
    int fd_ = -1;
    std::string peer_;
    std::atomic<bool> closed_{false};

    ReadHandler read_handler_;
    CloseHandler close_handler_;

    mutable std::mutex write_mutex_;
    std::deque<OutEntry> out_queue_;
    std::size_t queued_bytes_ = 0;
    bool want_write_registered_ = false;

    bool close_handler_fired_ = false;
    std::chrono::steady_clock::time_point last_active_ = std::chrono::steady_clock::now();
    int idle_timeout_ms_ = 0;
};

// 事件循环。非线程安全的方法都只在 loop 线程调用，文档里逐条标注。
class Reactor {
public:
    struct Options {
        // 单连接写队列上限。超过即判定客户端不消费，主动断开（防内存放大）。
        std::size_t max_write_queue_bytes = 8u * 1024u * 1024u;
        // 空闲连接清扫间隔（毫秒）。
        int idle_sweep_interval_ms = 1000;
    };

    Reactor();
    explicit Reactor(Options options);
    ~Reactor();

    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    // 监听。host 为空串表示 INADDR_ANY。成功返回 true。仅 loop 启动前调用。
    bool listen(const std::string& host, std::uint16_t port, int backlog = 128);

    // 实际监听端口（port 传 0 时由内核分配，用于测试/多实例）。
    std::uint16_t local_port() const { return local_port_; }

    // 仅 loop 线程调用。
    void set_accept_handler(AcceptHandler h) { accept_handler_ = std::move(h); }

    // 阻塞运行直到 stop()。
    void run();
    // 只跑一轮（测试用）。
    void run_once(int timeout_ms);
    // 线程安全。
    void stop();

    // 把一个闭包投到 loop 线程执行。线程安全。
    void post(std::function<void()> fn);

    // 定时器。必须在 loop 线程调用（服务器在启动阶段注册，天然满足）。
    TimerId run_after(int delay_ms, std::function<void()> fn);
    TimerId run_every(int interval_ms, std::function<void()> fn);
    void cancel_timer(TimerId id);

    bool in_loop_thread() const;
    std::size_t connection_count() const;

    const Options& options() const { return options_; }
    const char* backend() const { return Poller::backend_name(); }

private:
    friend class Connection;

    struct TimerEntry {
        TimerId id = 0;
        int interval_ms = 0;  // 0 = 一次性
        std::function<void()> fn;
    };

    void register_connection(const ConnectionPtr& conn);
    void request_close(const ConnectionPtr& conn);
    void sweep_closed();
    void sweep_timers();
    void sweep_idle();
    void drain_posted();
    void accept_ready();
    void wakeup();
    void drain_wakeup();
    int next_timeout_ms() const;
    void schedule_flush(const ConnectionPtr& conn);

    Options options_;
    std::unique_ptr<Poller> poller_;
    int listen_fd_ = -1;
    std::uint16_t local_port_ = 0;
    AcceptHandler accept_handler_;

    std::unordered_map<int, ConnectionPtr> connections_;
    std::vector<int> closing_;

    // 定时器：按到期时间排序，multimap 允许同刻多枚。
    // 取消走迭代器索引（multimap 的键会重复，光存时间点定位不到具体那一枚）。
    using TimerMap = std::multimap<std::chrono::steady_clock::time_point, TimerEntry>;
    TimerMap timers_;
    TimerId next_timer_id_ = 1;
    std::unordered_map<TimerId, TimerMap::iterator> timer_index_;

    // 跨线程投递
    std::mutex posted_mutex_;
    std::deque<std::function<void()>> posted_;
    int wakeup_pipe_[2] = {-1, -1};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread::id loop_thread_id_{};
};

}  // namespace net

#endif  // NET_REACTOR_H
