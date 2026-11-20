// J 组 J251 · 条件必暴族 —— 2 宏/16 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1639（0 参）"自身处于能力提升状态时100%打出致命一击"
//   1590（0 参）"自身体力低于对手时100%的概率打出致命一击"
//
// 实现口径（直系先例 j_prob_must_crit.cpp 1429/1420 / D26 1395 通道）：
//   - 暴击掷点在 ON_SKILL_HIT（早于 SKILL_EFFECT）→ ws.must_crit 不能覆盖本次攻击；
//     当前攻击必暴的既验通道 = SKILL_EFFECT 时点直接置 `ctx->crit_happened[owner]`，
//     ATTACK_DAMAGE 阶段只消费该旗标（battleFsm.cpp:693-710），并按
//     `ws.cached_crit_damage`（默认 200）放大整段。
//   - 1639 门 = 自身六槽（含命中）任一正等级（"处于能力提升状态"，J51/1103/1420 同判据）。
//     起始即 +6 的槽同样算"处于提升状态"——门只看 >0。
//   - 1590 门 = 自身当前体力严格小于对手当前体力（hp <，相等不触发）。
//   - 审计槽（场景精确断门，键段 9xxxx 私有）：
//       991639100 + owner = 1 门过（必暴）/ -1 门闭
//       991590100 + owner = 1 门过（必暴）/ -1 门闭
//     +owner 而不是 +owner*2：两条宏键段不同，不会互撞。
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算）：
//   1639 全 8/8 零参：35711 拥荧入怀 [1639,991]（991=未击败闪避窗，对手 NONE 时不跑）、
//     35329 无畏盾击 [1639,1449]、35176 天谴·末日无生、34762 霜月圆舞、35377 九转轮回天、
//     35869 怨甲垢痕、37402 突进兵装、38058 金坪镇华殿。
//   1590 全 8/8 零参：34950 神鸟扑击（纯链）、35607 地雷重击（纯链）、
//     35668 彻夜之光（纯链）、35674 弱光打击（纯链）、34304/35479/36189/37161。
//
// 动工前置三查（2026-10-22）：①内核 grep 1639/1590 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 无行 ③全库 moves_lib/soul_lib 并集（含各 worktree）
//   无 SKILL_EFFECT(1639/1590) 注册（soul_lib 注释里的 1639 是魂印 id 空间，不相干）——真缺口。
//
// 场景回归：test/scenario/scenario_970_cond_crit.cpp（号段 970-979）。

#include "common.h"

namespace {

constexpr int k1639AuditBase = 991639100;   // +owner：1=门过（必暴）/ -1=门闭
constexpr int k1590AuditBase = 991590100;   // +owner：1=门过（必暴）/ -1=门闭

// 自身是否有任一正等级（"处于能力提升状态"；J51/1103/1420 同判据）。
bool j251_has_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 1639 —— 自身处于能力提升状态时100%打出致命一击（0 参） ──
// int_args = [owner, target]。
EffectResult effect_1639_boosted_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1639, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1639, args,
                     "1639 布局=[owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1639, args, "需要 owner/target ∈ {0,1}");
    if (!j251_has_boost(ctx, owner)) {
        ctx->plugin_storage[k1639AuditBase + owner] = -1;   // 门闭
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1639AuditBase + owner] = 1;
    ctx->crit_happened[owner] = true;   // 当前攻击必暴（D26 1395 通道）
    return EffectResult::kOk;
}

// ── 1590 —— 自身体力低于对手时100%打出致命一击（0 参） ──
// int_args = [owner, target]。
EffectResult effect_1590_low_hp_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1590, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1590, args,
                     "1590 布局=[owner,target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1590, args, "需要 owner/target ∈ {0,1}");
    if (ctx->getPet(owner).hp >= ctx->getPet(target).hp) {
        ctx->plugin_storage[k1590AuditBase + owner] = -1;   // 门闭（相等不触发）
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1590AuditBase + owner] = 1;
    ctx->crit_happened[owner] = true;   // 当前攻击必暴（D26 1395 通道）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1639, effect_1639_boosted_crit)
SKILL_EFFECT(1590, effect_1590_low_hp_crit)
