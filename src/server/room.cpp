#include <server/room.h>

#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/battle_mutate.h>
#include <server/output_json.h>
#include <server/session.h>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <exception>
#include <mutex>
#include <string>

namespace server {
namespace {

void log_room(const char* level, int match_id, const std::string& msg) {
    std::printf("[%s] [match %d] %s\n", level, match_id, msg.c_str());
    std::fflush(stdout);
}

}  // namespace

Room::Room(int match_id, std::shared_ptr<ThreadPoolBase> battle_pool, int seat_count)
    : match_id_(match_id),
      seat_count_(seat_count == 1 ? 1 : 2),
      battle_pool_(std::move(battle_pool)) {
    fsm_ = std::make_shared<BattleFsm>(true);
    fsm_->battle_pool_ = battle_pool_;
}

Room::~Room() = default;

// ---------------------------------------------------------------- 座位管理

int Room::attached_seat_count_locked() const {
    int n = 0;
    for (int i = 0; i < seat_count_; ++i) {
        if (seats_[i]) {
            ++n;
        }
    }
    return n;
}

int Room::attached_seat_count() const {
    std::lock_guard<std::mutex> lk(mu_);
    return attached_seat_count_locked();
}

bool Room::attach_seat(int seat, const SessionPtr& session) {
    if (seat < 0 || seat >= seat_count_ || !session) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (seats_[seat]) {
            return false;  // 座位已被占
        }
        seats_[seat] = session;
    }

    {
        const std::string payload = room_info_to_json(match_id_, seat, seat_count_, solo());
        session->send_frame(proto::build_frame(proto::Command::ROOM_INFO,
                                              static_cast<std::uint32_t>(match_id_), payload));
    }
    log_room("INFO", match_id_, "seat " + std::to_string(seat) + " attached");

    // 座位满了、阵容也收到了 → 开局。pvp 就靠这一步"等第二个人坐进第二座"。
    std::string err;
    try_start_battle(err);
    if (!err.empty()) {
        send_error_to_seat(seat, err);
    }
    return true;
}

void Room::detach_seat(int seat) {
    if (seat < 0 || seat >= seat_count_) {
        return;
    }
    std::lock_guard<std::mutex> lk(mu_);
    seats_[seat].reset();
    log_room("INFO", match_id_, "seat " + std::to_string(seat) + " detached");
}

bool Room::has_any_seat() const {
    std::lock_guard<std::mutex> lk(mu_);
    return attached_seat_count_locked() > 0;
}

bool Room::ready_to_start() const {
    std::lock_guard<std::mutex> lk(mu_);
    return sides_ready_locked() && attached_seat_count_locked() == seat_count_;
}

bool Room::battle_started() const {
    std::lock_guard<std::mutex> lk(mu_);
    return battle_started_;
}

bool Room::sides_ready_locked() const {
    // solo 与 pvp 判据相同：两边阵容都要有。
    // 差别只在"谁置的标志"—— solo 由 submit_lineup 一次置两个，pvp 由两个座位各置一个。
    return side_received_[0] && side_received_[1];
}

// ---------------------------------------------------------------- 开局

