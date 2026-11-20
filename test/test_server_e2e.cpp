// 服务端端到端测试：真的起一个 server（reactor + BattleService），真的用 socket 打一局。
//
// 为什么需要它（而不是只测各层）：手工联调一次性暴露过两个只有端到端才会现形的问题 ——
//   1. 线格式大端被直接当本机序交给 FSM（m_buffer 是 memcpy 成 int[] 读的），
//      小端机上 action_type=1 变成 16777216 → 所有动作被判"rejected"；
//   2. 房间被回收后，队列里还排着持有裸 BattleContext* 的推进任务 → 锁已销毁的互斥量。
// 两者都是"各层单测全绿、合起来不能用"。所以这里断言的是**可观测的业务结果**：
// 发一个技能之后，事件带里必须出现体力变化（动作真生效了），而不是"服务端没报错"。
//
// 依赖：必须在仓库根运行（与场景测试同）—— 相对路径加载
// scripts/data/processed/seer_unity.sqlite 与 resources/<lib>/liblib_1.dylib。

#include <net/framing.h>
#include <server/protocol.h>

#include <entities/pet_factory.h>
#include <net/reactor.h>
#include <server/service.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
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

// ------------------------------------------------------------ 测试客户端

struct Frame {
    proto::Command command;
    std::string payload;
};

class TestClient {
public:
    ~TestClient() { close_socket(); }

    bool connect_to(std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return false;
        }
        int one = 1;
        ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        return ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    }

    bool send(proto::Command cmd, std::uint32_t uuid, const std::string& payload) {
        const std::string frame = proto::build_frame(cmd, uuid, payload);
        std::size_t off = 0;
        while (off < frame.size()) {
            const ssize_t n = ::send(fd_, frame.data() + off, frame.size() - off, 0);
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

    // 读到 pred 成立为止（或超时）。返回 pred 最终是否成立。
    template <class Pred>
    bool pump_until(Pred pred, int timeout_ms) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        if (pred()) {
            return true;
        }
        char buf[64 * 1024];
        while (std::chrono::steady_clock::now() < deadline) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd_, &rfds);
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 50 * 1000;
            const int ready = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
            if (ready > 0) {
                const ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
                if (n > 0) {
                    const bool ok = framer_.feed(buf, static_cast<std::size_t>(n),
                                                [this](const char* f, std::size_t l) {
                                                    proto::Header h;
                                                    if (proto::parse_header(f, l, h)) {
                                                        frames_.push_back(Frame{
                                                            h.command,
                                                            std::string(f + proto::kHeaderSize,
                                                                        l - proto::kHeaderSize)});
                                                    }
                                                });
                    if (!ok) {
                        bad_frame_ = true;
                        return false;
                    }
                } else if (n == 0) {
                    closed_ = true;
                    return pred();
                }
            }
            if (pred()) {
                return true;
            }
        }
        return pred();
    }

    // 取最后一条某类型消息的载荷。
    const std::string* last_payload(proto::Command cmd) const {
        for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
            if (it->command == cmd) {
                return &it->payload;
            }
        }
        return nullptr;
    }

    int count(proto::Command cmd) const {
        int n = 0;
        for (const auto& f : frames_) {
            if (f.command == cmd) {
                ++n;
            }
        }
        return n;
    }

    bool bad_frame() const { return bad_frame_; }
    bool closed() const { return closed_; }
    const std::vector<Frame>& frames() const { return frames_; }

    // 关闭连接（触发服务端 close 回调）。TestClient 持有 fd 且不可拷贝，
    // 所以"断开"必须显式调用，不能靠赋值一个空对象。
    void disconnect() { close_socket(); }

    // 按到达顺序取下一条"需要客户端反应"的消息，跳过其它类型。
    //
    // 必须用游标按序消费，不能"每次取最后一条 INPUT_REQUIRED"：最后一条是**旧**的，
    // 会让自动对打反复基于同一条过期提示重复提交动作 —— 实测症状是打不完（回合数堆到
    // 几十却毫无进展），而它看起来像服务端的问题，其实是测试自己的错。
    // 只查询"游标之后有没有待反应的新消息"，不消费。给 pump_until 的谓词用。
    bool has_pending_trigger(std::size_t cursor) const {
        for (std::size_t i = cursor; i < frames_.size(); ++i) {
            if (frames_[i].command == proto::Command::INPUT_REQUIRED ||
                frames_[i].command == proto::Command::BATTLE_OVER) {
                return true;
            }
        }
        return false;
    }

    bool take_next_trigger(std::size_t& cursor, proto::Command& cmd, std::string& payload) {
        while (cursor < frames_.size()) {
            const Frame& f = frames_[cursor];
            ++cursor;
            if (f.command == proto::Command::INPUT_REQUIRED ||
                f.command == proto::Command::BATTLE_OVER) {
                cmd = f.command;
                payload = f.payload;
                return true;
            }
        }
        return false;
    }

