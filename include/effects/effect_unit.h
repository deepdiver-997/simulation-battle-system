#ifndef EFFECT_UNIT_H
#define EFFECT_UNIT_H

#include <effects/effect.h>
#include <primitives/battle_primitives.h>

// 条件效果单元（效果组合语法第一刀）。
//
// 效果 = 主动作（原语调用）→ 归一化返回值 → if-else 分支。
// 执行器同步调用原语拿细码（apply_anomaly 的 TARGET_IMMUNE 等），归一化为 BranchKey，
// 走对应分支——分支是执行器局部变量，不需要全局结果槽。
// 复杂效果 = 单元递归组合；Type B 补偿（被免疫/被阻 → 补偿）就是 on_immune/on_blocked 分支。
//
// 数据驱动方向：EffectUnit 后续从 effect_info 模板 + effect_meta 生成
// （"{n}%令对手{...}" → Anomaly 主动作 + 概率；"未触发则" → 兜底分支），本轮先手工构造。

enum class PrimitiveTag {
    Anomaly,       // apply_anomaly：param0=anomaly_id, param1=duration_rounds
    StatChange,    // stat_change：param0=stat, param1=delta
    Heal,          // heal：param0=fraction_denom（>0=1/denom 最大体力；<=0=全部）
    FixedDamage,   // fixed_damage：param0=amount（固定伤害）
    // 第二刀（组合语法：无相谛 5 类条件模板）
    PpReduce,           // pp_reduce：param0=每技能减 N 点 PP
    RemoveRoundEffects, // remove_round_effects（消除目标回合类效果，复用 break_round_effects）
    DrainHp,            // drain_hp：param0=fraction_denom（吸取目标 max_hp/denom 固定伤害+自身恢复等量）
    Kill,               // kill：目标体力归 0（秒杀）
    PowerBoost,         // 改 ws.skill_power_view[actor] += param0（技能威力视图层，非原语）
};

// 前置条件（执行主动作前求值；不满足则效果不触发，走 on_other/无动作）。
enum class UnitCondition {
    None,           // 无条件
    SameElement,    // 双方主元素属性相同
    FirstMove,      // 本回合先出手（当前执行状态是 FIRST 系列）
    SecondMove,     // 本回合后出手（SECOND 系列）
    TargetNoAnomaly,// 目标不处于任何异常状态
    TargetHasAnomaly, // 目标处于至少一个异常状态
    TargetHpBelow,  // 目标当前体力 < condition_param
};

// 原语返回值归一化的分支键。
enum class BranchKey {
    Success,       // 成功（SUCCESS/REPLACED/DURATION_EXTENDED）
    Immune,        // 被免疫/天生机制挡住（Type B 补偿落点）
    Blocked,       // 被其他效果阻止
    TargetDefeated,
    AtCap,         // 到上限/下限（能力等级）
    Invalid,       // 参数无效
    Never,         // 概率未触发（chance roll 失败）
};

struct EffectUnit {
    PrimitiveTag primary_tag = PrimitiveTag::Anomaly;
    // 相对技能持有方：0=自身, 1=对手。执行器从 args.int_args[0]（bind_participants
    // 绑定）解析真实 owner；args 无 owner 时回退 actor 字面量。
    int actor = 0;   // 主动作发起方（相对）
    int target = 1;  // 主动作目标（相对：0=自身, 1=对手）
    int param0 = 0;  // 原语参数0（anomaly_id / stat / fraction_denom / amount）
    int param1 = 0;  // 原语参数1（duration / delta）
    int chance_arg = -1;    // 动态概率参数下标（args.int_args 下标；-1=无）
    int chance_value = -1;  // 字面概率（-1=必定执行；与 chance_arg 二选一，chance_value 优先）

    // 前置条件（第二刀）：不满足则效果不触发（走 on_other/无动作），先于概率 roll。
    UnitCondition condition = UnitCondition::None;
    int condition_param = 0;  // TargetHpBelow 阈值等

    // 分支动作（递归子单元；nullptr=无动作）。chance 未触发/条件不满足走 on_other。
    const EffectUnit* on_success = nullptr;
    const EffectUnit* on_immune = nullptr;   // Type B：被免疫 → 补偿
    const EffectUnit* on_blocked = nullptr;
    const EffectUnit* on_other = nullptr;    // 其余失败键（含概率未触发）
};

// 执行器（sim_core 内驻；原语在 sim_core，插件调不到）。
// 概率 roll → 调原语 → 归一化细码为 BranchKey → 递归执行分支。
// 返回最终归一化 BranchKey（测试/上层观测用）。
BranchKey execute_effect_unit(BattleContext* ctx, const EffectArgs& args, const EffectUnit& unit);

// **跳过本单元条件**的执行器：条件视为已满足（概率 roll 照常）。
// 用途 = 单元准入门（`UnitAdmissionFn`）：魂印侧记录了"取消进度"时，注册期就把该单元
// 换成这个执行器注册，从而**不改技能对象**也能表达"该条已永久无条件"。
// ⚠️ 只跳过**本单元顶层**的条件；递归进 on_success/on_other 等分支子单元时各自按条件判。
BranchKey execute_effect_unit_unconditional(BattleContext* ctx, const EffectArgs& args,
                                            const EffectUnit& unit);

// ── 单元准入门（core 侧存储，插件经 IEffectRegistry::registerUnitAdmission 写入）──
// core 的注册路径（Skills::register_branch）逐单元询问；任一钩子返回 true → 该单元跳过
// 条件求值。见 plugin_interface.h 的 UnitAdmissionFn 注释（为什么要有这个东西）。
// 存储进程全局：注册表实例可能是 EffectFactory 也可能是 SoulMarkManager，两处都写到这儿。
// 用 typedef（非 using 别名）：两个头都要声明它，typedef 允许相同类型的重复声明。
typedef bool (*UnitAdmissionFn)(BattleContext*, int owner, int skill_id, int unit_index,
                                int condition);
void add_unit_admission(UnitAdmissionFn fn);
// 逐单元询问：无钩子返回 false（照常按条件求值）。
bool unit_admission_grants(BattleContext* ctx, int owner, int skill_id, int unit_index,
                           int condition);

#endif // EFFECT_UNIT_H
