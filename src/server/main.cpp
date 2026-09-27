// sim_server 入口。
//
// 启动序列：初始化数据层 → BattleService（建 worker 舰队 + 各自线程跑事件循环
// + 战斗线程池）→ 主线程阻塞等信号。
// 关闭序列：SIGINT/SIGTERM → 主线程从管道醒来 → service.stop()（停所有事件循环、
// join 线程、回收房间与战斗池）。
//
// 事件循环全部跑在 service 自己的线程上之后，main 不再需要"主线程跑 loop +
// 守候线程收信号"的结构 —— 主线程直接阻塞读信号管道就是最简单的等待方式。
// 仍然用自管道收信号：信号处理函数里只有 async-signal-safe 的调用能用，
// 直接调 service.stop()（内部有互斥锁与 join）是未定义行为。

#include <server/service.h>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/soul_mark_manager.h>
#include <entities/suit_manager.h>
#include <effects/effect.h>
#include <fsm/state.h>
#include <net/poller.h>
#include <server/output_json.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

int g_signal_pipe[2] = {-1, -1};

// 把三个插件注册表的已注册 id 全集 + 时点/异常对照表写成一行 JSON：
// {"moves":[...],"soul":[...],"suit":[...],
//  "states":[{"id":..,"name":".."}...],"anomalies":[{"id":..,"name":".."}...]}
// id 升序。网关拿注册表和数据库引用的 effect id 做差集（"未实现"徽标），
// 拿 states/anomalies 给前端当断点下拉与事件带中文化的对照表。
bool dump_registry_json(const std::string& path) {
    const std::vector<int> moves = EffectFactory::getInstance().registered_effect_ids();
    const std::vector<int> soul = SoulMarkManager::getInstance().registered_soulmark_ids();
    const std::vector<int> suit = SuitManager::getInstance().registered_suit_ids();

    auto join_ids = [](const std::vector<int>& ids) {
        std::string out;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            out += std::to_string(ids[i]);
        }
        return out;
    };

    // 时点：State 枚举 -1..40 全量（kLinearStateOrder 在匿名命名空间拿不到，
    // 而且它不含 FINISHED——对照表按枚举区间遍历最稳，state_name_cn 全覆盖）。
    auto escape = [](const char* text) {
        return server::json_escape(std::string(text == nullptr ? "" : text));
    };
    auto join_states = [&escape]() {
        std::string out;
        for (int id = -1; id <= 40; ++id) {
            if (id > -1) {
                out += ',';
            }
            out += "{\"id\":" + std::to_string(id) + ",\"name\":\"" +
                   escape(state_name_cn(static_cast<State>(id))) + "\"}";
        }
        return out;
    };

    // 异常状态：官方 id 0..kOfficialAbnormalStatusMaxId，附中文名。
    auto join_anomalies = [&escape]() {
        std::string out;
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (id > 0) {
                out += ',';
            }
            out += "{\"id\":" + std::to_string(id) + ",\"name\":\"" +
                   escape(abnormal_status_name_cn(static_cast<AbnormalStatusId>(id))) + "\"}";
        }
        return out;
    };

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << "{\"moves\":[" << join_ids(moves) << "],\"soul\":[" << join_ids(soul)
         << "],\"suit\":[" << join_ids(suit) << "],\"states\":[" << join_states()
         << "],\"anomalies\":[" << join_anomalies() << "]}";
    file.close();
    if (!file) {
        return false;
    }
    std::printf("[INFO] [server] 注册表已导出 %s（moves=%zu soul=%zu suit=%zu）\n",
                path.c_str(), moves.size(), soul.size(), suit.size());
    std::fflush(stdout);
    return true;
}

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
    int worker_count = 1;
    int acceptor_count = 1;
    // --dump-registry <path>：插件加载完成后把三个注册表（moves/soul/suit）的
    // 已注册 effect id 全集写成 JSON，然后继续正常启动。覆盖率对账数据源。
    // --dump-registry-exit <path>：同上，写完即退出（给网关按需刷新用）。
    std::string dump_registry;
    bool dump_and_exit = false;
    bool ok = true;
};

