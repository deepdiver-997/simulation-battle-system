#include <utils/sbs_rand.h>
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
//
// 每个断言宏执行时计数 +1，进程正常退出打印 SBS_SCENARIO_CHECKS=<n>——
// 回归 runner 据此检测"零断言场景"：占位骨架可编译可通过（claim-scenario.sh 的
// 占位设计），会把 "N/N 全绿" 稀释成假绿；没有这个计数，空壳与真测试不可区分。
#ifndef CHECK
#define CHECK(cond, ...)                                                         \
    do {                                                                         \
        ++scenario::g_scenario_checks;                                           \
        if (!(cond)) {                                                           \
            std::cerr << "\n[CHECK FAILED] " << #cond << "\n  " << __VA_ARGS__   \
                      << "\n  at " << __FILE__ << ":" << __LINE__ << std::endl;  \
            std::abort();                                                        \
        }                                                                        \
    } while (0)
#define CHECK_EQ(a, b, ...)                                                      \
    do {                                                                         \
        ++scenario::g_scenario_checks;                                           \
        if (!((a) == (b))) {                                                     \
            std::cerr << "\n[CHECK_EQ FAILED] " << #a << "="                     \
                      << static_cast<long long>(a) << "  vs  " << #b << "="      \
                      << static_cast<long long>(b) << "\n  " << __VA_ARGS__      \
                      << "\n  at " << __FILE__ << ":" << __LINE__ << std::endl;  \
            std::abort();                                                        \
        }                                                                        \
    } while (0)
#endif

// REQUIRE：assert 的 NDEBUG 免疫替代品（不受 -DNDEBUG 影响，永远执行）。
//
// 为什么不许场景测试用 <cassert> 的 assert：Release(-DNDEBUG) 会把 assert 整体
// 编译掉，而场景测试大量把**有副作用的原语调用**写进 assert（stat_change /
// stat_reversal / break_round_effects…）。Release 下调用消失 → 状态没铺设 →
// 轻则断言假通过（测试静默变空壳），重则后续 CHECK_EQ 假失败
// （scenario_030 在 Release 挂"能力等级反转"即此因，2026-09-20）。
// 用 REQUIRE 语义等价于 assert 恒开：失败打印条件原文 + 位置 + abort，
// 与 include 顺序无关（不像 #undef assert 会被后续 <cassert> 重新包含重置）。
#define REQUIRE(cond)                                                            \
    do {                                                                         \
        ++scenario::g_scenario_checks;                                           \
        if (!(cond)) {                                                           \
            std::cerr << "\n[REQUIRE FAILED] " << #cond                          \
                      << "\n  at " << __FILE__ << ":" << __LINE__ << std::endl;  \
            std::abort();                                                        \
        }                                                                        \
    } while (0)

namespace scenario {

// 断言计数与退出上报（见文件头断言宏注释）。inline 变量：每个二进制恰好一份。
// main 返回后析构打印标记行；abort 失败路径不打印——失败的场景用退出码判定，
// 计数只服务"通过但零断言"的空壳检测。
inline int g_scenario_checks = 0;
struct ScenarioCheckReporter {
    ~ScenarioCheckReporter() {
        std::cout << "SBS_SCENARIO_CHECKS=" << g_scenario_checks << std::endl;
    }
};
inline ScenarioCheckReporter g_scenario_check_reporter;

// 最小控制块：wait_for_input 空转，其余纯虚 no-op。FSM 在等输入状态会解引用它。
//
// 接口在 2026-09-17 瘦身过：IControlBlock 从 14 个方法缩到 3 个
// （wait_for_input / async_write / on_fsm_paused）。原接口上的 broadcast /
// duplicate_context / log_operation / get_fsm 等**全仓零调用**，已删除 ——
// 那些方法只服务于"一个连接塞多个对局"的旧控制块，场景测试从未用上。
struct StubControlBlock : IControlBlock {
    void wait_for_input(BattleContext*) override {}
    void async_write(int, const std::string&, BattleContext*, BattleFsm*) override {}
    void on_fsm_paused(BattleContext*) override {}
};

// 构造手动精灵：技能按 id 加载（不校验可学性），数值/魂印/通用特性手动。
inline ElfPet make_pet(int id, const std::string& name,
                       const std::array<int, 5>& skill_ids,
                       int speed, int atk, int hp,
                       SoulMark soul_mark = {},
                       CommonTrait common_trait = {}) {
    std::array<Skills, 5> skills{
        Skills(skill_ids[0]), Skills(skill_ids[1]), Skills(skill_ids[2]),
        Skills(skill_ids[3]), Skills(skill_ids[4]),
    };
    // 暴击率：引擎已按官方口径接入（`moves.crit_rate` = 分子/16，**0 = 永不必暴**）。
    // 大多数技能本来就是 0；但 DB 里有约 200 条 `crit_rate>0` 的技能（如苍炎坠 8/16 = 50%），
    // 而场景测试的伤害断言多为精确值/紧区间比较、`sbs_rand` 又未播种（序列会被每次调用移位）
    // → 默认清零暴击率，让断言可比。注意 rate==0 时暴击判定**短路、不消耗 sbs_rand()**，
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
                  std::move(common_trait), base, /*initial_hp=*/hp,
                  /*shield=*/0, /*cover=*/0,
                  /*is_locked=*/false, std::move(skills));
}

