// J 组 J236 · 自身负等级视图改写族 —— 2 宏/24 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1750（0 参）"无视自身能力下降状态"
//   1874（0 参）"攻击时将自身的能力下降状态视为相应的能力提升状态"
//
// 实现口径（视图改写，不落本体）：
//   - 伤害公式的攻/防读取源是 `ws.view_levels`（getTempAbilityValue），不是本体
//     `ctx->ability_levels`。两宏都在 SKILL_EFFECT 时点改写**自身**的负等级视图：
//       1750：负槽 → 0（无视下降）；
//       1874：负槽 → -level（镜像为同幅提升，-2 → +2）。
//     正槽/零槽原样不动。
//   - 原值存 plugin_storage（键段 991750xxx / 991874xxx，私有 id 段），并登记
//     FIRST/SECOND_AFTER_ACTION 探针：仅在**本次出手方 == 自身**时按掩码还原。
//     生命周期与 1767（batch_c_skills.cpp，攻击时翻转对手正槽）同款——只覆盖本次
//     技能结算，不把负等级永久洗成正等级。
//   - 审计槽（991750200 / 991874200 + owner）记录最近一次实际改写的掩码，便于场景
//     断言"前提真的成立"（本次确实有负槽被改写）。
//   - 0 参效果的装载布局 = [owner, target]（404/1767 同款），owner 即施放方。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 24/24 自洽）：
//   1750×11：35560 [1750,1568,1224,1472] / 35664 [1750,1439,1773] /
//     36021 [1750,494,1568,942,858] / 36198 [1418,1750,1115,572] /
//     36295 [1887,1750,781,1471] / 36508 [494,1750,1946,1947] /
//     36742 [1750,1652,1845] / **37097 闪光一勺 [1750]（纯链，场景载体）** /
//     37822 [1750,1454,922] / 37828 [1750,931,554] / 38120 [1750,1060]。
//   1874×13：36201 / 36391 / 36538 / **37235 兵者诡道 [1874]（纯链，场景载体）** /
//     37302 / 37809 / 37813 / 37880 / 38040 / 38132 / 38418 / 38451 / 38484。
//
// 动工前置三查（2026-10-22）：①内核 grep 1750/1874 无硬编码（ws.view_levels 是通用
//   视图层，仅供伤害公式/命中读取）②effect_info 仅两条模板、无同类效果 ③soul_lib/
//   moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）无注册
//   ——真缺口。
//
// 场景回归：test/scenario/scenario_5670_negative_view_rewrite.cpp（号段 5670-5679）。

#include "common.h"

namespace {

// 原值 / 掩码 / 审计键段（私有 id 段，避开 469x 运行时发布槽与 99xxxx 既有键）。
constexpr int k1750SaveBase = 991750000;   // +owner*8+slot：被改写槽的原等级
constexpr int k1750MaskBase = 991750100;   // +owner：本次待还原掩码（bit i = 槽 i）
constexpr int k1750AuditBase = 991750200;  // +owner：最近一次实际改写掩码（审计）
constexpr int k1874SaveBase = 991874000;
constexpr int k1874MaskBase = 991874100;
constexpr int k1874AuditBase = 991874200;

// ── AFTER_ACTION 桶的当前行动方（batch_a/1767 同款判据）──────────────────────────
int bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// plugin_storage int 槽读写（batch_c_skills.cpp 同款；any 类型不符按缺失语义回退）。
int storage_int(BattleContext* ctx, int key, int def = 0) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return def;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : def;
}

void set_storage_int(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 还原：只还本次改写过的槽（掩码），避免把同回合别家的视图改动一起洗掉 ──
EffectResult restore_impl(BattleContext* ctx, const EffectArgs& args, int save_base,
                          int mask_base) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    const int mask = storage_int(ctx, mask_base + owner, 0);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (mask & (1 << i)) {
            ctx->ws.view_levels[owner][i] = storage_int(ctx, save_base + owner * 8 + i, 0);
        }
    }
    set_storage_int(ctx, mask_base + owner, 0);
    return EffectResult::kOk;
}

EffectResult effect_1750_restore(BattleContext* ctx, const EffectArgs& args) {
    return restore_impl(ctx, args, k1750SaveBase, k1750MaskBase);
}

EffectResult effect_1874_restore(BattleContext* ctx, const EffectArgs& args) {
    return restore_impl(ctx, args, k1874SaveBase, k1874MaskBase);
}

// ── 改写：负槽按 mirror 语义改写并挂还原探针 ──
EffectResult rewrite_impl(BattleContext* ctx, const EffectArgs& args, int effect_id,
                          int save_base, int mask_base, int audit_base, bool mirror,
                          EffectFn restore_fn) {
    EFFECT_ARG_GUARD(ctx != nullptr, effect_id, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 1, effect_id, args,
                     "0 参效果布局=[owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, effect_id, args, "owner 越界");

    int mask = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int level = ctx->ws.view_levels[owner][i];
        if (level >= 0) {
            continue;   // 只处理下降状态；正槽/零槽不动
        }
        set_storage_int(ctx, save_base + owner * 8 + i, level);
        ctx->ws.view_levels[owner][i] = mirror ? -level : 0;
        mask |= (1 << i);
    }
    set_storage_int(ctx, audit_base + owner, mask);
    if (mask == 0) {
        return EffectResult::kOk;   // 本次没有下降状态：纯 no-op
    }

    set_storage_int(ctx, mask_base + owner, mask);
    // 两侧 AFTER_ACTION 都挂探针（1767 同款）：本 handler 只会在"自己出手"时运行，
    // 届时另一侧桶已过、真正会执行的是行动方侧的 AFTER_ACTION。
    const std::vector<int> payload{owner};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, owner, 1, EffectArgs(payload), restore_fn);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1750 —— 无视自身能力下降状态 ──
EffectResult effect_1750_ignore_own_drops(BattleContext* ctx, const EffectArgs& args) {
    return rewrite_impl(ctx, args, 1750, k1750SaveBase, k1750MaskBase, k1750AuditBase,
                        /*mirror=*/false, &effect_1750_restore);
}

// ── 1874 —— 攻击时将自身的能力下降状态视为相应的能力提升状态 ──
EffectResult effect_1874_drops_as_boosts(BattleContext* ctx, const EffectArgs& args) {
    return rewrite_impl(ctx, args, 1874, k1874SaveBase, k1874MaskBase, k1874AuditBase,
                        /*mirror=*/true, &effect_1874_restore);
}

}  // namespace

SKILL_EFFECT(1750, effect_1750_ignore_own_drops)
SKILL_EFFECT(1874, effect_1874_drops_as_boosts)