// 建队可能在数据层抛（精灵/技能查不到）。抛了必须回滚成"还没开局"，
// 让客户端改了阵容重新提交，而不是把半死的房间留在表里。
bool Room::try_start_battle(std::string& err) {
    err.clear();
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (battle_started_ || !sides_ready_locked()) {
            return false;  // 阵容还没齐
        }
        if (attached_seat_count_locked() != seat_count_) {
            return false;  // 等对手坐进来
        }
    }

    // 建队必须在锁外：create_robot 会查 SQLite、构造技能与魂印，耗时且可能抛。
    std::unique_ptr<BattleContext> fresh;
    try {
        SeerRobot robots[2] = {
            SeerRobotFactory::create_robot(lineup_.side1, lineup_.medicines[0],
                                           lineup_.equip_item_ids[0]),
            SeerRobotFactory::create_robot(lineup_.side2, lineup_.medicines[1],
                                           lineup_.equip_item_ids[1]),
        };
        fresh.reset(new BattleContext(this, robots));
        // boss 挑战对局（2026-09-26 "boss 有效"线）：开关注入 context，
        // 魂印程序注册口按节点 boss_invalid 标签过滤。
        fresh->is_boss_challenge = lineup_.boss_challenge;
        // 双背包快照（精灵王线）：待命背包进 context（只读），空 = 直开对局（待命 6 格全空）。
        // 装配期旗标按 id 走 PetFactory 判定——待命背包与出战背包同构（静态 6+6），
        // 战斗创建时定死，故出战侧的装配期判定对待命 id 同样成立。
        for (int side = 0; side < 2; ++side) {
            const auto& ids = lineup_.standby_pet_ids[side];
            if (!ids.empty()) {
                fresh->standby_initialized = true;
                for (std::size_t i = 0; i < ids.size() && i < 6; ++i) {
                    fresh->standby_pet_ids[side][i] = ids[i];
                    fresh->standby_is_spirit_king[side][i] =
                        PetFactory::pet_is_spirit_king(ids[i]);
                    fresh->standby_hp_consume_kit[side][i] =
                        PetFactory::pet_has_hp_consume_kit(ids[i]);
                }
            }
        }
    } catch (const std::exception& ex) {
        std::lock_guard<std::mutex> lk(mu_);
        side_received_[0] = false;  // 回滚：允许改了阵容重新提交
        side_received_[1] = false;
        err = std::string("failed to build teams: ") + ex.what();
        log_room("ERROR", match_id_, err);
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(mu_);
        if (battle_started_) {
            return false;  // 并发下已被别人开起来了
        }
        context_ = std::move(fresh);
        context_->uuid = static_cast<unsigned int>(match_id_);
        // 事件带默认关闭（71 个场景用例零开销）；服务端对局显式打开。
        context_->enable_tape(true);
        battle_started_ = true;
        // 新对局：上一次的"已上报序号"作废。
        last_reported_seq_ = -1;
        last_reported_waiting_player_ = -2;
    }

    log_room("INFO", match_id_, "battle started, seats=" + std::to_string(seat_count_));

    // 首次推进：从 GAME_START 一路跑到第一次需要输入（或直接结束）。
    post_run();
    return true;
}

bool Room::submit_lineup(const BattleCreateRequest& request, std::string& err) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (battle_started_) {
            err = "battle already started";
            return false;
        }
        if (side_received_[0] || side_received_[1]) {
            err = "lineup already submitted";
            return false;
        }
        // solo 语义：一次给双方。pvp 下若要各自提交，走 submit_side。
        lineup_ = request;
        side_received_[0] = true;
        side_received_[1] = true;
    }
    const bool started = try_start_battle(err);
    if (!started) {
        err.clear();  // 等对手不算错误：已受理
    }
    return true;
}

bool Room::submit_side(int seat, const std::array<BattlePetMessage, 6>& party, std::string& err) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (battle_started_) {
            err = "battle already started";
            return false;
        }
        if (seat < 0 || seat >= seat_count_) {
            err = "bad seat";
            return false;
        }
        if (side_received_[seat]) {
            err = "side already submitted for your seat";
            return false;
        }
        if (solo()) {
            err = "solo room takes both sides at once (submit_lineup)";
            return false;
        }
        if (seat == 0) {
            lineup_.side1 = party;
        } else {
            lineup_.side2 = party;
        }
        side_received_[seat] = true;
    }
    const bool started = try_start_battle(err);
    if (!started) {
        err.clear();
    }
    return true;
}

// ---------------------------------------------------------------- 输入

// 该谁操作。"选技能期"看 current_player_id_；"死亡换宠期"看谁的场上精灵倒了
// （理由见 legal_actions.cpp 的同名注释 —— 用错判据会让对局永远打不完）。
int Room::resolve_acting_player(const BattleContext* ctx) const {
    if (ctx == nullptr) {
        return -1;
    }
    if (ctx->currentState != State::CHOOSE_AFTER_DEATH) {
        return ctx->current_player_id_;
    }
    auto on_stage_dead = [ctx](int side) {
        const int slot = ctx->on_stage[side];
        if (slot < 0 || slot >= 6) {
            return false;
        }
        return ctx->seerRobot[side].elfPets[slot].hp <= 0;
    };
    const bool dead0 = on_stage_dead(0);
    const bool dead1 = on_stage_dead(1);
    if (dead0 && !dead1) {
        return 0;
    }
    if (dead1 && !dead0) {
        return 1;
    }
    // 两边都倒：先让引擎当前指定的那个换；都没倒（不该发生）也退回引擎值。
    return ctx->current_player_id_;
}