void print_usage(const char* argv0) {
    std::printf(
        "用法: %s [选项]\n"
        "  --host <addr>        监听地址（默认 127.0.0.1；对局域网开放要填 0.0.0.0）\n"
        "  --port <n>           监听端口（默认 4399）\n"
        "  --mode solo|pvp      solo=每连接一局、一个连接驱动双方（默认）；pvp=两连接一局\n"
        "  --threads <n>        战斗计算线程数（默认 4）\n"
        "  --workers <n>        事件循环线程数（默认 1）。新连接轮询分摊到各 worker\n"
        "  --acceptors <n>      参与监听的 worker 数（默认 1，≤ workers）。\n"
        "                       >1 时各 worker SO_REUSEPORT 同一端口；--port 0 时强制 1\n"
        "  --idle <ms>          空闲连接超时毫秒，0=不限（默认 0）\n"
        "  --dump-registry <path>\n"
        "                       插件加载后把已注册 effect id 写成 JSON，然后继续启动\n"
        "  --dump-registry-exit <path>\n"
        "                       同上，写完即退出（覆盖率对账按需刷新用）\n"
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
        } else if (flag == "--workers") {
            if (const char* v = next("--workers")) a.worker_count = std::atoi(v);
        } else if (flag == "--acceptors") {
            if (const char* v = next("--acceptors")) a.acceptor_count = std::atoi(v);
        } else if (flag == "--idle") {
            if (const char* v = next("--idle")) a.idle_timeout_ms = std::atoi(v);
        } else if (flag == "--dump-registry") {
            if (const char* v = next("--dump-registry")) a.dump_registry = v;
        } else if (flag == "--dump-registry-exit") {
            if (const char* v = next("--dump-registry-exit")) {
                a.dump_registry = v;
                a.dump_and_exit = true;
            }
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

    if (!args.dump_registry.empty()) {
        if (!dump_registry_json(args.dump_registry)) {
            std::fprintf(stderr, "注册表导出失败：%s\n", args.dump_registry.c_str());
            return 1;
        }
        if (args.dump_and_exit) {
            return 0;
        }
    }

    if (::pipe(g_signal_pipe) != 0) {
        std::fprintf(stderr, "创建信号管道失败: %s\n", std::strerror(errno));
        return 1;
    }
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    // 向已关闭的连接写数据不该打死服务（net 层已设 SO_NOSIGPIPE，这里再兜一层）。
    std::signal(SIGPIPE, SIG_IGN);

    server::BattleService::Options sopts;
    sopts.host = args.host;
    sopts.port = args.port;
    sopts.seat_count = args.seat_count;
    sopts.battle_threads = args.battle_threads;
    sopts.idle_timeout_ms = args.idle_timeout_ms;
    sopts.worker_count = args.worker_count;
    sopts.acceptor_count = args.acceptor_count;

    server::BattleService service(sopts);
    if (!service.start()) {
        return 1;
    }

    std::printf("[INFO] [server] 事件循环后端=%s，已就绪，Ctrl-C 退出\n", service.backend());
    std::fflush(stdout);

    // 主线程阻塞等信号（信号处理函数只写一个字节进来）。
    unsigned char sig = 0;
    for (;;) {
        const ssize_t n = ::read(g_signal_pipe[0], &sig, 1);
        if (n == 1) {
            break;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        break;  // 管道异常：也要走一遍优雅停机
    }

    std::printf("\n[INFO] [server] 收到信号 %d，正在停止…\n", static_cast<int>(sig));
    std::fflush(stdout);
    service.stop();
    ::close(g_signal_pipe[0]);
    ::close(g_signal_pipe[1]);

    std::printf("[INFO] [server] 已停止\n");
    return 0;
}
