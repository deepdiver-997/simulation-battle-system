#include <server/service.h>

#include <net/reactor.h>
#include <server/room.h>
#include <server/session.h>
#include <thread_pool/native_thread_pool.h>

#include <algorithm>
#include <cstdio>

namespace server {
namespace {

void log_svc(const char* level, const std::string& msg) {
    std::printf("[%s] [service] %s\n", level, msg.c_str());
    std::fflush(stdout);
}

}  // namespace

BattleService::BattleService(net::Reactor& reactor, Options options)
    : reactor_(reactor), options_(std::move(options)) {}

BattleService::~BattleService() {
    stop();
}

bool BattleService::start() {
    if (options_.seat_count != 1 && options_.seat_count != 2) {
        log_svc("ERROR", "seat_count must be 1 (solo) or 2 (pvp)");
        return false;
    }

    battle_pool_ = std::make_shared<NativeThreadPool>(
        static_cast<std::size_t>(options_.battle_threads > 0 ? options_.battle_threads : 1));
    battle_pool_->start();

    // 新连接：建会话、挂到连接上。会话的所有权在 BattleService 里
    // （Room 只持 weak_ptr），这样一条连接断开时对局侧不需要操心对象生命周期。
    reactor_.set_accept_handler([this](const net::ConnectionPtr& conn) {
        // 会话 id 用连接 fd 反查不稳（fd 会复用），这里直接用一个自增号。
        static std::atomic<int> next_session_id{1};
        auto session = std::make_shared<Session>(next_session_id.fetch_add(1), conn, *this);

        {
            std::lock_guard<std::mutex> lk(mu_);
            sessions_.push_back(session);
        }

        // 先挂回调再让连接开始收数据：反过来的话第一批字节可能没有消费者。
        std::weak_ptr<Session> weak = session;
        conn->set_read_handler([weak](const net::ConnectionPtr&, const char* data, std::size_t len) {
            if (auto s = weak.lock()) {
                s->on_read(data, len);
            }
        });
        conn->set_close_handler([weak, this](const net::ConnectionPtr&) {
            if (auto s = weak.lock()) {
                s->on_close();
                on_session_closed(s);
            }
        });

        log_svc("INFO", "session " + std::to_string(session->id()) + " connected from " +
                            conn->peer());
    });

    if (!reactor_.listen(options_.host, options_.port, options_.backlog)) {
        log_svc("ERROR", "listen failed on " + options_.host + ":" + std::to_string(options_.port));
        return false;
    }

    log_svc("INFO", "listening on " + options_.host + ":" +
                        std::to_string(reactor_.local_port()) +
                        (options_.seat_count == 1 ? " (solo: 每连接一局，一个连接驱动双方)"
                                                  : " (pvp: 两连接一局)"));
    return true;
}

void BattleService::stop() {
    std::vector<RoomPtr> rooms;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& kv : rooms_) {
            rooms.push_back(kv.second);
        }
        rooms_.clear();
        sessions_.clear();
    }
    rooms.clear();  // Room 析构会释放 BattleContext

    if (battle_pool_) {
        battle_pool_->stop(true);
        battle_pool_.reset();
    }
}

int BattleService::next_match_id() {
    std::lock_guard<std::mutex> lk(mu_);
    return next_match_id_++;
}

std::size_t BattleService::room_count() const {
    std::lock_guard<std::mutex> lk(mu_);
    return rooms_.size();
}

std::size_t BattleService::session_count() const {
    std::lock_guard<std::mutex> lk(mu_);
    return sessions_.size();
}

RoomPtr BattleService::assign_room(const SessionPtr& session, std::string& err) {
    err.clear();
    if (!session) {
        err = "null session";
        return nullptr;
    }

    std::lock_guard<std::mutex> lk(mu_);

    if (options_.seat_count == 2) {
        // pvp：找一个还有空座、还没开局的房间坐进去。
        for (auto& kv : rooms_) {
            RoomPtr room = kv.second;
            if (room->battle_started()) {
                continue;
            }
            const int seat = room->attached_seat_count();
            if (seat >= room->seat_count()) {
                continue;
            }
            if (!room->attach_seat(seat, session)) {
                continue;
            }
            session->bind_room(room, seat);
            log_svc("INFO", "session " + std::to_string(session->id()) + " joined match " +
                                std::to_string(room->match_id()) + " as seat " +
                                std::to_string(seat));
            return room;
        }
    }

    // solo，或 pvp 没找到可坐的房间 → 新建。
    const int match_id = next_match_id_++;
    auto room = std::make_shared<Room>(match_id, battle_pool_, options_.seat_count);
    rooms_[match_id] = room;
    if (!room->attach_seat(0, session)) {
        err = "failed to attach seat 0";
        rooms_.erase(match_id);
        return nullptr;
    }
    session->bind_room(room, 0);
    log_svc("INFO", "session " + std::to_string(session->id()) + " created match " +
                        std::to_string(match_id) + " seats=" + std::to_string(options_.seat_count));
    return room;
}

void BattleService::on_room_empty(int match_id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(match_id);
    if (it == rooms_.end()) {
        return;
    }
    // 一个座位都不剩了 → 回收房间与它的 BattleContext。
    // 这是"连接断开后对局不会泄漏"的唯一出口。
    rooms_.erase(it);
    log_svc("INFO", "match " + std::to_string(match_id) + " recycled (no seats left)");
}

void BattleService::on_session_closed(const SessionPtr& session) {
    if (!session) {
        return;
    }

    // 先把会话从表里摘掉（会话的所有权在这里），再腾座位。
    // 顺序不能反：腾座位可能触发房间回收，那之后 room 的 weak_ptr 就该失效了。
    {
        std::lock_guard<std::mutex> lk(mu_);
        sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), session), sessions_.end());
    }

    RoomPtr room = session->room();
    const int seat = session->seat();
    if (room && seat >= 0) {
        room->detach_seat(seat);
    }

    // 一个座位都不剩 → 回收房间与它的 BattleContext。
    // 这是"连接断开后对局不泄漏"的唯一出口。
    if (room && !room->has_any_seat()) {
        on_room_empty(room->match_id());
    }

    log_svc("INFO", "session " + std::to_string(session->id()) + " closed");
}

}  // namespace server
