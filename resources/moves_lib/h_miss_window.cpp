// H 组 H5 · miss/必中窗口族 —— 78×27 / 432×22（2026-09-30，worktree ../agents/h5）
//
// 家族语义（effect_info 模板 + 4399 双源逐字）：
//   78（args_num=1）"{0}回合内物理攻击对自身必定miss"
//     —— 暗影护体 20484 [3]="3回合内物理攻击对自身必定miss"、潜行 21655 [2]
//     ="2回合内，物理攻击对自身必定MISS"。**只挡物理**（SkillType::Physical），
//     特殊攻击照常命中（T2/T4 钉死）。
//   432（args_num=1）"{0}回合内对手所有攻击必定MISS，必中技能有效"
//     —— 静夜寰宇决 17618 [2]、虚化身躯 21883 [2]="2回合内对手所有攻击必定MISS，
//     必中技能有效"、天使之怒 23729 [3] 同文。全类型（物/特/属性技命中判定均走
//     hit_rate_mod 乘算链）。
//
// 内核 attack gate 语义辨析（工单点名，动工前查核）：
//   - miss 分档：本族走 **命中率层**（skills.cpp 命中判定：accuracy × ws.hit_rate_mod
//     乘算 → hit_chance；"必定 miss"= mod 压 0 → rand%100 >= 0 恒真走 MISS 出口，
//     照常 notify 消费次数类盔——文档 §2.3 语义）——与 792 封攻票（SKILL_INVALID
//     攻击无效路径、走补偿分支）**不同档**；与 86 圣洁 attribute_must_miss（属性技
//     专用必 miss，内核硬编码槽）同为"必 miss"但那个有独立槽且改内核被禁。
//   - 必中穿透：cred.must_hit 技能**整体绕过**命中判定块（skills.cpp 注释"必中技能
//     （cred.must_hit）绕过它"）→ hit_rate_mod 压 0 对必中天然无效 → 432"必中技能
//     有效"**自动满足**（内核统一行为，86 同注"必定miss治不了必中"）；78 官方文本
//     未提必中，同内核口径（必中物理技可穿 78），记档。
//   - 950 直系（f_break_round2.cpp）：hit_rate_mod=0 + register_round_effect
//     (ROUND_START) 窗口 tick 补写——"{0}回合内"含当回合双写口径（当回合直写、
//     tick 覆盖 R+1..，先手窗口 [R, R+{0})）。432 照抄 950 去掉 break 门槛。
//
// 78 的物理限定实现（本族唯一非直系点）：hit_rate_mod 无技能类型维度、且"必 miss"
// 无 per-attack 钩子（attribute_must_miss 是属性技专用内核槽）→ 可行解 =
// **回合级条件压 0**：判据时点 roundChoice 已物化（选择期在 ROUND_START 前——
// battleFsm 状态序 OPERATION_CHOOSE_* → ROUND_START → MOVE_RIGHT，482 先例佐证
// MOVE_RIGHT 时点可读、选择期更早）→ SKILL_EFFECT 当回合直判 + ROUND_START tick
// 判 `roundChoice[opp]` 选中技能 type==SkillType::Physical 才压 0（特攻回合不压、
// 非技能行动不压；压 0 恰好只影响对手本回合唯一行动）。ws 每回合 reset(1.0f) →
// 窗口每回合重写（damage_floor 同款纪律）。
//
// 动工前置三查（2026-09-30）：①内核 grep 78/432 无硬编码（唯一命中是 pet_factory
// "基础 78 int 段"套装注释，无关）②custom_effect_seed.sql 零命中 ③moves_lib 无既有
// 注册——真缺口。先例：950（miss 窗口）、86（必 miss 分档注释锚点）、482（roundChoice
// 判据时点）。
//
// 场景回归：test/scenario/scenario_830_miss_window.cpp（830-839 号段，一文件覆盖两
// 效果；miss 断言 = 被保护方 hp 不变，对照 = 掉血>0；78 物理必 miss/特攻照中、432
// 全类型必 miss/燕返必中穿透、窗口过期恢复命中）。

#include "common.h"

namespace {

constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL（moves_lib 惯例）

// ── 78 tick —— 对手本回合选中**物理**技能 → 压对手命中率修正为 0（必 miss）──
// args = [owner, target]；roundChoice 此时已物化（选择期在 ROUND_START 前，见文件头）。
EffectResult effect_78_physical_miss_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];   // 被保护方（效果属主）
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[opp][0] != kActionSelectSkill) {
        return EffectResult::kOk;   // 对手本回合非技能行动（换宠/嗑药）：无命中可压
    }
    const int slot = ctx->roundChoice[opp][1];
    const auto& skills = ctx->getPet(opp).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return EffectResult::kOk;
    }
    if (skills[static_cast<std::size_t>(slot)].type == SkillType::Physical) {
        ctx->ws.hit_rate_mod[opp] = 0.0f;   // 物理攻击对自身必 miss（特攻回合不压）
    }
    return EffectResult::kOk;
}

// ── 78 —— {0}回合内物理攻击对自身必定miss ──
// int_args = [owner, target, {0}=回合数]。
EffectResult effect_78_physical_miss_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 78, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 78, args,
                     "78 布局=[owner,target,回合数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 78,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 78, args, "需要 回合数 > 0");
    // "{0}回合内"含当回合双写（950 同款）：当回合直判（对手后手时其命中判定在本桶后）
    // + ROUND_START tick 覆盖 R+1..（先手窗口 [R, R+{0})）。
    (void)effect_78_physical_miss_tick(ctx, args);
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 78, rounds,
                          {owner, opp}, &effect_78_physical_miss_tick);
    return EffectResult::kOk;
}

// ── 432 tick —— 无条件压对手命中率修正为 0（全类型必 miss；必中由内核绕过天然有效）──
EffectResult effect_432_all_miss_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[opp] = 0.0f;   // 950 同款直压（去掉消强门槛）
    return EffectResult::kOk;
}

// ── 432 —— {0}回合内对手所有攻击必定MISS，必中技能有效 ──
// int_args = [owner, target, {0}=回合数]。
EffectResult effect_432_all_miss_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 432, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 432, args,
                     "432 布局=[owner,target,回合数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 432,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 432, args, "需要 回合数 > 0");
    (void)effect_432_all_miss_tick(ctx, args);   // 当回合立即（950 双写口径）
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 432, rounds,
                          {owner, opp}, &effect_432_all_miss_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(78, effect_78_physical_miss_window);
SKILL_EFFECT(432, effect_432_all_miss_window);
