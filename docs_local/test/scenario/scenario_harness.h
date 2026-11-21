#ifndef SCENARIO_HARNESS_H
#define SCENARIO_HARNESS_H

// 场景化对战测试框架（header-only，供每个场景二进制复用）。
//
// 理念（2026-08-22 用户方向）：
// - 手动构造精灵/数值/魂印，减少 DB 依赖（唯一触点 Skills(id) 加载技能静态数据）。
// - 走真实 FSM（BattleFsm::run），提交操作（m_buffer 路径触发 on_selected 注册先制）。
// - 用 debug_step_mode / breakpoints / getFullStateJson 逐步查看时点执行结果。
//
// 用法：
//   auto pet0 = scenario::make_pet(0, "南霜", {28319, 10001, 10001, 10001, 10001},
//                                   /*speed=*/100, /*atk=*/200, /*hp=*/500, sm);
//   scenario::ScenarioBattle battle({pet0,...}, {pet1,...});
//   battle.start();                            // 驱动到 CHOOSE 等输入
//   battle.submit_operation(0, SELECT_SKILL, 0);
//   battle.submit_operation(1, SELECT_SKILL, 0);
//   battle.drive();                            // 跑完整个回合到下一输入/FINISHED
//   assert(battle.ctx().preemptive_right == ...);

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <entities/common_trait.h>
#include <entities/elf-pet.h>
#include <entities/numerical-properties.h>
#include <entities/seer-robot.h>
#include <entities/soul_mark.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/iControlBlock.h>

// ── 轻量断言宏（比裸 assert 更会打印失败信息）────────────────────────
// 用法：CHECK(cond, "msg") / CHECK_EQ(a, b, "msg")。失败打印表达式+值(ENUM_EQ用static_cast<int>)+位置+abort。
#ifndef CHECK
#define CHECK(cond, ...)                                                         \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << "\n[CHECK FAILED] " << #cond << "\n  " << __VA_ARGS__   \
                      << "\n  at " << __FILE__ << ":" << __LINE__ << std::endl;  \
            std::abort();                                                        \
        }                                                                        \
    } while (0)
#define CHECK_EQ(a, b, ...)                                                      \
    do {                                                                         \
        if (!((a) == (b))) {                                                     \
            std::cerr << "\n[CHECK_EQ FAILED] " << #a << "="                     \
                      << static_cast<long long>(a) << "  vs  " << #b << "="      \
                      << static_cast<long long>(b) << "\n  " << __VA_ARGS__      \
                      << "\n  at " << __FILE__ << ":" << __LINE__ << std::endl;  \
            std::abort();                                                        \
        }                                                                        \
    } while (0)
#endif