void Room::dispatch_input_locked(int player, std::string input) {
    context_->m_buffer.assign(input.begin(), input.end());
    context_->is_empty = false;
    waiting_ = false;
    waiting_player_ = -1;
}

void Room::post_run() {
    // ⚠️ 必须持**强**引用，不能只捕获裸指针。
    //
    // 排队中的任务可能在"房间已被回收"之后才被 worker 取到：连接断开 → 座位腾空 →
    // BattleService 把房间从表里摘掉 → Room/BattleContext 析构——而队列里还排着这个
    // 房间的推进任务。裸指针版本会拿着已释放的 BattleContext 去锁它的 run_mutex
    // （实测症状：`[pool] task threw: mutex lock failed: Invalid argument`）。
    //
    // 持强引用之后，摘表只是丢掉 map 的那一份引用：只要还有任务排队或正在执行，
    // Room 就活着，最后一个任务结束时才析构。正在执行的那次推进本身也由它自己的
    // 任务引用覆盖，所以"跑一半被销毁"同样不可能发生。
    auto self = shared_from_this();
    battle_pool_->post([self] {
        if (self->context_ != nullptr && self->fsm_ != nullptr) {
            self->fsm_->run(self->context_.get());
        }
    });
}

void Room::submit_action(int seat, int robot, int action_type, int index) {
    std::string err;
    bool need_post = false;

    {
        std::lock_guard<std::mutex> lk(mu_);

        const int max_index = (action_type == static_cast<int>(proto::ActionType::CHOOSE_PET)) ? 6
            : (action_type == static_cast<int>(proto::ActionType::NONE)) ? 1 : 5;
        if (!battle_started_ || context_ == nullptr) {
            err = "battle not started";
        } else if (!seat_owns_player(seat, robot)) {
            err = "your seat cannot act for player " + std::to_string(robot);
        } else if (action_type < 0 || action_type > 3) {
            err = "bad action type";
        } else if (action_type == static_cast<int>(proto::ActionType::NONE)
                   ? (index != 0)
                   : (index < 0 || index >= max_index)) {
            err = "bad action index";
        } else if (waiting_ && waiting_player_ == robot && pending_inputs_[robot].empty()) {
            // 正等着这个玩家 → 直接派发
            dispatch_input_locked(robot, proto::build_fsm_input(robot, action_type, index));
            need_post = true;
        } else {
            // 还没轮到、或 FSM 正在跑 → 排队，等 wait_for_input 时派发。
            // 这条路径支撑"客户端把双方动作一次发完"（solo 的主力用法）。
            //
            // 每玩家只排 1 个：一个回合本来就只需要该玩家一个动作。允许排更多的话，
            // 重复提交（重试、界面抖动、脚本重放）会攒下**过期意图**，在之后的某个回合
            // 被静默采用——那种 bug 极难从现象定位。满了就明确拒绝，让客户端知道要等提示。
            if (!pending_inputs_[robot].empty()) {
                err = "player " + std::to_string(robot) +
                      " already has a pending action; wait for INPUT_REQUIRED";
            } else {
                pending_inputs_[robot].push_back(proto::build_fsm_input(robot, action_type, index));
            }
        }
    }

    if (!err.empty()) {
        send_error_to_seat(seat, err);
        return;
    }
    if (need_post) {
        post_run();
    }
}

void Room::wait_for_input(BattleContext* ctx) {
    bool need_post = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        waiting_ = true;
        waiting_player_ = resolve_acting_player(ctx);
        if (waiting_player_ < 0 || waiting_player_ > 1) {
            return;  // 引擎给了异常值：不派发，交给下一次 wait_for_input 收敛
        }

        // 已经排着这个玩家的输入 → 不真的等，立刻派发继续跑。
        // "客户端把双方动作一次发完"能工作就是靠这里。
        auto& queue = pending_inputs_[waiting_player_];
        if (!queue.empty()) {
            std::string input = std::move(queue.front());
            queue.pop_front();
            dispatch_input_locked(waiting_player_, std::move(input));
            need_post = true;
        }
    }

    if (need_post) {
        post_run();
    }
}

