#ifndef SERVER_ROOM_H
#define SERVER_ROOM_H

// Room = 一场对局。**同时就是 FSM 的 IControlBlock 实现**。
//
// 为什么是这种形状（而不是"一个连接一个控制块"）：
//   - 对局的推进、等待输入、输出序列化都发生在 Room 里，与"有几个连接"无关；
//   - 单一连接驱动双方（solo）和双人各占一座（pvp）**只在座位数和"谁能动哪个玩家"
//     上不同**，其余代码完全共用。加 pvp 时加的是撮合层，不是重写对局层；
//   - 于是"单 socket 多 uuid 多对局"那套 waiting_uuid_ / pending_inputs_ / 按 uuid
//     路由的结构全部消失 —— 那套复杂度只服务于"一个连接里塞多局"，本次不需要。
//
// 线程模型：
//   - FSM 在 battle_pool 线程上跑（wait_for_input / async_write / on_fsm_paused 都在该线程）；
//   - 客户端命令在 reactor 线程上到达（submit_action 等）；
//   - 两边共用的状态（waiting_ / pending_ / last_*）由 mu_ 保护，临界区都很短。

#include <fsm/iControlBlock.h>

#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include <entities/pet_factory.h>
#include <server/protocol.h>
#include <thread_pool/thread_pool_base.h>

// 内核类型在全局命名空间。
class BattleContext;
class BattleFsm;

namespace server {

// 本层类型要在自己的命名空间里前向声明 —— 放到全局去的话 SessionPtr 会变成
// shared_ptr<::Session>，与 session.h 里的 shared_ptr<server::Session> 冲突。
class Session;

using SessionPtr = std::shared_ptr<Session>;

class Room : public IControlBlock, public std::enable_shared_from_this<Room> {
public:
    Room(int match_id, std::shared_ptr<ThreadPoolBase> battle_pool, int seat_count);
    ~Room() override;

    Room(const Room&) = delete;
    Room& operator=(const Room&) = delete;

    int match_id() const { return match_id_; }
    int seat_count() const { return seat_count_; }
    bool solo() const { return seat_count_ == 1; }

    // ── 座位 ──
    // 把连接放入座位。座位已被占或越界返回 false。
    bool attach_seat(int seat, const SessionPtr& session);
    // 连接断开：腾出座位。对局不在这里销毁 —— 由 BattleService 在"一个座位都不剩"时回收。
    void detach_seat(int seat);
    int attached_seat_count() const;
    bool has_any_seat() const;

    // ── 阵容与开局 ──
    // solo：一次提交双方阵容（request.side1 / side2）。
    bool submit_lineup(const BattleCreateRequest& request, std::string& err);
    // pvp：每个座位提交**自己那一边**，落到自己对应的边；两边齐了才开局。
    bool submit_side(int seat, const std::array<BattlePetMessage, 6>& party, std::string& err);

    bool battle_started() const;
    bool ready_to_start() const;

    // ── 客户端命令（reactor 线程）──
    // 提交动作。seat 是提交者座位，robot 是要操作的玩家。非法输入回 ERROR 给该座位。
    void submit_action(int seat, int robot, int action_type, int index);

    void set_step_mode(bool on);
    void toggle_breakpoint(int state_id);
    std::string state_json();
    std::string full_state_json();
    // 合法性动作集 JSON。player < 0 → 用"当前等着输入的那个玩家"。
    std::string legal_actions_json(int player);
    int current_state() const;
    bool is_waiting_for_input() const;
    int waiting_player() const;

    // ── IControlBlock（battle_pool 线程）──
    void wait_for_input(BattleContext* ctx) override;
    void async_write(int player_id, const std::string& data, BattleContext* ctx,
                     BattleFsm* fsm) override;
    void on_fsm_paused(BattleContext* ctx) override;

    // 座位 → 玩家。solo：seat0 同时驱动玩家 0/1；pvp：seat i 只驱动玩家 i。
    int seat_for_player(int player) const { return solo() ? 0 : player; }
    bool seat_owns_player(int seat, int player) const {
        if (seat < 0 || seat >= seat_count_ || player < 0 || player > 1) {
            return false;
        }
        return solo() || seat == player;
    }

private:
    // 条件满足（座位满 + 两边阵容齐）就建队开局。
    // 建队走 SQLite 且可能抛，所以整个动作在锁外做、失败可回滚。
    bool try_start_battle(std::string& err);

    // 把一份 FSM 输入（4 个 int）交给 ctx 并唤醒 FSM。调用方必须已持 mu_。
    void dispatch_input_locked(int player, std::string input);
    void post_run();
    void send_frame_to_all_seats(const std::string& frame);
    void send_frame_to_player(int player, const std::string& frame);
    void send_error_to_seat(int seat, const std::string& text);
    int attached_seat_count_locked() const;
    bool sides_ready_locked() const;
    // 该谁操作（选技能期 = current_player_id_；死亡换宠期 = 场上精灵倒了的那一方）。
    int resolve_acting_player(const BattleContext* ctx) const;
    // 发送 BATTLE_OVER（幂等：一局只报一次）。
    void finish_battle(BattleContext& ctx, const std::string& reason);

    const int match_id_;
    const int seat_count_;
    std::shared_ptr<ThreadPoolBase> battle_pool_;
    std::shared_ptr<BattleFsm> fsm_;

    mutable std::mutex mu_;
    std::array<SessionPtr, 2> seats_;
    // 按玩家排队：FSM 正在跑时到达的输入先排着，等 wait_for_input 时派发。
    std::array<std::deque<std::string>, 2> pending_inputs_;
    bool waiting_ = false;
    int waiting_player_ = -1;

    bool battle_started_ = false;
    bool battle_finished_reported_ = false;
    BattleCreateRequest lineup_;
    bool side_received_[2] = {false, false};

    // on_fsm_paused 去重：run() 可能被重复唤醒，此时不该重复发同一份带子。
    int last_reported_seq_ = -1;
    int last_reported_waiting_player_ = -2;

    // BattleContext 由 Room 独占持有，FSM 只拿裸指针（沿用原有所有权约定）。
    std::unique_ptr<BattleContext> context_;
};

}  // namespace server

#endif  // SERVER_ROOM_H