private:
    void close_socket() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int fd_ = -1;
    net::LengthPrefixedFramer framer_{1u << 20, proto::kHeaderSize};
    std::vector<Frame> frames_;
    bool bad_frame_ = false;
    bool closed_ = false;
};

// 文档里的 init012 预设阵容（双方都是 12 号精灵、不同技能组）。
std::string init012_lineup() {
    return "{\"side1\":{\"petId\":12,\"skills\":[10038,10057,10001,10008,10009]},"
           "\"side2\":{\"petId\":12,\"skills\":[10001,10008,10009,10033,20017]}}";
}

// 从 INPUT_REQUIRED 载荷里挑一个合法动作并提交。返回 false = 没得可选。
bool auto_play_one(TestClient& c, const std::string& input_required_payload) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(input_required_payload);
    } catch (const std::exception&) {
        return false;
    }
    const int player = doc.value("player", -1);
    if (player < 0 || !doc.contains("legal")) {
        return false;
    }
    const auto& legal = doc["legal"];

    if (legal.value("mustChoosePet", false)) {
        for (const auto& p : legal["pets"]) {
            if (p.value("usable", false) && !p.value("onStage", false)) {
                return c.send(proto::Command::CHOOSE_PET, 1,
                              proto::build_action_payload(
                                  player, static_cast<int>(proto::ActionType::CHOOSE_PET),
                                  p.value("slot", 0)));
            }
        }
        return false;
    }
    for (const auto& sk : legal["skills"]) {
        if (sk.value("usable", false)) {
            return c.send(proto::Command::SELECT_SKILL, 1,
                          proto::build_action_payload(
                              player, static_cast<int>(proto::ActionType::SELECT_SKILL),
                              sk.value("index", 0)));
        }
    }
    return false;
}

// 事件带里双方的总体力（用来判断"有没有真的发生变化"）。
bool tape_hp(const std::string& tape_payload, int& hp0, int& hp1) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(tape_payload);
    } catch (const std::exception&) {
        return false;
    }
    if (!doc.contains("samples") || doc["samples"].empty()) {
        return false;
    }
    const auto& last = doc["samples"].back();
    hp0 = last["hp"][0].get<int>();
    hp1 = last["hp"][1].get<int>();
    return true;
}

}  // namespace

