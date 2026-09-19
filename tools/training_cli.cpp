// sim_training_cli —— 命令行联调客户端（原生 socket，不依赖 Boost）。
//
// 定位：机制验证的主要入口。它把服务端吐的**事件带**翻译成人能读的推进过程，
// 所以能指着某一行说"这一步的伤害不对" —— 这比只看回合首尾两张快照有用得多。
//
// 输入线程 = stdin 命令；读取线程 = 收帧 + 打印 + （auto 模式下）自动应答待输入。
// 两者只共用 socket 的写侧（加锁），读侧只有读取线程碰。

#include <net/framing.h>
#include <server/protocol.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::mutex g_write_mutex;
int g_sock = -1;
std::atomic<bool> g_running{true};
std::atomic<bool> g_auto{false};

// 最近一条"待输入"载荷。auto 打开时要用它立刻应答 —— 否则若提示在 auto 打开**之前**
// 就到了，程序会一直等下一条提示，而对局正等着我们，双方互等（实测卡住的就是这一步）。
std::mutex g_last_req_mutex;
std::string g_last_req;
bool g_has_pending_req = false;

void log_line(const char* level, const std::string& msg) {
    std::printf("[%s] %s\n", level, msg.c_str());
    std::fflush(stdout);
}

bool send_frame(proto::Command cmd, uint32_t uuid, const std::string& payload) {
    const std::string frame = proto::build_frame(cmd, uuid, payload);
    std::lock_guard<std::mutex> lk(g_write_mutex);
    std::size_t off = 0;
    while (off < frame.size()) {
        const ssize_t n = ::send(g_sock, frame.data() + off, frame.size() - off, 0);
        if (n > 0) {
            off += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- 阵容构造

// 单只精灵的 JSON（服务端支持"一侧只给一个对象 = 这只重复 6 只"的简写）。
std::string pet_json(int pet_id, const std::vector<int>& skills, int trait_id = 0) {
    std::ostringstream oss;
    oss << "{\"petId\":" << pet_id << ",\"skills\":[";
    for (std::size_t i = 0; i < skills.size(); ++i) {
        if (i > 0) oss << ",";
        oss << skills[i];
    }
    oss << "],\"traitId\":" << trait_id << "}";
    return oss.str();
}

std::string lineup_json(int pet1, const std::vector<int>& sk1, int pet2,
                        const std::vector<int>& sk2) {
    return "{\"side1\":" + pet_json(pet1, sk1) + ",\"side2\":" + pet_json(pet2, sk2) + "}";
}

// 文档里的 init012 预设：双方都是 12 号精灵、不同技能组。
const char* kInit012 = nullptr;  // 由 build_init012() 惰性构造

std::string build_init012() {
    static const std::string cached = lineup_json(
        12, {10038, 10057, 10001, 10008, 10009}, 12, {10001, 10008, 10009, 10033, 20017});
    return cached;
}

// ---------------------------------------------------------------- 打印

std::string hp_pair(const nlohmann::json& j) {
    std::ostringstream oss;
    oss << j["hp"][0] << "/" << j["maxHp"][0] << " vs " << j["hp"][1] << "/" << j["maxHp"][1];
    return oss.str();
}

void print_tape(const nlohmann::json& doc) {
    if (!doc.contains("samples")) {
        return;
    }
    const auto& samples = doc["samples"];
    std::printf("── 事件带 seq %d→%d（%zu 个时点）\n", doc.value("from", 0), doc.value("to", 0),
                samples.size());
    for (const auto& s : samples) {
        std::ostringstream line;
        line << "  #" << s.value("seq", 0) << " r" << s.value("round", 0) << " "
             << s.value("stateName", std::string("?")) << "(" << s.value("state", 0) << ")"
             << " hp[" << hp_pair(s) << "]";

        // 等级只在非零时打，避免每行都刷一堆 0。
        bool any_level = false;
        for (int side = 0; side < 2; ++side) {
            for (const auto& lv : s["levels"][side]) {
                if (lv.get<int>() != 0) {
                    any_level = true;
                }
            }
        }
        if (any_level) {
            line << " 等级[" << s["levels"][0].dump() << "|" << s["levels"][1].dump() << "]";
        }
        if (s.value("pendingDamage", 0) != 0 || s.value("resolvedDamage", 0) != 0) {
            line << " 伤害(pending=" << s.value("pendingDamage", 0)
                 << ",resolved=" << s.value("resolvedDamage", 0) << ")";
        }

        for (const auto& ev : s["events"]) {
            line << "  [" << ev.value("type", std::string("?")) << " a" << ev.value("actor", -1)
                 << "→t" << ev.value("target", -1) << " 量=" << ev.value("amount", 0);
            if (ev.contains("slot")) {
                line << " 槽=" << ev["slot"].get<int>();
            }
            line << "]";
        }
        std::printf("%s\n", line.str().c_str());
    }
    std::fflush(stdout);
}

// 自动应答：挑第一个 usable 的技能（换宠局面挑第一个 usable 的精灵）。
void auto_respond(const nlohmann::json& doc) {
    const int player = doc.value("player", -1);
    if (player < 0 || !doc.contains("legal")) {
        return;
    }
    const auto& legal = doc["legal"];
    const bool must_choose_pet = legal.value("mustChoosePet", false);

    if (!must_choose_pet) {
        if (legal.contains("skills")) {
            for (const auto& sk : legal["skills"]) {
                if (sk.value("usable", false)) {
                    std::printf("  [auto] 玩家%d 用技能 %d(%s)\n", player,
                                sk.value("index", 0), sk.value("name", std::string("?")).c_str());
                    send_frame(proto::Command::SELECT_SKILL, 0,
                               proto::build_action_payload(
                                   player, static_cast<int>(proto::ActionType::SELECT_SKILL),
                                   sk.value("index", 0)));
                    return;
                }
            }
        }
        log_line("WARN", "auto: 没有可用技能");
        return;
    }

    if (legal.contains("pets")) {
        for (const auto& p : legal["pets"]) {
            if (p.value("usable", false) && !p.value("onStage", false)) {
                std::printf("  [auto] 玩家%d 换宠槽 %d(%s)\n", player, p.value("slot", 0),
                            p.value("name", std::string("?")).c_str());
                send_frame(proto::Command::CHOOSE_PET, 0,
                           proto::build_action_payload(
                               player, static_cast<int>(proto::ActionType::CHOOSE_PET),
                               p.value("slot", 0)));
                return;
            }
        }
    }
    log_line("WARN", "auto: 没有可换的精灵");
}

void print_input_required(const nlohmann::json& doc, const std::string& raw_payload) {
    {
        std::lock_guard<std::mutex> lk(g_last_req_mutex);
        g_last_req = raw_payload;
        g_has_pending_req = true;
    }
    const int player = doc.value("player", -1);
    std::printf(">>> 等待输入：玩家 %d（%s，回合 %d，时点 %s）\n", player,
                doc.value("reason", std::string("?")).c_str(), doc.value("round", 0),
                doc.value("stateName", std::string("?")).c_str());

    if (!doc.contains("legal")) {
        return;
    }
    const auto& legal = doc["legal"];
    if (legal.contains("skills")) {
        std::printf("    技能: ");
        for (const auto& sk : legal["skills"]) {
            std::printf("[%d]%s(pp=%d)%s ", sk.value("index", 0),
                        sk.value("name", std::string("?")).c_str(), sk.value("pp", 0),
                        sk.value("usable", false) ? "" : "(不可用)");
        }
        std::printf("\n");
    }
    if (legal.value("mustChoosePet", false) && legal.contains("pets")) {
        std::printf("    换宠: ");
        for (const auto& p : legal["pets"]) {
            std::printf("[%d]%s(%d/%d)%s%s ", p.value("slot", 0),
                        p.value("name", std::string("?")).c_str(), p.value("hp", 0),
                        p.value("maxHp", 0), p.value("usable", false) ? "" : "(不可用)",
                        p.value("onStage", false) ? "(在场)" : "");
        }
        std::printf("\n");
    }
    std::fflush(stdout);

    if (g_auto.load()) {
        auto_respond(doc);
    }
}

// auto 刚打开时，如果已经有一条待输入还挂着，立刻应答它。
void respond_to_pending_input() {
    std::string raw;
    {
        std::lock_guard<std::mutex> lk(g_last_req_mutex);
        if (!g_has_pending_req) {
            return;
        }
        raw = g_last_req;
    }
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(raw);
    } catch (const std::exception&) {
        return;
    }
    auto_respond(doc);
}

void handle_frame(const char* frame, std::size_t len) {
    proto::Header h;
    if (!proto::parse_header(frame, len, h)) {
        return;
    }
    const std::string payload(frame + proto::kHeaderSize, len - proto::kHeaderSize);

    // ERROR 是纯文本（FSM 的错误文案），不当 JSON 解析。
    if (h.command == proto::Command::ERROR) {
        log_line("ERROR", payload);
        return;
    }

    nlohmann::json doc;
    if (!payload.empty()) {
        try {
            doc = nlohmann::json::parse(payload);
        } catch (const std::exception& ex) {
            std::printf("[%s] (非 JSON 载荷) %s\n", proto::command_name(h.command), payload.c_str());
            return;
        }
    }

    switch (h.command) {
        case proto::Command::ROOM_INFO:
            std::printf("[房间] match=%d seat=%d seats=%d mode=%s\n", doc.value("match", 0),
                        doc.value("seat", 0), doc.value("seats", 0),
                        doc.value("mode", std::string("?")).c_str());
            break;
        case proto::Command::TAPE:
            print_tape(doc);
            break;
        case proto::Command::INPUT_REQUIRED:
            print_input_required(doc, payload);
            break;
        case proto::Command::BATTLE_OVER:
            std::printf("═══ 战斗结束：winner=%d（回合 %d，存活 %s）\n", doc.value("winner", -1),
                        doc.value("round", 0), doc["alive"].dump().c_str());
            g_auto.store(false);
            break;
        case proto::Command::SYNC_STATE:
            std::printf("[快照] %s\n", payload.c_str());
            break;
        case proto::Command::DEBUG_FULLSTATE:
            std::printf("[完整状态] %s\n", payload.c_str());
            break;
        case proto::Command::LEGAL_ACTIONS:
            std::printf("[合法动作] %s\n", payload.c_str());
            break;
        default:
            std::printf("[%s] %s\n", proto::command_name(h.command), payload.c_str());
            break;
    }
    std::fflush(stdout);
}

void reader_loop() {
    net::LengthPrefixedFramer framer(1u << 20, proto::kHeaderSize);
    char buf[64 * 1024];
    for (;;) {
        const ssize_t n = ::recv(g_sock, buf, sizeof(buf), 0);
        if (n > 0) {
            const bool ok = framer.feed(buf, static_cast<std::size_t>(n),
                                       [](const char* f, std::size_t l) { handle_frame(f, l); });
            if (!ok) {
                log_line("ERROR", "服务端发来非法帧长的数据，断开");
                g_running.store(false);
                return;
            }
            continue;
        }
        if (n == 0) {
            log_line("INFO", "服务端关闭了连接");
            g_running.store(false);
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (!g_running.load()) {
            return;
        }
        log_line("ERROR", std::string("recv 失败: ") + std::strerror(errno));
        g_running.store(false);
        return;
    }
}

void print_help() {
    std::printf(
        "命令：\n"
        "  init012                          用文档预设（双方 12 号精灵、不同技能组）开局\n"
        "  lineup <petId> <s1..s5> [petId2 <t1..t5>]  自定义阵容（solo：只给一只则双方相同）\n"
        "  party <petId> <s1..s5>           pvp 模式：提交自己这一边的阵容\n"
        "  trait <traitId>                  下一次 lineup 带的通用特性 id\n"
        "  turn <a0> <i0> <a1> <i1>         一次提交双方动作（a = skill|pet|medicine）\n"
        "  skill <robot> <idx>              提交单个技能操作\n"
        "  pet <robot> <slot>               提交换宠\n"
        "  medicine <robot> <idx>           提交药剂\n"
        "  auto [on|off]                    自动应答等待输入（默认关）\n"
        "  sync | status                   请求快照 / 看服务端状态\n"
        "  full                             完整状态转储（含效果表）\n"
        "  step | run                       单步 / 连续\n"
        "  break <stateId>                  toggle 断点\n"
        "  heartbeat\n"
        "  quit\n");
}

int action_type_of(const std::string& a) {
    if (a == "skill") return static_cast<int>(proto::ActionType::SELECT_SKILL);
    if (a == "pet") return static_cast<int>(proto::ActionType::CHOOSE_PET);
    if (a == "medicine") return static_cast<int>(proto::ActionType::USE_MEDICINE);
    return -1;
}

}  // namespace

int main(int argc, char** argv) {
    ::signal(SIGPIPE, SIG_IGN);

    std::string host = "127.0.0.1";
    int port = 4399;
    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::atoi(argv[2]);

    g_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock < 0) {
        log_line("ERROR", "socket() 失败");
        return 1;
    }
    int one = 1;
    ::setsockopt(g_sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    if (::connect(g_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        log_line("ERROR", "连接 " + host + ":" + std::to_string(port) + " 失败: " +
                              std::strerror(errno));
        return 1;
    }
    log_line("INFO", "已连接 " + host + ":" + std::to_string(port));
    print_help();

    std::thread reader(reader_loop);

    std::string line;
    int pending_trait = 0;
    while (g_running.load() && std::cout << "cli> " && std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) {
            continue;
        }
        if (cmd == "quit" || cmd == "exit") {
            break;
        }
        if (cmd == "help") {
            print_help();
            continue;
        }
        if (cmd == "auto") {
            std::string v;
            if (iss >> v) {
                g_auto.store(v == "on" || v == "1" || v == "true");
            } else {
                g_auto.store(!g_auto.load());
            }
            log_line("INFO", std::string("auto = ") + (g_auto.load() ? "on" : "off"));
            if (g_auto.load()) {
                respond_to_pending_input();
            }
            continue;
        }
        if (cmd == "trait") {
            int t = 0;
            if (iss >> t) {
                pending_trait = t;
                log_line("INFO", "下一份阵容带通用特性 id=" + std::to_string(t));
            }
            continue;
        }
        if (cmd == "init012") {
            send_frame(proto::Command::INIT_BATTLE_JSON, 1, build_init012());
            log_line("SEND", "init012");
            continue;
        }
        if (cmd == "party") {
            // pvp 模式：提交"自己这一边"的阵容。服务端把它落到自己座位对应的边。
            int p1 = 0;
            std::vector<int> s1(5, 0);
            if (!(iss >> p1)) {
                log_line("WARN", "用法: party <petId> <s1..s5>");
                continue;
            }
            for (int i = 0; i < 5; ++i) {
                if (!(iss >> s1[static_cast<std::size_t>(i)])) {
                    log_line("WARN", "需要 5 个技能 id");
                    break;
                }
            }
            send_frame(proto::Command::INIT_BATTLE_JSON, 1,
                       "{\"party\":" + pet_json(p1, s1, pending_trait) + "}");
            pending_trait = 0;
            log_line("SEND", "party（pvp 单边阵容）");
            continue;
        }
        if (cmd == "lineup") {
            int p1 = 0;
            std::vector<int> s1(5, 0);
            if (!(iss >> p1)) {
                log_line("WARN", "用法: lineup <petId> <s1..s5> [petId2 <t1..t5>]");
                continue;
            }
            for (int i = 0; i < 5; ++i) {
                if (!(iss >> s1[static_cast<std::size_t>(i)])) {
                    log_line("WARN", "需要 5 个技能 id");
                    break;
                }
            }
            int p2 = 0;
            std::vector<int> s2;
            if (iss >> p2) {
                s2.assign(5, 0);
                for (int i = 0; i < 5; ++i) {
                    if (!(iss >> s2[static_cast<std::size_t>(i)])) break;
                }
            } else {
                p2 = p1;
                s2 = s1;
            }
            const std::string body = "{\"side1\":" + pet_json(p1, s1, pending_trait) +
                                     ",\"side2\":" + pet_json(p2, s2, pending_trait) + "}";
            pending_trait = 0;
            send_frame(proto::Command::INIT_BATTLE_JSON, 1, body);
            log_line("SEND", "lineup");
            continue;
        }

        if (cmd == "turn") {
            std::string a0, a1;
            int i0 = 0, i1 = 0;
            if (!(iss >> a0 >> i0 >> a1 >> i1)) {
                log_line("WARN", "用法: turn <a0> <i0> <a1> <i1>（a = skill|pet|medicine）");
                continue;
            }
            const int t0 = action_type_of(a0);
            const int t1 = action_type_of(a1);
            if (t0 < 0 || t1 < 0) {
                log_line("WARN", "动作只能是 skill|pet|medicine");
                continue;
            }
            send_frame(proto::Command::SELECT_SKILL, 1, proto::build_action_payload(0, t0, i0));
            send_frame(proto::Command::SELECT_SKILL, 1, proto::build_action_payload(1, t1, i1));
            log_line("SEND", "turn " + a0 + " " + std::to_string(i0) + " / " + a1 + " " +
                                 std::to_string(i1));
            continue;
        }
        if (cmd == "skill" || cmd == "pet" || cmd == "medicine") {
            int robot = 0, idx = 0;
            if (!(iss >> robot >> idx)) {
                log_line("WARN", "用法: " + cmd + " <robot> <idx>");
                continue;
            }
            send_frame(proto::Command::SELECT_SKILL, 1,
                       proto::build_action_payload(robot, action_type_of(cmd), idx));
            log_line("SEND", cmd + " " + std::to_string(robot) + " " + std::to_string(idx));
            continue;
        }
        if (cmd == "sync" || cmd == "status") {
            send_frame(proto::Command::SYNC_STATE, 1, "");
            continue;
        }
        if (cmd == "full") {
            send_frame(proto::Command::DEBUG_FULLSTATE, 1, "");
            continue;
        }
        if (cmd == "step") {
            send_frame(proto::Command::DEBUG_STEP, 1, "");
            continue;
        }
        if (cmd == "run") {
            send_frame(proto::Command::DEBUG_CONTINUE, 1, "");
            continue;
        }
        if (cmd == "break") {
            int state = 0;
            if (!(iss >> state)) {
                log_line("WARN", "用法: break <stateId>");
                continue;
            }
            std::string body(4, '\0');
            proto::write_u32_be(&body[0], static_cast<uint32_t>(state));
            send_frame(proto::Command::DEBUG_BREAKPOINT, 1, body);
            log_line("SEND", "break " + std::to_string(state));
            continue;
        }
        if (cmd == "heartbeat") {
            send_frame(proto::Command::HEARTBEAT, 1, "");
            continue;
        }
        log_line("WARN", "未知命令，输入 help 查看");
    }

    g_running.store(false);
    ::shutdown(g_sock, SHUT_RDWR);
    ::close(g_sock);
    if (reader.joinable()) {
        reader.join();
    }
    return 0;
}