namespace scenario {

// 最小控制块：wait_for_input 空转，其余纯虚 no-op。FSM 在等输入状态会解引用它。
struct StubControlBlock : IControlBlock {
    void wait_for_input(BattleContext*) override {}
    void async_write(int, const std::string&, BattleContext*, BattleFsm*) override {}
    void broadcast(const std::string&) override {}
    void broadcast_to(const std::vector<int>&, const std::string&) override {}
    void log_operation(const std::string&) override {}
    std::string get_operation_log() const override { return {}; }
    std::unique_ptr<BattleContext> duplicate_context(uint32_t) override { return nullptr; }
    int match_id() const override { return 0; }
    bool is_training_mode() const override { return false; }
    void register_context(std::unique_ptr<BattleContext>) override {}
    std::unique_ptr<BattleContext> unregister_context(uint32_t) override { return nullptr; }
    std::shared_ptr<BattleFsm> get_fsm() const override { return nullptr; }
    void set_fsm(std::shared_ptr<BattleFsm>) override {}
    BattleContext* get_context() override { return nullptr; }
};

// 构造手动精灵：技能按 id 加载（不校验可学性），数值/魂印手动。
inline ElfPet make_pet(int id, const std::string& name,
                       const std::array<int, 5>& skill_ids,
                       int speed, int atk, int hp,
                       SoulMark soul_mark = {}) {
    std::array<Skills, 5> skills{
        Skills(skill_ids[0]), Skills(skill_ids[1]), Skills(skill_ids[2]),
        Skills(skill_ids[3]), Skills(skill_ids[4]),
    };
    // 暴击率：引擎已按官方口径接入（`moves.crit_rate` = 分子/16，**0 = 永不必暴**）。
    // 大多数技能本来就是 0；但 DB 里有约 200 条 `crit_rate>0` 的技能（如苍炎坠 8/16 = 50%），
    // 而场景测试的伤害断言多为精确值/紧区间比较、`std::rand` 又未播种（序列会被每次调用移位）
    // → 默认清零暴击率，让断言可比。注意 rate==0 时暴击判定**短路、不消耗 rand()**，
    // 所以默认档与"未接入暴击"时代字节级一致。
    // 需要验暴击的场景显式打开：`set_crit_sixteenths(pet, 16)` = 必定暴击。
    for (Skills& s : skills) {
        s.critical_strike_rate = 0.0f;
    }
    numerical_properties base{};
    base[NumericalPropertyIndex::PHYSICAL_ATTACK] = atk;
    base[NumericalPropertyIndex::SPECIAL_ATTACK] = atk;
    base[NumericalPropertyIndex::DEFENSE] = 100;
    base[NumericalPropertyIndex::SPECIAL_DEFENSE] = 100;
    base[NumericalPropertyIndex::SPEED] = speed;
    base[NumericalPropertyIndex::HP] = hp;
    // 元素类型用 0（克制矩阵 [0][0]=1 正常 1x；类型 1 相互克制=0 会打出 0 伤害）。
    return ElfPet(id, name, {0, 0}, /*soul_seal=*/0, Gender::NONE, std::move(soul_mark),
                  CommonTrait{}, base, /*initial_hp=*/hp,
                  /*levels=*/{0, 0, 0, 0, 0, 0}, /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 把精灵所有技能的暴击率设为 sixteenths/16（官方口径：crit_rate 是分母 16 的分子）。
// 1 = 基础暴击率 1/16；16 = 必定暴击（测试用确定性档）。0 = 关闭暴击。
inline void set_crit_sixteenths(ElfPet& pet, int sixteenths) {
    for (Skills& s : pet.skills) {
        s.critical_strike_rate = static_cast<float>(sixteenths) * 100.0f / 16.0f;
    }
}

// 构造一支 6 格队伍（前 4 只 make_pet 出的同一精灵，后 2 只相同——占位）。
inline std::array<ElfPet, 6> make_party(const ElfPet& pet) {
    return {pet, pet, pet, pet, pet, pet};
}

// 高闪避魂印：ROUND_START 把 owner 的闪避率设为 1.0（放大对方的 miss 率）。
inline EffectResult dodge_soul_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[owner] = 1.0f;  // 该 owner 闪避 100%
    return EffectResult::kOk;
}
inline SoulMark make_dodge_soul_mark() {
    SoulMark sm;
    sm.id = 9998;
    sm.name = "高闪避";
    sm.description = "自身闪避率 100%（放大对方 miss 率）";
    sm.effect = &dodge_soul_effect;
    sm.args = EffectArgs(std::vector<int>{0, 1});
    return sm;
}

// 场景对战驱动器：持有 BattleContext + BattleFsm + 最小控制块。
class ScenarioBattle {
public:
    ScenarioBattle(const std::array<ElfPet, 6>& party0, const std::array<ElfPet, 6>& party1)
        : robots_{SeerRobot(party0, {}), SeerRobot(party1, {})}
        , ctx_(&cb_, robots_.data())
        , fsm_(/*enable_debug=*/true) {}

    // 驱动到 CHOOSE 等输入（GAME_START → ... → OPERATION_CHOOSE_SKILL_MEDICAMENT 停下）
    void start() { fsm_.run(&ctx_); }

    // 提交一次操作（须在 CHOOSE 等输入时；走 m_buffer 路径触发 on_selected 注册先制）
    void submit_operation(int player, BattleFsm::ActionType action, int index) {
        const int buf[4] = {player, static_cast<int>(action), index, 0};
        ctx_.m_buffer.assign(reinterpret_cast<const char*>(buf),
                             reinterpret_cast<const char*>(buf) + 4 * sizeof(int));
        ctx_.is_empty = false;
        fsm_.run(&ctx_);
    }

    // 连续驱动（跑到等输入 / FINISHED）
    void drive() { fsm_.run(&ctx_); }

    // 单步：每次执行一个状态（debug_step_mode）
    void step() {
        ctx_.debug_step_mode = true;
        fsm_.run(&ctx_);
        ctx_.debug_step_mode = false;
    }

    // 单步直到到达 target 状态（含）
    void step_until(State target) {
        while (ctx_.currentState != target && ctx_.currentState != State::FINISHED) {
            step();
        }
    }

    // 单步直到 target 状态已被执行（currentState 越过 target）
    void step_past(State target) {
        while (static_cast<int>(ctx_.currentState) <= static_cast<int>(target)
               && ctx_.currentState != State::FINISHED) {
            step();
        }
    }

    BattleContext& ctx() { return ctx_; }
    State state() const { return ctx_.currentState; }
    ElfPet& pet(int player) { return ctx_.getPet(player); }
    std::string dump() const { return ctx_.getFullStateJson(); }

private:
    StubControlBlock cb_;
    std::array<SeerRobot, 2> robots_;
    BattleContext ctx_;
    BattleFsm fsm_;
};

}  // namespace scenario

#endif  // SCENARIO_HARNESS_H
