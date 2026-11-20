// J 组 J17 · 先后手门族 —— 4 宏/97 载体（2026-10-02，worktree ../agents/j17）
//
// 家族语义（effect_info 模板即权威）：全部"先手/后手门"——按本回合行动序分支。
//   1247 [0] 先出手时使对手当回合内无法通过技能恢复自身体力
//   969  [1] 后出手时下回合受到的伤害降低{0}点
//   1439 [1] 对手处于能力下降状态造成的伤害提升{0}%（非窗口族：单刀条件增伤）
//   1616 [0] 当回合使用的技能克制对手时获得本系属性加成
//
// 实现口径：
//   - 先/后手门 = **行动方判定**（980/52 同判据）：SKILL_EFFECT 桶按行动方执行，
//     `acting_side_is_first(ctx)`（common.h，FIRST_* 状态域）真=先手方、假=后手方，
//     不查先手权（MOVE_RIGHT 时先手权未算出，52 注①）。
//   - 1247 = HEAL_BLOCK 票（797/1361 直系）："当回合内" → duration_rounds=1（850 当回合
//     票同款窗口），挂对手（ON_STAGE）。
//   - 969 = 463/408 点数减伤窗族（REDUCE_FLAT + MITIGATE）的 **NextRounds 单回合**版：
//     后手门过 → 窗口 [R+1, R+1]（round_effect_start_round NextRounds 显式折算，下回合
//     不含当回合）；shared_ptr<Win> 入 plugin_storage（键 969000+self）重复使用=刷新
//     （463 同形）；只减红伤（463 家族口径）。工单行"525 BLOCK 窗点数版"按 463 家族落地
//     （463 是"所受伤害减 N 点"的模板直系），记档。
//   - 1439 = 单刀条件 amp：SKILL_EFFECT 时对手任一负槽（J14 同判据）→ 直写
//     ws.damage_add_pct[owner] += {0}——写入落进**本次攻击**的 snapshot（1037 发现的
//     同一通道，本族正要它）；无回合窗 → 无需 950 双写（工单行提示按窗口版写，落地为
//     单刀版，记档）。条件在施放期求值（模板无"期间"字样，非窗口族）。
//   - 1616 = ws.force_involve 显式开关（2490 首用，battleWorkspace.h:505）：克制判定
//     **同 2490/2030 系别判据**——g_core->restraint_multiplier(技能静态系别, 对手系别视图)
//     > 1.0（克制向，2030 是微弱向 <1.0）；技能静态系别经 roundChoice 槽位取
//     `skills[idx].element`（防自污染口径，785 注）。SKILL_EFFECT 时点写 force_involve
//     来得及：伤害公式在 BATTLE_*_ATTACK_DAMAGE 读（state.h 时点序 SKILL_EFFECT →
//     ATTACK_DAMAGE），ws 每回合 reset 天然单回合有效。
//
// 载体切片（unity 库 json_each 精确匹配 26+24+23+24=97；side_effect.arg_count 切分全
// 载体自洽、零剩余，脚本核算 2026-10-02）：
//   1247 全部 0 参：33536 急速撞击 [1247]、31178 混世无极锋…
//   969  [点数]：26909 幽灵气息 [200]（属性技）、35269 开山碎岩击 [400]、33929 惊觉狂怒 [100]
//   1439 [pct]：32924 妖兽狂魂破 [50]、35376 不灭界火 [100]
//   1616 全部 0 参：38424 赴火、37210 孤烛迷徒录、3855x 系列专属技…
//
// 动工前置三查（2026-10-02）：①内核 grep 4 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 无既有注册——真缺口（force_involve/HEAL_BLOCK/REDUCE_FLAT 是既有
// 内核钩子，非硬编码）。
// 场景回归：test/scenario/scenario_1020_first_second_gates.cpp（号段 1020-1029）。

#include "common.h"

namespace {

// 对手处于能力下降状态（任一负槽；六槽含命中，J14 同判据）。
bool has_any_stat_drop(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1247 —— 先出手时使对手当回合内无法通过技能恢复自身体力 ──
// int_args = [owner, target]（args_num=0，装载器注入）。
EffectResult effect_1247_first_heal_block(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1247, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1247, args,
                     "1247 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1247,
                     args, "需要 owner/target ∈ {0,1}");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门（980 同判据：后手无事发生）
    }
    // "当回合内" = duration 1（850 当回合票同款；797/1361 的 HEAL_BLOCK 票直系）。
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/1, /*source_id=*/1247,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1247);
    return EffectResult::kOk;
}

// ── 969 —— 后出手时下回合受到的伤害降低{0}点 ──
// int_args = [owner, target, 点数]（args_num=1）。463/408 同形的 NextRounds 单回合版。
EffectResult effect_969_second_flat_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 969, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 969, args,
                     "969 布局=[owner,target,点数]");
    const int self = args.int_args[0];
    const int reduce = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     969, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(reduce > 0, 969, args, "需要 点数 > 0");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门：先手无事发生
    }
    using Win = std::pair<int, int>;   // [start, expire)
    const int key = 969000 + self;
    const int start = ctx->round_effect_start_round(self, /*rounds=*/1,
                                                    EffectWindowKind::NextRounds);
    auto win = std::make_shared<Win>(start, start + 1);
    if (ctx->plugin_storage.count(key)) {
        *std::any_cast<std::shared_ptr<Win>>(ctx->plugin_storage[key]) = *win;
        return EffectResult::kOk;   // 重复使用=刷新窗口，不叠加（463 同形）
    }
    ctx->plugin_storage[key] = win;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, reduce, win](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            const int rc = c->roundCount;
            if (rc < win->first || rc >= win->second) {
                return;   // 窗口外（[R+1, R+1]）
            }
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                return;   // 只减"所受的攻击伤害"（红伤，463 家族口径）
            }
            damage.final = std::max(0, damage.final - reduce);
        });
    return EffectResult::kOk;
}

// ── 1439 —— 对手处于能力下降状态造成的伤害提升{0}% ──
// int_args = [owner, target, pct]（args_num=1）。单刀条件 amp：写入落进本次攻击 snapshot。
EffectResult effect_1439_drop_cond_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1439, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1439, args,
                     "1439 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1439, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 1439, args, "需要 pct > 0");
    if (has_any_stat_drop(ctx, target)) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);   // 落本次攻击 snapshot
    }
    return EffectResult::kOk;
}

// ── 1616 —— 当回合使用的技能克制对手时获得本系属性加成 ──
// int_args = [owner, target]（args_num=0）。克制判定同 2490/2030 系别判据（克制向）。
EffectResult effect_1616_counter_force_involve(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1616, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1616, args,
                     "1616 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1616, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->restraint_multiplier) {
        return EffectResult::kOk;
    }
    // 技能静态系别（防自污染口径，785 注）：经 roundChoice 槽位取 skills[idx].element。
    const int idx = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (idx < 0 || idx >= static_cast<int>(skills.size())) {
        return EffectResult::kOk;
    }
    const int* atk_elem = skills[static_cast<std::size_t>(idx)].element;
    if (g_core->restraint_multiplier(atk_elem, ctx->ws.view_elementalAttributes[target]) > 1.0) {
        ctx->ws.force_involve[owner] = true;   // 伤害公式 ATTACK_DAMAGE 读（时点序来得及）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1247, effect_1247_first_heal_block)
SKILL_EFFECT(969, effect_969_second_flat_reduce)
SKILL_EFFECT(1439, effect_1439_drop_cond_amp)
SKILL_EFFECT(1616, effect_1616_counter_force_involve)