void Room::async_write(int player_id, const std::string& data, BattleContext* ctx, BattleFsm* fsm) {
    // FSM 只在"输入非法，请重选"时走这里（7 个调用点全是错误文案）。
    // 这里把它包成 ERROR 帧 —— 老实现把裸文本直接写进流，长度前缀成帧会被破坏，
    // 新客户端会把它判成非法帧而断连。
    send_frame_to_player(
        player_id,
        proto::build_frame(proto::Command::ERROR, static_cast<std::uint32_t>(match_id_), data));

    // 与老实现同语义：交给客户端之后继续推进 ctx，让 FSM 重新进入 wait_for_input
    // 把等待重新排好（此时 is_empty 仍为 true，所以会立刻再次停在等输入）。
    // 同样持强引用续跑（理由见 post_run 的注释）。
    (void)ctx;
    auto self = shared_from_this();
    battle_pool_->post([self] {
        if (self->context_ != nullptr && self->fsm_ != nullptr) {
            self->fsm_->run(self->context_.get());
        }
    });
}

// ---------------------------------------------------------------- 输出

void Room::send_frame_to_all_seats(const std::string& frame) {
    std::array<SessionPtr, 2> snapshot;
    {
        std::lock_guard<std::mutex> lk(mu_);
        snapshot = seats_;
    }
    for (auto& s : snapshot) {
        if (s) {
            s->send_frame(frame);
        }
    }
}

void Room::send_frame_to_player(int player, const std::string& frame) {
    if (player < 0 || player > 1) {
        return;
    }
    SessionPtr target;
    {
        std::lock_guard<std::mutex> lk(mu_);
        target = seats_[seat_for_player(player)];
    }
    if (target) {
        target->send_frame(frame);
    }
}

void Room::send_error_to_seat(int seat, const std::string& text) {
    SessionPtr target;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (seat >= 0 && seat < seat_count_) {
            target = seats_[seat];
        }
    }
    if (target) {
        target->send_frame(proto::build_frame(proto::Command::ERROR,
                                              static_cast<std::uint32_t>(match_id_), text));
    }
}

