// sim_server 入口。
//
// 启动序列：初始化数据层 → 起 reactor（kqueue/epoll）→ 起 BattleService → 跑事件循环。
// 关闭序列：SIGINT/SIGTERM → 守候线程停 reactor → 主线程退出循环 → 停 BattleService。
//
// 为什么用自管道收信号：信号处理函数里只有 async-signal-safe 的调用能用，
// 直接在里面调 reactor.stop()（内部有互斥锁与管道写）是未定义行为。
// 惯用做法是"信号处理函数只 write 一个字节"，再让一个普通线程 read 它。

#include <net/reactor.h>
#include <server/service.h>

#include <entities/pet_factory.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

int g_signal_pipe[2] = {-1, -1};

void signal_handler(int signo) {
    const unsigned char byte = static_cast<unsigned char>(signo);
    ssize_t n = 0;
    do {
        n = ::write(g_signal_pipe[1], &byte, 1);
    } while (n < 0 && errno == EINTR);
}

struct CliArgs {
    std::string host = "127.0.0.1";
    std::uint16_t port = 4399;
    int seat_count = 1;
    int battle_threads = 4;
    int idle_timeout_ms = 0;
    bool ok = true;
};

void print_usage(const char* argv0) {
    std::printf(
        "用法: %s [选项]\n"
        "  --host <addr>        监听地址（默认 127.0.0.1；对局域网开放要填 0.0.0.0）\n"
        "  --port <n>           监听端口（默认 4399）\n"
        "  --mode solo|pvp      solo=每连接一局、一个连接驱动双方（默认）；pvp=两连接一局\n"
        "  --threads <n>        战斗计算线程数（默认 4）\n"
        "  --idle <ms>          空闲连接超时毫秒，0=不限（默认 0）\n"
        "  --help\n",
        argv0);
}

CliArgs parse_args(int argc, char** argv) {
    CliArgs a;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s 需要一个参数\n", what);
                a.ok = false;
                return nullptr;
            }
            return argv[++i];
        };

        if (flag == "--help" || flag == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (flag == "--host") {
            if (const char* v = next("--host")) a.host = v;
        } else if (flag == "--port") {
            if (const char* v = next("--port")) a.port = static_cast<std::uint16_t>(std::atoi(v));
        } else if (flag == "--mode") {
            if (const char* v = next("--mode")) {
                const std::string m = v;
                if (m == "solo") {
                    a.seat_count = 1;
                } else if (m == "pvp") {
                    a.seat_count = 2;
                } else {
                    std::fprintf(stderr, "--mode 只能是 solo 或 pvp\n");
                    a.ok = false;
                }
            }
        } else if (flag == "--threads") {
            if (const char* v = next("--threads")) a.battle_threads = std::atoi(v);
        } else if (flag == "--idle") {
            if (const char* v = next("--idle")) a.idle_timeout_ms = std::atoi(v);
        } else {
            std::fprintf(stderr, "未知选项: %s\n", flag.c_str());
            print_usage(argv[0]);
            a.ok = false;
        }
    }
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    const CliArgs args = parse_args(argc, argv);
    if (!args.ok) {
        return 2;
    }

    if (!PetFactory::initialize_runtime_data()) {
        std::fprintf(stderr,
                     "初始化运行期数据失败：检查 scripts/data/processed/seer_unity.sqlite 与 "
                     "resources/<lib>/liblib_1.dylib 是否存在，且当前目录是仓库根\n");
        return 1;
    }

    if (::pipe(g_signal_pipe) != 0) {
        std::fprintf(stderr, "创建信号管道失败: %s\n", std::strerror(errno));
        return 1;
    }
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    // 向已关闭的连接写数据不该打死服务（net 层已设 SO_NOSIGPIPE，这里再兜一层）。
    std::signal(SIGPIPE, SIG_IGN);

    net::Reactor::Options ropts;
    ropts.idle_sweep_interval_ms = 1000;
    net::Reactor reactor(ropts);

    server::BattleService::Options sopts;
    sopts.host = args.host;
    sopts.port = args.port;
    sopts.seat_count = args.seat_count;
    sopts.battle_threads = args.battle_threads;
    sopts.idle_timeout_ms = args.idle_timeout_ms;

    server::BattleService service(reactor, sopts);
    if (!service.start()) {
        return 1;
    }

    std::printf("[INFO] [server] 事件循环后端=%s，已就绪，Ctrl-C 退出\n", reactor.backend());
    std::fflush(stdout);

    // 收信号的守候线程：读到字节就停 reactor。
    std::thread signal_thread([&reactor] {
        unsigned char byte = 0;
        for (;;) {
            const ssize_t n = ::read(g_signal_pipe[0], &byte, 1);
            if (n == 1) {
                std::printf("\n[INFO] [server] 收到信号 %d，正在停止…\n", static_cast<int>(byte));
                std::fflush(stdout);
                reactor.stop();
                return;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            return;  // 管道被关，退出
        }
    });

    reactor.run();  // 主线程跑事件循环，直到被停

    // 先停服务（会 drain 战斗线程池、回收房间），再让守候线程退出。
    service.stop();
    ::close(g_signal_pipe[1]);
    if (signal_thread.joinable()) {
        signal_thread.join();
    }
    ::close(g_signal_pipe[0]);

    std::printf("[INFO] [server] 已停止\n");
    return 0;
}