// 手工构造通用特性（不走 DB）：name 必须与官方 desc 一致（运行时按名字映射 TraitKind）。
// args 语义见 include/effects/trait_state.h（瞬杀 args[0]=千分点；精神 args={类别码,百分比}）。
inline CommonTrait make_trait(const char* name, int star_level, std::vector<int> args) {
    CommonTrait t;
    t.id = 1;
    t.name = name;
    t.star_level = star_level;
    t.description = "test";
    t.args = std::move(args);
    return t;
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

// 免弱魂印：每回合开始给 owner 挂 STAT_DROP 免疫票（窗口 9 回合，覆盖整场测试）。
// 用途：锁"附加下降被免弱挡下、附加提升照常"一类的两半独立口径。
inline EffectResult stat_drop_immune_soul_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/9, /*source_id=*/9997,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/9997);
    return EffectResult::kOk;
}
inline SoulMark make_stat_drop_immunity_soul_mark() {
    SoulMark sm;
    sm.id = 9997;
    sm.name = "免弱";
    sm.description = "自身免疫能力下降（测试桩）";
    sm.effect = &stat_drop_immune_soul_effect;
    sm.args = EffectArgs(std::vector<int>{0, 1});
    return sm;
}

// 场景对战驱动器：持有 BattleContext + BattleFsm + 最小控制块。
class ScenarioBattle {
public:
    // medicines 可选（item_id → 数量，口径见 seer-robot.h MedicineStock）。
    // 常用：{{300011, N}} = N 瓶初级体力药剂(+20体力)；{{300016, N}} = N 瓶初级活力药剂(+5PP)。
    // 嗑药（USE_MEDICINE）完全跳过出手——测"每回合使用技能"类效果的不触发分支时用。
    // equipment 可选（套装线）：每方穿戴部件 item_id；必须在 ctx_ 构造**之前**落到
    // robot 上——BattleContext 构造时拷入 worn_equipment 并派生活动套装/数值加成。
    static std::array<SeerRobot, 2> make_robots(
        const std::array<ElfPet, 6>& party0, const std::array<ElfPet, 6>& party1,
        const MedicineStock& medicines0, const MedicineStock& medicines1,
        const std::array<std::vector<int>, 2>& equipment) {
        std::array<SeerRobot, 2> robots{SeerRobot(party0, medicines0),
                                        SeerRobot(party1, medicines1)};
        robots[0].equip_item_ids = equipment[0];
        robots[1].equip_item_ids = equipment[1];
        return robots;
    }

    ScenarioBattle(const std::array<ElfPet, 6>& party0, const std::array<ElfPet, 6>& party1,
                   const MedicineStock& medicines0 = {},
                   const MedicineStock& medicines1 = {},
                   const std::array<std::vector<int>, 2>& equipment = {},
                 bool pvp = false)
        : robots_{make_robots(party0, party1, medicines0, medicines1, equipment)}
        , ctx_(&cb_, robots_.data(), /*pvp_battle=*/pvp)
        , fsm_(/*enable_debug=*/true) {
        // 效果参数解包错误 fail-fast（2026-09-28）：场景内解包失效=假绿——
        // FSM 时点跳转检查点写报告后直接终止对局，回归立即可见。
        ctx_.effect_arg_fail_fast = true;
    }

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
