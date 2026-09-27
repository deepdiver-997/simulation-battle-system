#include <server/service.h>

#include <net/poller.h>
#include <net/reactor.h>
#include <server/room.h>
#include <server/session.h>
#include <thread_pool/native_thread_pool.h>

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <thread>

namespace server {
namespace {

void log_svc(const char* level, const std::string& msg) {
    std::printf("[%s] [service] %s\n", level, msg.c_str());
    std::fflush(stdout);
}

}  // namespace

BattleService::BattleService(Options options) : options_(std::move(options)) {}

BattleService::~BattleService() {
    stop();
}

bool BattleService::start() {
    if (options_.seat_count != 1 && options_.seat_count != 2) {
        log_svc("ERROR", "seat_count must be 1 (solo) or 2 (pvp)");
        return false;
    }

    int workers = std::max(1, options_.worker_count);
    int acceptors = std::max(1, std::min(options_.acceptor_count, workers));
    // port=0（内核随机分配）时多个 acceptor 会各拿各的端口，监听就散了 —— 强制回 1。
    if (options_.port == 0 && acceptors > 1) {
        log_svc("WARN", "port=0 with multiple acceptors would listen on random ports; "
                        "forcing acceptor_count=1");
        acceptors = 1;
    }

    battle_pool_ = std::make_shared<NativeThreadPool>(
        static_cast<std::size_t>(options_.battle_threads > 0 ? options_.battle_threads : 1));
    battle_pool_->start();

    // ── worker 舰队 ──
    for (int i = 0; i < workers; ++i) {
        net::Reactor::Options ropts;
        ropts.idle_sweep_interval_ms = 1000;
        reactors_.emplace_back(new net::Reactor(ropts));
    }

    // 轮询路由：所有 worker（含 acceptor 自己）共享一个 atomic 计数器，连接均匀摊开。
    // 内核对多监听 fd 的唤醒分配按四元组哈希、不保证均衡，均衡在这里补齐。
    // 捕获裸指针是安全的：reactors_ 由本对象持有，路由只在各 reactor 线程跑，
    // 而 stop() 会先 join 全部 loop 线程再析构 reactors_。
    std::vector<net::Reactor*> ring;
    ring.reserve(reactors_.size());
    for (auto& r : reactors_) {
        ring.push_back(r.get());
    }
    auto counter = std::make_shared<std::atomic<std::uint64_t>>(0);
    for (auto& r : reactors_) {
        r->set_connection_router([ring, counter]() -> net::Reactor* {
            return ring[counter->fetch_add(1, std::memory_order_relaxed) % ring.size()];
        });
    }

    // 新连接：建会话、挂到连接上。这个回调在**属主 reactor 的线程**触发
    // （adopt 之后），sessions_ 由 mu_ 保护所以跨 worker 安全。
    // 会话的所有权在 BattleService 里（Room 只持 weak_ptr），
    // 连接断开时对局侧不需要操心对象生命周期。
    for (auto& r : reactors_) {
        r->set_accept_handler([this](const net::ConnectionPtr& conn) { on_accept(conn); });
    }

    // ── accept 开关：前 acceptors 个 reactor 真正监听 ──
    for (int i = 0; i < acceptors; ++i) {
        if (!reactors_[static_cast<std::size_t>(i)]->listen(options_.host, options_.port,
                                                            options_.backlog)) {
            log_svc("ERROR", "listen failed on " + options_.host + ":" +
                                 std::to_string(options_.port));
            return false;
        }
    }
    local_port_ = reactors_[0]->local_port();

    // ── 启动所有事件循环线程 ──
    for (auto& r : reactors_) {
        net::Reactor* rr = r.get();
        loop_threads_.emplace_back([rr] { rr->run(); });
    }

    log_svc("INFO", "listening on " + options_.host + ":" + std::to_string(local_port_) + " (" +
                        std::to_string(workers) + " workers, " + std::to_string(acceptors) +
                        " acceptors, " +
                        (options_.seat_count == 1 ? "solo: 每连接一局，一个连接驱动双方)"
                                                  : "pvp: 两连接一局)"));
    return true;
}

void BattleService::on_accept(const net::ConnectionPtr& conn) {
    // 会话 id 用连接 fd 反查不稳（fd 会复用），这里直接用一个自增号。
    static std::atomic<int> next_session_id{1};
    auto session = std::make_shared<Session>(next_session_id.fetch_add(1), conn, *this);

    {
        std::lock_guard<std::mutex> lk(mu_);
        sessions_.push_back(session);
    }

    // 先挂回调再让连接开始收数据：反过来的话第一批字节可能没有消费者。
    // （adopt 的顺序保证在这里依然成立：回调挂好后，fd 才会在下一次 wait 进入内核。）
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

    log_svc("INFO", "session " + std::to_string(session->id()) + " connected from " + conn->peer());
}

void BattleService::stop() {
    // 先停事件循环（停止接受新事件），join 全部 loop 线程，再回收会话与房间。
    // 顺序不能反：会话/房间的析构路径会经 Connection::close() 投递回 loop 线程，
    // loop 还活着时析构它们会留下"投给已死 reactor"的任务。
    for (auto& r : reactors_) {
        r->stop();
    }
    for (auto& t : loop_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    loop_threads_.clear();

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

    reactors_.clear();  // 关闭剩余连接的 fd
}

int BattleService::next_match_id() {
    std::lock_guard<std::mutex> lk(mu_);
    return next_match_id_++;
}

const char* BattleService::backend() const {
    return net::Poller::backend_name();
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
