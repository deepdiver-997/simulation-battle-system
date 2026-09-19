#ifndef SERVER_SERVICE_H
#define SERVER_SERVICE_H

// BattleService = 连接接入 + 房间表 + 撮合 + 战斗线程池。
//
// 房间形态由 seat_count 决定（见 docs/01-架构与设计/网络层与服务端形状.md）：
//   seat_count == 1 → solo：每条新连接**独占**一个新房间，一个连接同时驱动双方操作。
//                     这是开放给玩家验证时的主力形态 —— 一个人就能复现问题，
//                     不需要撮合、房间号、断线重连。
//   seat_count == 2 → pvp：新连接先等一个"缺人的房间"，凑够两座才开局。
//                     加的是撮合层，对局层一行不改。
//
// 所有权：
//   连接 → net::Reactor（连接表）
//   会话 → BattleService（sessions_）
//   房间 → BattleService（rooms_）
//   Room 持 Session 的 weak_ptr；Session 持 Room 的 weak_ptr。都不成环。

#include <net/reactor.h>
#include <server/protocol.h>
#include <thread_pool/thread_pool_base.h>

#include <atomic>
#include <cstdint>
#include <map>
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
        // 单连接写队列上限（字节）。对端不消费就断开，防内存放大。
        std::size_t max_write_queue_bytes = 8u * 1024u * 1024u;
        // 空闲连接超时（毫秒）；0 = 不限。玩家验证期建议给一个值，回收挂死连接。
        int idle_timeout_ms = 0;
    };

    BattleService(net::Reactor& reactor, Options options);
    ~BattleService();

    BattleService(const BattleService&) = delete;
    BattleService& operator=(const BattleService&) = delete;

    // 建战斗线程池并开始监听。失败返回 false（端口占用等）。
    bool start();
    // 停线程池、清房间。不负责停 reactor（那是 main 的事）。
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

    const net::Reactor& reactor() const { return reactor_; }

private:
    int next_match_id();

    net::Reactor& reactor_;
    Options options_;
    std::shared_ptr<ThreadPoolBase> battle_pool_;

    mutable std::mutex mu_;
    std::unordered_map<int, RoomPtr> rooms_;
    std::vector<SessionPtr> sessions_;
    int next_match_id_ = 1;
};

}  // namespace server

#endif  // SERVER_SERVICE_H