int main() {
    ::signal(SIGPIPE, SIG_IGN);

    if (!PetFactory::initialize_runtime_data()) {
        std::printf("初始化运行期数据失败（要在仓库根运行）\n");
        return 1;
    }

    server::BattleService::Options sopts;
    sopts.host = "127.0.0.1";
    sopts.port = 0;  // 由内核分配，避免与手工起的实例撞端口
    sopts.seat_count = 1;   // solo
    sopts.battle_threads = 2;
    // 多 worker：事件循环舰队 + 连接轮询分发 + adopt 移交路径全部在真实流量下被走到。
    // 两个连接大概率落在不同 worker 上，pvp 语义的跨 reactor 写路径也因此被覆盖。
    sopts.worker_count = 2;
    sopts.acceptor_count = 1;  // 单监听 + 轮询分发（port=0 时也只能是 1）

    server::BattleService service(sopts);
    if (!service.start()) {
        std::printf("服务端启动失败\n");
        return 1;
    }
    const std::uint16_t port = service.local_port();
    std::printf("\n服务端已起，端口 %u（solo，2 workers）\n", static_cast<unsigned>(port));

    // 事件循环线程由 service 自己管理 —— 测试直接当客户端用。

    // ---------------------------------------------------------- 开局与事件带
    std::printf("\n== 开局：房间信息 + 事件带 + 待输入 ==\n");
    TestClient c;
    {
        check(c.connect_to(port), "客户端连接成功");
        check(c.send(proto::Command::INIT_BATTLE_JSON, 1, init012_lineup()), "发送 JSON 阵容");

        check(c.pump_until([&] { return c.last_payload(proto::Command::INPUT_REQUIRED) != nullptr; },
                           5000),
              "收到 INPUT_REQUIRED（开局推进到选择期）");
        check(!c.bad_frame(), "全程没有非法帧（成帧器未拒绝过数据）");

        const std::string* room = c.last_payload(proto::Command::ROOM_INFO);
        check(room != nullptr, "收到 ROOM_INFO");
        if (room != nullptr) {
            const auto doc = nlohmann::json::parse(*room);
            check(doc.value("seats", 0) == 1 && doc.value("mode", std::string()) == "solo",
                  "ROOM_INFO 标明 solo / 1 座位");
        }

        const std::string* tape = c.last_payload(proto::Command::TAPE);
        check(tape != nullptr, "收到 TAPE（开局推进的事件带）");
        if (tape != nullptr) {
            const auto doc = nlohmann::json::parse(*tape);
            check(doc.value("from", -1) == 1, "事件带序号从 1 开始");
            check(doc["samples"].size() >= 2, "开局事件带含多个时点（GAME_START 起）");
            const auto& first = doc["samples"].front();
            check(first.value("state", 999) == -1, "首个时点是 GAME_START(-1)");
        }

        const std::string* req = c.last_payload(proto::Command::INPUT_REQUIRED);
        if (req != nullptr) {
            const auto doc = nlohmann::json::parse(*req);
            check(doc.value("player", -1) == 0, "开局先等玩家 0 操作");
            check(doc.contains("snapshot"), "INPUT_REQUIRED 内嵌权威快照");
            int usable = 0;
            for (const auto& sk : doc["legal"]["skills"]) {
                if (sk.value("usable", false)) {
                    ++usable;
                }
            }
            check(usable > 0, "开局合法动作集里有可用技能");
        }
    }

    // -------------------------------------------------- 关键：动作真的生效了吗
    std::printf("\n== 提交技能后，事件带里必须出现体力变化 ==\n");
    int hp0_before = 0;
    int hp1_before = 0;
    {
        const std::string* tape = c.last_payload(proto::Command::TAPE);
        check(tape != nullptr && tape_hp(*tape, hp0_before, hp1_before), "取到开局末态体力");

        const std::size_t tape_count_before = static_cast<std::size_t>(c.count(proto::Command::TAPE));
        // robot=0 用技能 0。这条断言专门守字节序：若 m_buffer 的字节序写错，
        // 动作会被 FSM 判成非法，体力不会变。
        check(c.send(proto::Command::SELECT_SKILL, 1,
                     proto::build_action_payload(0, static_cast<int>(proto::ActionType::SELECT_SKILL), 0)),
              "提交玩家0 技能0");

        check(c.pump_until([&] { return c.count(proto::Command::TAPE) > static_cast<int>(tape_count_before); },
                           5000),
              "收到新的事件带");
        check(c.last_payload(proto::Command::CMD_ERROR) == nullptr, "动作没有被拒绝（无 ERROR）");

        // 等两个玩家都提交完（solo 下轮流提示），跑完一个回合。
        {
            std::size_t cursor = 0;
            proto::Command trig = proto::Command::INVALID;
            std::string payload;
            for (int i = 0; i < 6; ++i) {
                if (!c.pump_until([&] { return c.has_pending_trigger(cursor); }, 3000)) {
                    break;
                }
                if (!c.take_next_trigger(cursor, trig, payload)) {
                    break;
                }
                if (trig == proto::Command::BATTLE_OVER) {
                    break;
                }
                if (!auto_play_one(c, payload)) {
                    break;
                }
            }
        }

        int hp0_after = 0;
        int hp1_after = 0;
        const std::string* tape_after = c.last_payload(proto::Command::TAPE);
        check(tape_after != nullptr && tape_hp(*tape_after, hp0_after, hp1_after), "取到回合后体力");
        check(hp0_after != hp0_before || hp1_after != hp1_before,
              "体力确实变了（" + std::to_string(hp0_before) + "/" + std::to_string(hp1_before) +
                  " → " + std::to_string(hp0_after) + "/" + std::to_string(hp1_after) +
                  "）—— 证明动作真的进了 FSM");
    }

    // -------------------------------------------------------------- 非法输入
    std::printf("\n== 非法输入要被拒并给出原因 ==\n");
    {
        const int err_before = c.count(proto::Command::CMD_ERROR);
        // index 越界（技能只有 5 个）
        check(c.send(proto::Command::SELECT_SKILL, 1,
                     proto::build_action_payload(0, static_cast<int>(proto::ActionType::SELECT_SKILL), 99)),
              "发送越界技能下标");
        check(c.pump_until([&] { return c.count(proto::Command::CMD_ERROR) > err_before; }, 3000),
              "收到 ERROR");
        const std::string* err = c.last_payload(proto::Command::CMD_ERROR);
        check(err != nullptr && !err->empty(), "ERROR 带非空原因");
    }
    {
        // solo 下座位 0 能驱动双方，但如果声称操作玩家 2（不存在）必须被拒
        const int err_before = c.count(proto::Command::CMD_ERROR);
        check(c.send(proto::Command::SELECT_SKILL, 1,
                     proto::build_action_payload(7, static_cast<int>(proto::ActionType::SELECT_SKILL), 0)),
              "发送 robot=7（越界玩家）");
        check(c.pump_until([&] { return c.count(proto::Command::CMD_ERROR) > err_before; }, 3000),
              "越界玩家被拒");
    }

    // ------------------------------------------------------ 一整局跑到结束
    std::printf("\n== 自动对打到 BATTLE_OVER ==\n");
    {
        TestClient b;
        check(b.connect_to(port), "第二个连接（solo 下是另一局）");
        check(b.send(proto::Command::INIT_BATTLE_JSON, 1, init012_lineup()), "发送阵容");

        bool finished = false;
        int turns = 0;
        constexpr int kMaxTurns = 400;  // 一步 = 一条待输入或一条结束
        std::size_t cursor = 0;
        for (; turns < kMaxTurns; ++turns) {
            // 等到有"需要反应的新消息"（游标之后）
            const bool got = b.pump_until([&] { return b.has_pending_trigger(cursor); }, 5000);
            if (!got) {
                break;
            }
            proto::Command trig = proto::Command::INVALID;
            std::string payload;
            if (!b.take_next_trigger(cursor, trig, payload)) {
                break;
            }
            if (trig == proto::Command::BATTLE_OVER) {
                finished = true;
                break;
            }
            if (!auto_play_one(b, payload)) {
                // 自动应答没得可选 = 该玩家已经没有合法动作。打印出来，
                // 免得把"引擎没结束对局"和"测试不会应答"混成一个失败。
                std::printf("  [info] 自动应答无可用动作，停在第 %d 步。载荷：\n%s\n", turns,
                            payload.c_str());
                break;
            }
        }

        // ⚠️ 注意这条为什么能过：引擎**没有**"一方精灵全灭 → 结算败北"的判定，
        //    打到全灭会卡在"死后选择"要求换一个不存在的精灵。服务端在
        //    on_fsm_paused 里检测"等着输入但一个合法动作都没有"并兜底结束
        //    （reason=engine_no_legal_action，服务端日志同时打 ERROR）。
        //    也就是说这里验证的是**服务端不把玩家挂死**，不是"引擎会正常结束对局"。
        //    引擎侧的正确修法属于领域语义（败北判定口径），另立任务。
        check(finished, "自动对打抵达 BATTLE_OVER（回合数 " + std::to_string(turns) + "）");
        check(!b.bad_frame(), "整局没有非法帧");

        const std::string* over = b.last_payload(proto::Command::BATTLE_OVER);
        if (over != nullptr) {
            const auto doc = nlohmann::json::parse(*over);
            const int winner = doc.value("winner", -99);
            const std::string reason = doc.value("reason", std::string("?"));
            check(winner == 0 || winner == 1, "BATTLE_OVER 给出胜者（winner=" +
                                                  std::to_string(winner) + "）");
            check(doc.contains("snapshot"), "BATTLE_OVER 内嵌最终快照");
            check(reason == "normal" || reason == "engine_no_legal_action",
                  "BATTLE_OVER 说明结束原因（reason=" + reason + "）");
            if (reason == "engine_no_legal_action") {
                std::printf("  [note] 本局由服务端兜底结束：引擎在一方全灭后卡在「死后选择」。\n");
            }
            // 败者（全灭方）应当就是判负方
            const int alive0 = doc["alive"][0].get<int>();
            const int alive1 = doc["alive"][1].get<int>();
            check((alive0 == 0) != (alive1 == 0), "结束时恰有一方精灵全灭（alive=" +
                                                      std::to_string(alive0) + "/" +
                                                      std::to_string(alive1) + "）");
        }
        // 事件带序号应当全局单调递增（客户端据此判断有没有漏收）
        int last_seq = 0;
        bool monotonic = true;
        for (const auto& f : b.frames()) {
            if (f.command != proto::Command::TAPE) {
                continue;
            }
            const auto doc = nlohmann::json::parse(f.payload);
            if (doc.value("from", 0) != last_seq + 1) {
                monotonic = false;
            }
            last_seq = doc.value("to", 0);
        }
        check(monotonic, "事件带序号连续无缺口（客户端可据此发现漏收）");
        check(last_seq > 0, "整局事件带非空（共 " + std::to_string(last_seq) + " 个时序采样）");
    }

    // ---------------------------------------------------------------- 收尾
    std::printf("\n== 连接断开后对局被回收（无泄漏）==\n");
    {
        // 显式断开（而不是等析构）：验证"连接断开 → 座位腾空 → 房间回收"这条路径。
        // TestClient 不可拷贝（持有 fd），所以只能这样收尾。
        c.disconnect();
    }
    bool recycled = false;
    for (int i = 0; i < 40; ++i) {
        if (service.room_count() == 0) {
            recycled = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    check(recycled, "所有连接断开后房间被回收（room_count=" +
                        std::to_string(service.room_count()) + "）");

    service.stop();  // 停所有 worker 的事件循环、join 线程、回收房间与战斗池

    std::printf("\n%s（失败项 %d）\n", g_failed == 0 ? "全部通过" : "存在失败", g_failed);
    return g_failed == 0 ? 0 : 1;
}
