#ifndef BATTLEFSM_H
#define BATTLEFSM_H

#include <memory>
#include <map>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <string>
#include <entities/seer-robot.h>

class BattleContext;
enum class State;

// Forward declare for callback type
class BattleFsm;

class BattleFsm {
public:
    enum class ActionType {
        CHOOSE_PET,
        SELECT_SKILL,
        USE_MEDICINE,
        NONE,   // 跳过出手阶段但是回合类效果依旧触发，比如被控制但是点击了技能
        SEND_EMOJI,
        // **内部操作**（2026-11-21 驱逐线）：驱逐下场。绝不进线协议——proto::ActionType
        // 不加对应值，且 operation() 的 switch **没有**本档 case（default 落空 → roundChoice
        // 不变 → handle_OperationChooseSkillMedicament 的 accepted 判 false 拒绝），
        // wire 侧发本数值天然被拒。产生途径只有两条：
        //   ① 玄武 2553 重分类：收包时把 CHOOSE_PET 改写成 EXPEL（参数=玩家选的槽）；
        //   ② 天尊线强制驱逐：BATTLE_ROUND_START 锚点按 forced_expel_pending 机器填参（随机槽）。
        // 处理语义 = 表现为中切的下场（is_skill_action=false 跳过出手主流程），
        // perform_switch(cause=EXPELLED) + **不发 EVENT_SWAP**（驱逐不是主动切换，
        // 朝曦/神印/2040 窗等"主动"键控 watcher 天然不触发）。
        EXPEL
    };

    BattleFsm(bool enable_debug = true);
    BattleFsm(const BattleFsm&) = delete;
    BattleFsm& operator=(const BattleFsm&) = delete;
    BattleFsm(BattleFsm&&) = delete;
    BattleFsm& operator=(BattleFsm&&) = delete;
    ~BattleFsm();

    using HandlerType = void (BattleFsm::*)(BattleContext*);

    void initHandler();
    void addStateAction(State state, HandlerType action);

    // 首次 run/operation 绑定本实例；此后不允许换 FSM（抛 std::logic_error），
    // FSM 的生命周期必须覆盖该 context 的使用。空 context 抛 std::invalid_argument。
    // run 自行持有 run_mutex；直接调用 operation 须由调用方保证与所有 context 操作
    // 串行（例如持 run_mutex）。FSM 的内部 operation 调用已在 run_mutex 下。
    void run(BattleContext* battleContext);
    // is_forced = true 表示**非自愿**的换宠（死后补位，handle_ChooseAfterDeath）——限制类异常
    // （凝滞 32 / 瘫痪 19）只拦"主动切换"，拦不住死后补位，否则对局会卡死。
    void operation(BattleContext* battleContext, int robotId, ActionType actionType, int index,
                   bool is_forced = false);
    void log(const std::string& message);

    bool id_debug = true;

    // ⚠️ FSM 不持有执行资源（2026-11-22 用户裁决）：线程池归 server 层（service 创建、
    //    Room 持有并 post 战斗任务），FSM 只是一组被调用的状态处理函数集合——驱动方式
    //    = runInternal 同步推进 + need_input 时交还控制块。历史上这里曾有一个
    //    battle_pool_ 成员 + post() 转发（零读取/零调用，纯死重），已删；别再加回来。

private:
    friend class BattleClone;
    friend class BattleCloneBranch;
    std::unordered_map<State, HandlerType> stateHandlerMap;
    bool verbose_trace_ = false;

    void trace_fsm(const BattleContext* battleContext, const std::string& phase) const;

    // Caller must serialize access to the context.
    void bind_context_locked(BattleContext& context);
    // True if a handler finished, including event delivery and sampling.
    bool run_loop(BattleContext* battleContext, bool one_state);
    bool runInternal(BattleContext* battleContext);

    // 命中前全局锚点（EVENT_BEFORE_SKILL_HIT）：一回合只在第一次进入命中前时点发一次，
    // emit 后立即 drain（见 battleFsm.cpp 实现处注释）。
    void emit_before_hit_anchor(BattleContext* ctx, int mover_id);

    // 时点采样：在每个状态执行完、事件 drain 之后录一条（tape 关闭时零开销）。
    void record_tape_sample(BattleContext* ctx, State state);

    void General_Handler(BattleContext* battleContext, int robotId);
    void handle_GameStart(BattleContext* battleContext);
    void handle_OperationEnterExitStage(BattleContext* battleContext);
    void handle_OperationChooseSkillMedicament(BattleContext* battleContext);
    void handle_OperationProtectionMechanism1(BattleContext* battleContext);
    void handle_OperationEnterStage(BattleContext* battleContext);
    void handle_BattleRoundStart(BattleContext* battleContext);
    void handle_BattleFirstMoveRight(BattleContext* battleContext);
    void handle_BattleFirstActionStart(BattleContext* battleContext);
    void handle_BattleFirstBeforeSkillHit(BattleContext* battleContext);
    void handle_BattleFirstOnSkillHit(BattleContext* battleContext);
    void handle_BattleFirstSkillEffect(BattleContext* battleContext);
    void handle_BattleFirstAttackDamage(BattleContext* battleContext);
    void handle_BattleFirstAfterAction(BattleContext* battleContext);
    void handle_BattleFirstActionEnd(BattleContext* battleContext);
    void handle_BattleFirstAfterActionEnd(BattleContext* battleContext);
    void handle_BattleFirstExtraAction(BattleContext* battleContext);
    void handle_BattleFirstMoverDeath(BattleContext* battleContext);
    void handle_BattleSecondActionStart(BattleContext* battleContext);
    void handle_BattleSecondBeforeSkillHit(BattleContext* battleContext);
    void handle_BattleSecondOnSkillHit(BattleContext* battleContext);
    void handle_BattleSecondSkillEffect(BattleContext* battleContext);
    void handle_BattleSecondAttackDamage(BattleContext* battleContext);
    void handle_BattleSecondAfterAction(BattleContext* battleContext);
    void handle_BattleSecondActionEnd(BattleContext* battleContext);
    void handle_BattleSecondAfterActionEnd(BattleContext* battleContext);
    void handle_BattleSecondExtraAction(BattleContext* battleContext);
    void handle_BattleRoundEnd(BattleContext* battleContext);
    void handle_BattleSecondMoverDeath(BattleContext* battleContext);
    void handle_BattleOldRoundEnd1(BattleContext* battleContext);
    void handle_BattleRoundReductionAllRoundMinus(BattleContext* battleContext);
    void handle_BattleRoundReductionNewRoundEnd(BattleContext* battleContext);
    void handle_BattleOldRoundEnd2(BattleContext* battleContext);
    void handle_BattleDeathTiming(BattleContext* battleContext);
    void handle_BattleDefeatStatus(BattleContext* battleContext);
    void handle_BattleOpponentDefeatStatus(BattleContext* battleContext);
    void handle_BattleNewDefeatMechanism(BattleContext* battleContext);
    void handle_OperationProtectionMechanism2(BattleContext* battleContext);
    void handle_BattleAfterDefeated(BattleContext* battleContext);
    void handle_ChooseAfterDeath(BattleContext* battleContext);
    void handle_BattleAfterDefeatingOpponent(BattleContext* battleContext);
    void handle_BattleRoundCompletion(BattleContext* battleContext);
    void handle_Finished(BattleContext* battleContext);
};

#endif // BATTLEFSM_H
