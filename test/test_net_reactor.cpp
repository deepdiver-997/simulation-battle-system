// net 层验证：原生事件循环 + 长度前缀成帧。
//
// 为什么要有这个程序：网络层换掉 Boost.Asio 之后，正确性不能再靠"原来 asio 是对的"
// 背书。这里逐条钉住新实现最容易出错的地方：
//   1. 成帧器：逐字节喂入 / 一次多帧 / 长度非法必须拒绝
//   2. 多连接并发回显：64 连接 × 40 帧，内容逐字节比对
//   3. 大帧：3MB 单帧，强制走"读不满 / 写不完"的部分 IO 路径
//   4. close 回调恰好一次（回归最常见的重复关闭 / 漏关闭）
//   5. 背压：对端不读且写队列超限 → 服务端主动断开，而不是把内存吃光
//   6. 帧头声明超长 → 断连（流已不可再同步）
//   7. 定时器：run_every 持续触发、run_after 单次、cancel 生效
//   8. fd 不泄漏：全部连接关闭后 fd 数回到基线
//
// 客户端故意用阻塞 socket 手写（不走 net 层）：被测对象是服务端，客户端越"笨"
// 越能暴露服务端的问题，也不会出现"实现和测试同一个 bug 互相掩盖"。

#include <net/framing.h>
#include <net/reactor.h>

#include <dirent.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failed = 0;

void check(bool cond, const std::string& what) {
    std::printf("  [%s] %s\n", cond ? "ok  " : "FAIL", what.c_str());
    if (!cond) {
        ++g_failed;
    }
}

void section(const char* name) {
    std::printf("\n== %s ==\n", name);
}

