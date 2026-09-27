#ifndef SERVER_SERVICE_H
#define SERVER_SERVICE_H

// BattleService = 事件循环舰队 + 连接接入 + 房间表 + 撮合 + 战斗线程池。
//
// 房间形态由 seat_count 决定（见 docs/01-架构与设计/网络层与服务端形状.md）：
//   seat_count == 1 → solo：每条新连接**独占**一个新房间，一个连接同时驱动双方操作。
//                     这是开放给玩家验证时的主力形态 —— 一个人就能复现问题，
//                     不需要撮合、房间号、断线重连。
//   seat_count == 2 → pvp：新连接先等一个"缺人的房间"，凑够两座才开局。
//                     加的是撮合层，对局层一行不改。
//
// 多 worker（多 reactor）形态：
//   worker_count 个 Reactor 各占一条线程；前 acceptor_count 个开 accept 模式
//   （调 listen，可 SO_REUSEPORT 各自 bind 同一端口）。新连接由 accept 回调里的
//   **轮询计数器**分给某个 worker（经 Reactor::adopt_connection 移交）—— 内核对
//   多监听 fd 的分配不保证均衡，均衡在应用层补齐。acceptor 自己也参与轮询。
//   worker_count == 1（默认）时行为与单 reactor 完全一致。
//
// 所有权：
//   连接 → 所属 net::Reactor（连接表）
//   会话 → BattleService（sessions_）
//   房间 → BattleService（rooms_）
//   Room 持 Session 的 weak_ptr；Session 持 Room 的 weak_ptr。都不成环。
//   会话创建发生在**属主 reactor 的线程**（adopt 之后的 accept 回调里），
//   sessions_/rooms_ 由 mu_ 保护，跨线程访问安全。

#include <net/reactor.h>
#include <server/protocol.h>
#include <thread_pool/thread_pool_base.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace server {

class Room;
class Session;

using RoomPtr = std::shared_ptr<Room>;
using SessionPtr = std::shared_ptr<Session>;

class BattleService {
public:
    struct Options {
        std::string host = "127.0.0.1";
        std::uint16_t port = 4399;
        int seat_count = 1;      // 1 = solo（默认，验证期主力）; 2 = pvp
        int battle_threads = 4;  // 战斗计算线程数
        int backlog = 128;
        int worker_count = 1;    // reactor 数量（每条线程一个）。多 worker 时轮询分摊连接
        int acceptor_count = 1;  // 开 accept 模式的 reactor 数量（≤ worker_count）。
                                 // >1 需要多 bind 同一端口（SO_REUSEPORT）；
                                 // port=0（内核随机分配）时强制回 1，否则各拿各的端口。
        // 单连接写队列上限（字节）。对端不消费就断开，防内存放大。
        std::size_t max_write_queue_bytes = 8u * 1024u * 1024u;
        // 空闲连接超时（毫秒）；0 = 不限。玩家验证期建议给一个值，回收挂死连接。
        int idle_timeout_ms = 0;
    };

    explicit BattleService(Options options);
    ~BattleService();

    BattleService(const BattleService&) = delete;
    BattleService& operator=(const BattleService&) = delete;

    // 建 worker 舰队与战斗线程池，开始监听并启动所有事件循环线程。
    // 失败返回 false（端口占用等）。
    bool start();
    // 停所有事件循环（join 线程）、清会话与房间、停战斗线程池。幂等。
    void stop();

    // ── 被 Session 调用 ──
    // 给这条连接安排房间与座位。solo 每次新建；pvp 可能并入等位中的房间。
    // 返回 nullptr 时 err 说明原因。
    RoomPtr assign_room(const SessionPtr& session, std::string& err);
    void on_session_closed(const SessionPtr& session);

    // ── 被 Room 调用 ──
    void on_room_empty(int match_id);

    int seat_count() const { return options_.seat_count; }
    std::size_t room_count() const;
    std::size_t session_count() const;

    // 实际监听端口（port 传 0 时由内核分配）。
    std::uint16_t local_port() const { return local_port_; }
    const char* backend() const;

private:
    int next_match_id();
    void on_accept(const net::ConnectionPtr& conn);

    Options options_;

    // worker 舰队：service 持有全部 reactor 与它们的 loop 线程。
    std::vector<std::unique_ptr<net::Reactor>> reactors_;
    std::vector<std::thread> loop_threads_;
    std::uint16_t local_port_ = 0;

    std::shared_ptr<ThreadPoolBase> battle_pool_;

    mutable std::mutex mu_;
    std::unordered_map<int, RoomPtr> rooms_;
    std::vector<SessionPtr> sessions_;
    int next_match_id_ = 1;
};

}  // namespace server

#endif  // SERVER_SERVICE_H
