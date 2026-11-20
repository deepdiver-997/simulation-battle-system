// G2 · 先手压制族三批 —— 792×37 / 1284×27 / 836×28（2026-09-29，worktree ../agents/g2）
//
// 792/1284 共同判据 = **先手门槛**：SKILL_EFFECT 桶被镜像 FIRST/SECOND 双侧（skills.cpp
// state_for_owner），后手方的桶跑在 BATTLE_SECOND_SKILL_EFFECT → acting_side_is_first()
// 为 false（986/1083 同判据，方向相反）。先手方的桶跑在 FIRST 侧 → 授票。
//
// 792（args_num=0）"先出手时对手当回合攻击技能无效"：
//   - seal_skill(attack=true, duration=1)：回合型封攻票**授予即生效**（1060/D7 781 先例
//     ——对手后手行动晚于先手方 SKILL_EFFECT，票已可拦），当回合末 BATTLE_ROUND_REDUCTION
//     tick 递减到 0 失效 → duration=1 恰好只覆盖"**当回合**剩余"（f_drain_family 985 时点
//     注释同源：R 直授 duration=N 会在 R 末掉一格——这里要的正是这个"当回合即逝"行为）。
//   - hit_invalid=false：普通 SEAL_ATTACK（技能按 SKILL_INVALID 处理、触发无效补偿）。
//     781 的"命中效果失效"（hit_invalid=true）是秩序之助专属口径，不外推（F3 985 同判）。
//   - 载体（37 个，切片自查 792 段全为无参一致）：绝壑袭 17659（[448,792]，448 占 7 参 →
//     792 无参）、圣婴破宇诀 18122（[792,30]，30.arg_count=0）。4399 交叉验证：18122
//     "必中；先出手时使对手本回合**攻击技能**失效"=封攻同字面；17659 转述"本回合**技能**
//     失效"——effect_info 模板（"攻击技能无效"）即权威，按封攻实现（转述掉"攻击"二字的
//     口径 F 组 1179/30809 同款）。
// 1284（args_num=1）"先出手时使对手{0}回合属性技能无效"：
//   - 同 792 形态、封属（attribute=true, attack=false）、duration={0} 跨回合窗口：InRounds
//     家族（overrides 无声明）→ 授予当回合剩余 + 后续 {0}-1 个整回合，每回合末 tick 递减。
//   - 载体：霜之教义 28019（[191,1073,1284]/[4,4,300,3]，191 占1/1073 占2 → 切片[3]）、
//     妖罗无息 28219（[1002,1284,1798,57]/[1,2,100,6,1,4,3]，1002 占1 → 切片[2]）。
// 836（args_num=1）"自身体力高于对手时先制+{0}"：
//   - 先制在选择期/MOVE_RIGHT 判定，SKILL_EFFECT 时点太晚（F9 1117 同结论）→ **CARRY
//     常驻 TEAM MOVE_RIGHT watcher**（per-side 幂等 arm，482/1117 模板）：tick 读本回合
//     选中技能的 836 记录切片 {0} → hp_self > hp_opp（"高于"=严格大于，935 同判）→
//     preemptive_level[owner] += {0}。先制比较在两个 MOVE_RIGHT 桶跑完之后 → 当回合生效。
//   - 载体：黄泉永生剑 18254（[836,153]/[2,2,2]，153 占 2 → 切片[2]）、护体金芒 24433
//     （[585,597,48,836]/[1,1,1,1,1,1,3,4,4,1]，585 占6/597 占2/48 占1 → 切片[1]）。
//
// 前置三查（2026-09-29）：①内核无 792/1284/836 硬编码（grep src/、include/ 无命中）
// ②custom_effect_overrides 无该三 id window 声明（默认 InRounds 家族）
// ③moves_lib 无既有实现（无 SKILL_EFFECT/CARRY_EFFECT(792/1284/836)）。
//
// 场景回归：test/scenario/scenario_720_first_strike.cpp（场景段 720/722-729，721 已占用）。