void Room::on_fsm_paused(BattleContext* ctx) {
    if (ctx == nullptr) {
        return;
    }

    const int seq = ctx->tape_.next_seq();
    int waiting_player = -1;
    {
        std::lock_guard<std::mutex> lk(mu_);
        // 去重：run() 可能被重复唤醒（重复 post、async_write 之后的续跑），
        // 此时既没有新采样、等待方也没变，不该把同一份带子再发一遍。
        if (waiting_) {
            waiting_player = waiting_player_;
        }
        if (seq == last_reported_seq_ && waiting_player == last_reported_waiting_player_) {
            return;
        }
        last_reported_seq_ = seq;
        last_reported_waiting_player_ = waiting_player;
    }

    // 1) 事件带：这一轮推进里每个时点一条采样，按 seq 有序。
    const auto& samples = ctx->tape_.samples();
    if (!samples.empty()) {
        const std::string payload = tape_to_json(match_id_, samples);
        if (!payload.empty()) {
            send_frame_to_all_seats(proto::build_frame(
                proto::Command::TAPE, static_cast<std::uint32_t>(match_id_), payload));
        }
        ctx->tape_.clear_delivered();
    }

    // 2) 结束 或 该谁操作了。
    if (ctx->currentState == State::FINISHED) {
        finish_battle(*ctx, "normal");
        return;
    }

    if (waiting_player < 0) {
        return;
    }

    // ⚠️ 引擎死路兜底：正在等输入，但该玩家一个合法动作都没有。
    //
    // 已知触发场景（实测复现）：一方 6 只精灵全灭后，FSM 仍停在"死后选择"要求换宠，
    // 而没有任何可换的精灵 —— 它的 need_input() 只看"场上精灵是否倒下"，不看
    // "还有没有活着的精灵可换"；kLinearStateOrder 里也没有 FINISHED，
    // 败北结算依赖"被击败方换宠后继续推进"，于是对局永久卡住。
    //
    // 这是**引擎缺口**，正确修法是改败北判定（属于领域语义，需要口径确认），
    // 不在这里私改。但服务端不能因此把玩家挂死：兜底结束 + 打 ERROR 让缺口可见。
    const LegalActions legal = compute_legal_actions(*ctx, waiting_player);
    if (!has_any_legal_action(legal)) {
        log_room("ERROR", match_id_,
                 std::string("引擎卡死：等待玩家 ") + std::to_string(waiting_player) +
                     " 在「" + state_name_cn(ctx->currentState) +
                     "」操作，但没有任何合法动作（该方精灵是否已全灭？）。"
                     "服务端兜底结束本局 —— 这是引擎缺口，不是正常结束。");
        finish_battle(*ctx, "engine_no_legal_action");
        return;
    }

    // 快照 + 合法动作集一起给：客户端收到这一条就能立刻渲染，且只给出合法按钮。
    const std::string payload =
        input_required_to_json(match_id_, *ctx, waiting_player, ctx->getStateJson());
    send_frame_to_player(waiting_player, proto::build_frame(
        proto::Command::INPUT_REQUIRED, static_cast<std::uint32_t>(match_id_), payload));

    // 谁在什么时候被等 —— 联调时最常问的就是这个，值得留痕（带 match 前缀便于过滤）。
    log_room("INFO", match_id_,
             std::string("等待玩家 ") + std::to_string(waiting_player) + " 操作（回合 " +
                 std::to_string(ctx->roundCount) + "，" + state_name_cn(ctx->currentState) + "）");
}

void Room::finish_battle(BattleContext& ctx, const std::string& reason) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (battle_finished_reported_) {
            return;  // 只报一次
        }
        battle_finished_reported_ = true;
    }
    const std::string payload = battle_over_to_json(match_id_, ctx, ctx.getStateJson(), reason);
    send_frame_to_all_seats(proto::build_frame(proto::Command::BATTLE_OVER,
                                              static_cast<std::uint32_t>(match_id_), payload));
    log_room("INFO", match_id_, "battle finished (" + reason + ")");
}

// ---------------------------------------------------------------- 调试/查询

void Room::set_step_mode(bool on) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (context_ == nullptr) {
            return;
        }
        context_->debug_step_mode = on;
    }
    // 开/关单步后立刻推一步，客户端不必再手动催一次。投递放在锁外。
    post_run();
}

void Room::toggle_breakpoint(int state_id) {
    std::lock_guard<std::mutex> lk(mu_);
    if (context_ == nullptr) {
        return;
    }
    auto& bps = context_->breakpoints;
    if (bps.count(state_id)) {
        bps.erase(state_id);
    } else {
        bps.insert(state_id);
    }
}

int Room::current_state() const {
    std::lock_guard<std::mutex> lk(mu_);
    return context_ == nullptr ? -999 : static_cast<int>(context_->currentState);
}

// ---------------------------------------------------------------- 调试手术（二期）

