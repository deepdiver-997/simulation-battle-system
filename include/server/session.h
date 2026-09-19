#ifndef SERVER_SESSION_H
#define SERVER_SESSION_H

// Session = 一条连接 = 一个座位（"单连接单对局"）。
//
// 职责只有三件：分帧、把命令派发给 Room、把 Room 的输出写回连接。
// 它**不做**对局调度，也不做 uuid 路由 —— 一个 Session 只服务一个 Room 的一个座位，
// 所以没有"这条消息属于哪个对局"的问题。
//
// 与 Room 的引用关系刻意做成双向 weak：
//   Session → weak_ptr<Room>（对局可能先于连接被回收）
//   Room    → weak_ptr<Session>（连接断开时对局可能还活着，比如 pvp 的另一边）
// 两边的强引用分别由 BattleService（房间表）与 net::Reactor（连接表）持有，不成环。

#include <net/framing.h>
#include <net/reactor.h>
#include <server/protocol.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace server {

class Session;
class Room;
class BattleService;

using SessionPtr = std::shared_ptr<Session>;
using RoomPtr = std::shared_ptr<Room>;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(int id, const net::ConnectionPtr& conn, BattleService& service);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    int id() const { return id_; }
    int seat() const { return seat_; }
    bool in_room() const;
    RoomPtr room() const;

    // 绑定到房间/座位。seat < 0 表示未入座。
    void bind_room(const RoomPtr& room, int seat);

    // 线程安全：内部投递到 reactor 线程再写。
    void send_frame(const std::string& frame);

    // 由 net 层回调（reactor 线程）。
    void on_read(const char* data, std::size_t len);
    void on_close();

private:
    void dispatch(proto::Command cmd, std::uint32_t uuid, const std::string& payload);
    void handle_init_battle(std::uint32_t uuid, const std::string& payload, bool json_form);
    void handle_action(std::uint32_t uuid, const std::string& payload);
    void send_error(const std::string& text);
    void send_payload(proto::Command cmd, const std::string& payload);
    bool ensure_room(std::string& err);

    const int id_;
    net::ConnectionPtr conn_;
    BattleService& service_;

    net::LengthPrefixedFramer framer_;
    std::weak_ptr<Room> room_;
    int seat_ = -1;
};

}  // namespace server

#endif  // SERVER_SESSION_H
