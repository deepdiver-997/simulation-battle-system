#ifndef BATTLEFSM_H
#define BATTLEFSM_H

#include <memory>
#include <map>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <string>
#include <entities/seer-robot.h>
#include <thread_pool/thread_pool_base.h>

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
        SEND_EMOJI
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

    // 使用 raw pointer，context 由 control block 持有
    void run(BattleContext* battleContext);
    // is_forced = true 表示**非自愿**的换宠（死后补位，handle_ChooseAfterDeath）——限制类异常
    // （凝滞 32 / 瘫痪 19）只拦"主动切换"，拦不住死后补位，否则对局会卡死。
    void operation(BattleContext* battleContext, int robotId, ActionType actionType, int index,
                   bool is_forced = false);
    void log(const std::string& message);

    bool id_debug = true;

    // 战斗线程池（由 Server 注入）。
    // 类型是纯虚基类而非具体池：FSM 只用到 post()，不该被"池是 asio 还是原生"绑死。
    std::shared_ptr<ThreadPoolBase> battle_pool_;

    // Battle pool 用于继续执行
    void post(std::function<void()> task);

private:
    std::unordered_map<State, HandlerType> stateHandlerMap;
    bool verbose_trace_ = false;

    void trace_fsm(const BattleContext* battleContext, const std::string& phase) const;

    bool runInternal(BattleContext* battleContext);

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