// 白名单操作集：客户端不能裸写 context（会把免死/复活/清除时点等不变量改破），
// 只发具名操作，由 battle_mutate 走引擎存储与校验落地。
// 执行窗口 = FSM 泊车（waiting_）：battle 单线程泊车时无引擎活动，持 run_mutex
// 与可能的推进任务互斥后改状态是安全的——这正是场景测试"CHOOSE 停泊期直调
// 原语"的同一姿势。推进中拒绝而不是排队：注入语义要求"此刻"，排队会埋雷。
void Room::debug_mutate(const std::string& payload) {
    const auto reply = [this](const std::string& body) {
        send_frame_to_all_seats(proto::build_frame(
            proto::Command::DEBUG_MUTATE, static_cast<std::uint32_t>(match_id_), body));
    };

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(payload);
    } catch (const std::exception& ex) {
        reply(std::string("{\"ok\":false,\"error\":\"bad json: ") + ex.what() + "\"}");
        return;
    }
    if (!doc.is_object() || !doc.contains("ops") || !doc["ops"].is_array()) {
        reply("{\"ok\":false,\"error\":\"expected {\\\"ops\\\": [...]}}\"}");
        return;
    }

    // —— 取任务与状态（锁内只做快照）——
    BattleContext* ctx = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!battle_started_ || context_ == nullptr) {
            reply("{\"ok\":false,\"error\":\"battle not started\"}");
            return;
        }
        if (!waiting_) {
            reply("{\"ok\":false,\"error\":"
                  "\"引擎未泊车：等输入提示出现（或逐步/断点停下）后再注入\"}");
            return;
        }
        ctx = context_.get();
    }

    // —— 逐条执行（持 run_mutex 与推进任务互斥；泊车时通常无竞争）——
    nlohmann::json notes = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> run_lk(ctx->run_mutex);
        for (const auto& item : doc["ops"]) {
            if (!item.is_object() || !item.contains("op")) {
                notes.push_back("✗ 非法操作项");
                continue;
            }
            const std::string op = item.value("op", "");
            const auto ival = [&item](const char* key, int dflt) {
                return item.contains(key) && item[key].is_number_integer()
                           ? item[key].get<int>() : dflt;
            };
            battle_mutate::Outcome out;
            if (op == "set_hp") {
                out = battle_mutate::set_hp(*ctx, ival("side", -1), ival("slot", -1),
                                            ival("value", 0));
            } else if (op == "set_pp") {
                out = battle_mutate::set_pp(*ctx, ival("side", -1), ival("slot", -1),
                                            ival("skill", -1), ival("value", 0));
            } else if (op == "set_level") {
                out = battle_mutate::set_level(*ctx, ival("side", -1), ival("stat", -1),
                                               ival("value", 0));
            } else if (op == "anomaly") {
                out = battle_mutate::apply_anomaly(*ctx, ival("side", -1), ival("id", -1),
                                                   ival("rounds", 1));
            } else if (op == "cure") {
                out = battle_mutate::cure_anomaly(*ctx, ival("side", -1), ival("id", -1));
            } else if (op == "mark") {
                out = battle_mutate::attach_mark(*ctx, ival("src", -1), ival("dst", -1));
            } else if (op == "unmark") {
                out = battle_mutate::clear_marks(*ctx, ival("side", -1));
            } else {
                out = battle_mutate::Outcome{false, "", "unknown op: " + op};
            }
            if (out.ok) {
                notes.push_back(out.note);
                log_room("INFO", match_id_, "手术: " + out.note);
            } else {
                notes.push_back("✗ " + out.err);
                log_room("WARN", match_id_, "手术被拒: " + out.err);
            }
        }
    }

    nlohmann::json body;
    body["ok"] = true;
    body["notes"] = notes;
    reply(body.dump());
    // 状态变了：把新快照推给客户端（mutation 结果立刻可见）。
    send_frame_to_all_seats(proto::build_frame(
        proto::Command::SYNC_STATE, static_cast<std::uint32_t>(match_id_), state_json()));
}

bool Room::is_waiting_for_input() const {
    std::lock_guard<std::mutex> lk(mu_);
    return waiting_;
}

int Room::waiting_player() const {
    std::lock_guard<std::mutex> lk(mu_);
    return waiting_player_;
}

std::string Room::state_json() {
    std::lock_guard<std::mutex> lk(mu_);
    if (context_ == nullptr) {
        return std::string("{\"error\":\"battle not started\"}");
    }
    return context_->getStateJson();
}

std::string Room::legal_actions_json(int player) {
    std::lock_guard<std::mutex> lk(mu_);
    if (context_ == nullptr) {
        return std::string("{\"error\":\"battle not started\"}");
    }
    if (player < 0) {
        player = waiting_ ? waiting_player_ : 0;
    }
    return legal_actions_to_json(compute_legal_actions(*context_, player));
}

std::string Room::full_state_json() {
    std::lock_guard<std::mutex> lk(mu_);
    if (context_ == nullptr) {
        return std::string("{\"error\":\"battle not started\"}");
    }
    return context_->getFullStateJson();
}

}  // namespace server