#include "common.h"

namespace {

// ── 792 —— 先出手时对手当回合攻击技能无效 ──
EffectResult effect_792_first_strike_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 792, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 792, args,
                     "需要 [owner, target]（args_num=0，无切片参数）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     792, args, "需要 owner/target ∈ {0,1} 且不同侧");
    EFFECT_ARG_GUARD(g_core && g_core->seal_skill, 792, args, "需要内核原语 seal_skill");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "先出手时"门槛：后手方桶在 SECOND_SKILL_EFFECT
    }
    // 当回合封攻：duration=1 授即生效、当回合末 tick 递减到 0 失效（见文件头时点论证）。
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/792,
                       /*attribute=*/false, /*attack=*/true,
                       /*count=*/0, /*duration_rounds=*/1, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1284 —— 先出手时使对手{0}回合属性技能无效 ──
EffectResult effect_1284_first_strike_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1284, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1284, args,
                     "需要 [owner, target, rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner
                         && rounds > 0, 1284, args, "需要 owner/target ∈ {0,1} 且 rounds > 0");
    EFFECT_ARG_GUARD(g_core && g_core->seal_skill, 1284, args, "需要内核原语 seal_skill");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    // 跨回合窗口封属：duration={0}（InRounds：当回合剩余 + {0}-1 个整回合）。
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/1284,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 836 CARRY tick：MOVE_RIGHT 时点结先制票 ──
// 返回选中技能里 effect_id 记录的切片首参（无该记录=非载体技 → 0；记录无切片 → 报参数
// 故障后 0）。f_misc_family selected_skill_has_effect 同款私有副本 + 取参扩展。
int g836_selected_carrier_bonus(BattleContext* ctx, int owner, int effect_id) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return 0;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return 0;
    }
    for (const official_data::SkillEffectRecord& rec :
         skills[static_cast<std::size_t>(slot)].rawEffectRecords) {
        if (rec.effect_id != effect_id) {
            continue;
        }
        if (rec.args.empty()) {
            ctx->report_effect_arg_fault(effect_id, nullptr, 0, "836 切片缺失（args_num=1）");
            return 0;
        }
        return rec.args.front();
    }
    return 0;
}

EffectResult effect_836_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int bonus = g836_selected_carrier_bonus(ctx, owner, 836);
    if (bonus <= 0) {
        return EffectResult::kOk;   // 非载体技 / 切片非法：无先制可加
    }
    // "自身体力高于对手"= 当前体力严格大于（935 同判；比较的是在场双方本回合选中期体力）。
    if (ctx->getPet(owner).hp <= ctx->getPet(1 - owner).hp) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// arm：per-side 幂等（roster 多携带者/复扫防双挂）。watcher 常驻、TEAM 活过换宠；
// 每回合 MOVE_RIGHT 桶重判"选中技能是否载体 + 体力门槛"（1117 同款形态）。
constexpr int k836ArmKeyBase = 990551200;   // +owner（键段已 grep 无撞；9905511xx=1117）

void carry_836_arm_priority_watcher(BattleContext* ctx, int owner, int skill_slot,
                                    int effect_id) {
    (void)skill_slot;
    (void)effect_id;
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    const int key = k836ArmKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return;
    }
    ctx->plugin_storage[key] = 1;
    Effect e(836, 0, owner, -1, EffectArgs(std::vector<int>{owner}),
             &effect_836_priority_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 836;
    ce->scope_ = EffectScope::TEAM;   // 活过换宠；duration<0 常驻、不可被断回合终结
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
}

}  // namespace

SKILL_EFFECT(792, effect_792_first_strike_seal_attack)
SKILL_EFFECT(1284, effect_1284_first_strike_seal_attribute)
CARRY_EFFECT(836, carry_836_arm_priority_watcher)