template <class F>
bool wait_until(F pred, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

// 裸长度前缀帧（只测成帧器本身用：4 字节长度 + 裸 payload）。
std::string make_raw_frame(const std::string& payload) {
    const std::uint32_t total = static_cast<std::uint32_t>(4 + payload.size());
    std::string out(4, '\0');
    out[0] = static_cast<char>((total >> 24) & 0xff);
    out[1] = static_cast<char>((total >> 16) & 0xff);
    out[2] = static_cast<char>((total >> 8) & 0xff);
    out[3] = static_cast<char>(total & 0xff);
    out += payload;
    return out;
}

// 协议帧：与真实线上格式一致 —— [4B 大端 total][2B cmd][4B uuid][payload]。
// total 含前缀自身，所以最小帧长 10；测试里 cmd/uuid 用固定值占位。
std::string make_proto_frame(const std::string& payload) {
    const std::uint32_t total = static_cast<std::uint32_t>(10 + payload.size());
    std::string out(4, '\0');
    out[0] = static_cast<char>((total >> 24) & 0xff);
    out[1] = static_cast<char>((total >> 16) & 0xff);
    out[2] = static_cast<char>((total >> 8) & 0xff);
    out[3] = static_cast<char>(total & 0xff);
    out += '\x00';  // cmd 高字节
    out += '\x01';  // cmd 低字节
    out += std::string(4, '\0');  // uuid
    out += payload;
    return out;
}

std::size_t count_open_fds() {
    DIR* d = ::opendir("/dev/fd");
    if (d == nullptr) {
        return 0;
    }
    std::size_t n = 0;
    while (::readdir(d) != nullptr) {
        ++n;
    }
    ::closedir(d);
    return n;
}

// ------------------------------------------------------------ 成帧器单测

void test_framer() {
    section("成帧器");

    // 成帧器是通用件，单测里把下限定成 4 便于用裸帧直接验证长度数学。
    net::LengthPrefixedFramer framer(1024 * 1024, 4);

    // 一帧被切成很多片喂进来，必须只在最后一片吐出来
    {
        const std::string frame = make_raw_frame("hello");
        int hits = 0;
        std::string seen;
        bool ok = true;
        for (char c : frame) {
            ok = framer.feed(&c, 1, [&](const char* f, std::size_t n) {
                ++hits;
                seen.assign(f, n);
            });
        }
        check(ok && hits == 1 && seen == frame, "逐字节喂入 → 恰好吐出 1 帧且内容一致");
        check(framer.buffered() == 0, "消费完后缓冲为空");
    }

    // 一次喂入 3 帧（含粘包），必须吐 3 次
    {
        const std::string a = make_raw_frame("aaa");
        const std::string b = make_raw_frame("bbbbbbbbbb");
        const std::string c = make_raw_frame("c");
        std::vector<std::string> got;
        const std::string stream = a + b + c;
        const bool ok = framer.feed(stream.data(), stream.size(),
                                   [&](const char* f, std::size_t n) { got.emplace_back(f, n); });
        check(ok && got.size() == 3, "一次喂入 3 帧（粘包）→ 吐出 3 帧");
        check(got.size() == 3 && got[0] == a && got[1] == b && got[2] == c, "粘包拆分顺序与内容正确");
    }

    // 半包：尾帧不完整时必须留在缓冲里等后续字节
    {
        const std::string a = make_raw_frame("aaaaaaaaaaaa");
        const std::string b = make_raw_frame("bbbbbbbbbbbb");
        const std::string head = a + b.substr(0, 6);
        int hits = 0;
        framer.feed(head.data(), head.size(), [&](const char*, std::size_t) { ++hits; });
        check(hits == 1, "半包：只吐出完整的第 1 帧");
        check(framer.buffered() == 6, "半包：残留 6 字节留在缓冲");

        const std::string tail = b.substr(6);
        hits = 0;
        framer.feed(tail.data(), tail.size(), [&](const char*, std::size_t) { ++hits; });
        check(hits == 1 && framer.buffered() == 0, "补齐后吐出第 2 帧");
    }

    // 帧头声明超长 → 拒绝（否则就是内存放大攻击面）
    {
        net::LengthPrefixedFramer strict(64, 4);
        std::string evil(4, '\0');
        evil[0] = 0x7f;  // 声明 0x7f000000 字节
        const bool ok = strict.feed(evil.data(), evil.size(), [](const char*, std::size_t) {});
        check(!ok && !strict.ok(), "帧头声明超长 → feed 返回 false");
    }

    // 声明长度小于协议下限（10）→ 拒绝。这里手工造一个 total=8 的帧头：
    // 注意 make_proto_frame 不可能造出 10 以下的帧，所以必须裸写。
    {
        net::LengthPrefixedFramer strict(1024, 10);
        std::string bad(4, '\0');
        bad[3] = 8;  // total = 8 < 10
        bad += "abcd";
        const bool ok = strict.feed(bad.data(), bad.size(), [](const char*, std::size_t) {});
        check(!ok && !strict.ok(), "帧长小于协议下限 → feed 返回 false");
    }

    // 协议帧格式（10 字节头）能正常通过默认构造的成帧器
    {
        net::LengthPrefixedFramer proto;  // 默认 max 1MB / min 10
        const std::string frame = make_proto_frame("hello-world");
        int hits = 0;
        const bool ok = proto.feed(frame.data(), frame.size(), [&](const char*, std::size_t) { ++hits; });
        check(ok && hits == 1, "协议帧（10 字节头）通过默认成帧器");
    }
}

// --------------------------------------------------------------- 回显服务

class EchoServer {
public:
    struct Stats {
        std::atomic<int> accepted{0};
        std::atomic<int> closed{0};         // close 回调总触发次数
        std::atomic<int> close_events{0};   // 至少触发过一次 close 回调的连接数
        std::atomic<int> double_close{0};   // 触发第二次的连接数（必须恒为 0）
        std::atomic<int> frame_errors{0};
        std::atomic<int> flood_writes_rejected{0};
        std::atomic<std::size_t> echoed_bytes{0};
    };

    explicit EchoServer(net::Reactor& rx) : rx_(rx) {
        rx_.set_accept_handler([this](const net::ConnectionPtr& c) { on_accept(c); });
    }

    Stats& stats() { return stats_; }

private:
    struct Session {
        explicit Session(net::ConnectionPtr c) : conn(std::move(c)) {}
        net::ConnectionPtr conn;
        // 上限放到 16MB：本测试要过 3MB 的单帧（真实服务端不需要这么大）。
        net::LengthPrefixedFramer framer{16u << 20, 10};
        // 计数器挂在 Session 上，而不是按 Connection* 建索引：
        // 连接销毁后堆地址会被后来的连接复用，按裸指针计数会读到上一个连接的残留值，
        // 把"地址复用"误报成"重复关闭"。Session 每连接一份，天然不串。
        int close_hits = 0;
    };

    void on_accept(const net::ConnectionPtr& conn) {
        stats_.accepted.fetch_add(1);
        auto session = std::make_shared<Session>(conn);
        sessions_[conn.get()] = session;

        // 只捕获 weak：连接 → handler → session → 连接 会成环泄漏。
        std::weak_ptr<Session> weak = session;
        conn->set_read_handler([this, weak](const net::ConnectionPtr&, const char* data, std::size_t len) {
            if (auto s = weak.lock()) {
                on_read(s, data, len);
            }
        });
        conn->set_close_handler([this, weak](const net::ConnectionPtr& c) {
            stats_.closed.fetch_add(1);
            if (auto s = weak.lock()) {
                // close 回调只在 loop 线程跑，计数器无需加锁。
                if (++s->close_hits == 1) {
                    stats_.close_events.fetch_add(1);
                } else {
                    stats_.double_close.fetch_add(1);
                }
            }
            sessions_.erase(c.get());
        });
    }

    void on_read(const std::shared_ptr<Session>& s, const char* data, std::size_t len) {
        const bool ok = s->framer.feed(data, len, [this, &s](const char* frame, std::size_t flen) {
            handle_frame(s, frame, flen);
        });
        if (!ok) {
            stats_.frame_errors.fetch_add(1);
            s->conn->close();  // 流已失同步，只能断
        }
    }

    void handle_frame(const std::shared_ptr<Session>& s, const char* frame, std::size_t flen) {
        if (s->conn->closed()) {
            return;
        }
        const std::string payload(frame + 10, flen - 10);  // 跳过 4B 长度 + 2B cmd + 4B uuid

        if (payload == "CLOSE") {
            s->conn->close();
            return;
        }
        if (payload == "FLOOD") {
            // 对端声明不读，服务端持续推数据 → 期望写队列超限后自己断开。
            const std::string chunk(64 * 1024, 'x');
            for (int i = 0; i < 400; ++i) {
                if (!s->conn->write(chunk)) {
                    stats_.flood_writes_rejected.fetch_add(1);
                    return;
                }
            }
            return;
        }

        stats_.echoed_bytes.fetch_add(flen);
        s->conn->write(std::string(frame, flen));  // 原样回显整帧（含帧头）
    }

    net::Reactor& rx_;
    Stats stats_;
    std::map<const net::Connection*, std::shared_ptr<Session>> sessions_;
};

// --------------------------------------------------------------- 测试客户端

class Client {
public:
    Client() = default;
    ~Client() { close(); }

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect_to(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return false;
        }
        int one = 1;
        ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            close();
            return false;
        }
        return true;
    }

    bool send_all(const char* p, std::size_t n) {
        std::size_t off = 0;
        while (off < n) {
            const ssize_t w = ::send(fd_, p + off, n - off, 0);
            if (w > 0) {
                off += static_cast<std::size_t>(w);
                continue;
            }
            if (w < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        return true;
    }

    bool recv_exact(char* p, std::size_t n) {
        std::size_t off = 0;
        while (off < n) {
            const ssize_t r = ::recv(fd_, p + off, n - off, 0);
            if (r > 0) {
                off += static_cast<std::size_t>(r);
                continue;
            }
            if (r < 0 && errno == EINTR) {
                continue;
            }
            return false;  // 0 = 对端关闭，<0 = 错误
        }
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int fd() const { return fd_; }

private:
    int fd_ = -1;
};

// 每连接往返 40 帧，帧长覆盖小包/中包/跨 read 缓冲区的大包
void run_client_roundtrips(std::uint16_t port, int client_index, std::atomic<int>* mismatches) {
    Client c;
    if (!c.connect_to(port)) {
        mismatches->fetch_add(1);
        return;
    }

    const std::size_t sizes[] = {6, 100, 4096, 70000, 300000};
    for (int i = 0; i < 40; ++i) {
        const std::size_t body = sizes[(client_index + i) % 5];
        std::string payload(body, '\0');
        for (std::size_t k = 0; k < body; ++k) {
            payload[k] = static_cast<char>((client_index * 31 + i * 7 + static_cast<int>(k)) & 0x7f);
        }
        const std::string frame = make_proto_frame(payload);

        if (!c.send_all(frame.data(), frame.size())) {
            mismatches->fetch_add(1);
            return;
        }
        std::string echo(frame.size(), '\0');
        if (!c.recv_exact(&echo[0], echo.size())) {
            mismatches->fetch_add(1);
            return;
        }
        if (echo != frame) {
            mismatches->fetch_add(1);
            return;
        }
    }
}

}  // namespace

int main() {
    ::signal(SIGPIPE, SIG_IGN);  // 客户端故意会被服务端断开，别被信号打死

    test_framer();

    // 用默认 Options（写队列上限 8MB）：帧长测试要能发 3MB，背压测试灌 25MB 来触发。
    net::Reactor rx;

    std::printf("\n平台后端: %s\n", rx.backend());
    if (!rx.listen("127.0.0.1", 0)) {
        std::printf("listen 失败，无法继续\n");
        return 1;
    }
    const std::uint16_t port = rx.local_port();
    std::printf("监听端口: %u\n", static_cast<unsigned>(port));

    EchoServer server(rx);

    // fd 基线必须在 Reactor 建好（listen fd + 唤醒管道）之后再取，
    // 否则量到的是 Reactor 自身的常驻 fd，会被误判成泄漏。
    const std::size_t fds_before = count_open_fds();

    std::thread loop_thread([&rx] { rx.run(); });

    // ---------------------------------------------------------- 多连接并发回显
    section("64 连接 × 40 帧并发回显");
    {
        constexpr int kClients = 64;
        std::atomic<int> mismatches{0};
        std::vector<std::thread> threads;
        threads.reserve(kClients);
        for (int i = 0; i < kClients; ++i) {
            threads.emplace_back([port, i, &mismatches] { run_client_roundtrips(port, i, &mismatches); });
        }
        for (auto& t : threads) {
            t.join();
        }
        check(mismatches.load() == 0, "所有连接的所有帧逐字节一致（mismatch=0）");
        check(server.stats().accepted.load() == kClients,
              "服务端 accept 次数 = " + std::to_string(kClients));
    }

    // ---------------------------------------------------------- 大帧 / 部分 IO
    section("3MB 单帧（强制部分读写）");
    {
        Client c;
        check(c.connect_to(port), "连接建立");
        std::string payload(3 * 1024 * 1024, '\0');
        for (std::size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>((i * 131) & 0x7f);
        }
        const std::string frame = make_proto_frame(payload);
        check(c.send_all(frame.data(), frame.size()), "客户端发完 3MB");
        std::string echo(frame.size(), '\0');
        check(c.recv_exact(&echo[0], echo.size()), "客户端收完 3MB 回显");
        check(echo == frame, "3MB 回显内容一致（覆盖 EAGAIN 部分写 + 分包读）");
    }

    // ------------------------------------------------------------------ 优雅关闭
    section("close 回调恰好一次");
    {
        size_t closed_before = 0;
        {
            Client c;
            check(c.connect_to(port), "连接建立");
            // 快照放在本用例连接建立之后：上一个用例的客户端析构会异步触发它的服务端关闭，
            // 用全局 +1 判断会被那个"迟到的一次"干扰。
            closed_before = static_cast<size_t>(server.stats().closed.load());
            const std::string frame = make_proto_frame("CLOSE");
            check(c.send_all(frame.data(), frame.size()), "发送 CLOSE 指令");
            char dummy = 0;
            // 服务端关闭后，客户端读应当拿到 EOF
            check(::recv(c.fd(), &dummy, 1, 0) == 0, "服务端关闭 → 客户端读到 EOF");
            check(wait_until([&] { return server.stats().closed.load() > static_cast<int>(closed_before); }, 1000),
                  "服务端 close 回调已触发");
        }
        // 关键不变式：回调次数与"收到过回调的连接数"一一对应，且没有连接收到第二次。
        check(server.stats().double_close.load() == 0, "没有任何连接收到重复 close 回调");
        check(server.stats().close_events.load() == server.stats().closed.load(),
              "close 回调次数 == 收到回调的连接数（无重复、无遗漏）");
    }

    // -------------------------------------------------------------------- 背压
    section("写队列超限 → 服务端主动断开");
    {
        const int closed_before = server.stats().closed.load();
        Client c;
        check(c.connect_to(port), "连接建立");
        const std::string frame = make_proto_frame("FLOOD");
        check(c.send_all(frame.data(), frame.size()), "请求 FLOOD");
        // 客户端故意不读：服务端写队列积压超过上限（默认 8MB）后必须自己断开
        const bool rejected = wait_until(
            [&] { return server.stats().flood_writes_rejected.load() > 0; }, 3000);
        check(rejected, "服务端检测到写队列超限并拒绝继续写");
        const bool closed = wait_until([&] { return server.stats().closed.load() > closed_before; }, 2000);
        check(closed, "超限后连接被服务端断开");
    }

    // ---------------------------------------------------------- 非法帧长拒绝
    section("帧头声明超长 → 断连");
    {
        const int errors_before = server.stats().frame_errors.load();
        const int closed_before = server.stats().closed.load();
        Client c;
        check(c.connect_to(port), "连接建立");
        std::string evil(4, '\0');
        evil[0] = 0x7f;  // 2GB
        check(c.send_all(evil.data(), evil.size()), "发送非法帧头");
        const bool rejected = wait_until(
            [&] { return server.stats().frame_errors.load() == errors_before + 1; }, 1000);
        check(rejected, "成帧器拒绝非法帧长");
        const bool closed = wait_until([&] { return server.stats().closed.load() > closed_before; }, 1000);
        check(closed, "拒绝后连接被断开");
    }

    // -------------------------------------------------------------------- 定时器
    section("定时器");
    {
        std::atomic<int> every_ticks{0};
        std::atomic<int> after_fired{0};
        std::atomic<int> cancelled_fired{0};
        // run_every 的句柄：回调按引用捕获本块栈上计数器，块结束前必须注销。
        // 写在 loop 线程、读在主线程，走 atomic + 下面 posted marker 的投递序保证可见。
        std::atomic<net::TimerId> every_id{0};

        rx.post([&] {
            every_id.store(rx.run_every(20, [&] { every_ticks.fetch_add(1); }));
            rx.run_after(30, [&] { after_fired.fetch_add(1); });
            const net::TimerId id = rx.run_after(50, [&] { cancelled_fired.fetch_add(1); });
            rx.cancel_timer(id);
        });

        check(wait_until([&] { return after_fired.load() >= 1; }, 1000), "run_after 单次触发");
        check(wait_until([&] { return every_ticks.load() >= 5; }, 1000), "run_every 持续触发（≥5 次）");
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        check(cancelled_fired.load() == 0, "cancel_timer 后不再触发");

        // 周期定时器**必须**在计数器作用域结束前注销：run_every 不注销就会带着
        // [&] 悬垂引用每 20ms 触发一次，块结束后的后续 section 会被它写穿栈槽
        // （Release 下曾把 posted_mutex_ 踩成 EINVAL abort，2026-09-20）。
        // 用 posted marker 的投递序确认注销真的已在 loop 线程生效。
        std::atomic<bool> every_cancelled{false};
        rx.post([&] { rx.cancel_timer(every_id.load()); });
        rx.post([&] { every_cancelled.store(true); });
        check(wait_until([&] { return every_cancelled.load(); }, 1000), "run_every 已注销（posted 序）");
        // 打印留在本作用域内：计数器是栈对象，post 出去捕获引用会变悬垂读。
        std::printf("  [info] run_every 触发次数=%d（200ms 内，20ms 间隔，量级合理即正确）\n",
                    every_ticks.load());
    }

    // --------------------------------------------------------------- fd 不泄漏
    section("资源回收");
    {
        // 读 connection_count 必须走**投递序**：post 计数闭包，紧接着 post 一个
        // marker 闭包。loop 单线程 FIFO，marker 置位即"计数已做完"，读到的是确定值。
        //
        // 旧写法 post 后 sleep(5ms) 就读：loop 忙时 5ms 不够，主线程拿到初值 ~0u
        // 而误判"未清空"；且 wait_until 每 5ms 丢弃一个旧探针 shared_ptr，其析构
        // 与 loop 线程执行/拷贝并发——标准上是数据竞争（台账 2026-09-21 偶发
        // "连接表未清"+ 一次挂死正是它，归 netsrv 线修复）。
        auto live = std::make_shared<std::atomic<std::size_t>>(~0u);
        auto counted = std::make_shared<std::atomic<bool>>(false);
        rx.post([live, &rx] { live->store(rx.connection_count()); });
        rx.post([counted] { counted->store(true); });
        check(wait_until([&] { return counted->load(); }, 3000),
              "探针在 loop 线程执行完毕（posted 序）");
        check(live->load() == 0, "所有连接都已从 Reactor 表中移除");

        // fd 检查同理：等 loop 把上述投递全部消化完再量，不给在途关闭留窗口。
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const std::size_t fds_after = count_open_fds();
        const long delta = static_cast<long>(fds_after) - static_cast<long>(fds_before);
        check(delta == 0, "fd 数回到基线（delta=" + std::to_string(delta) + "）");
    }

    rx.stop();
    loop_thread.join();

    std::printf("\n%s（失败项 %d）\n", g_failed == 0 ? "全部通过" : "存在失败", g_failed);
    return g_failed == 0 ? 0 : 1;
}
