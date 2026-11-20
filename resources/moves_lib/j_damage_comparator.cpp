// J 组 J255 · 伤害比较器反馈族 —— 3 宏/22 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威；三条共用同一前缀句式）：
//   632（args_num=3）"造成伤害{0}{1}，则下{2}回合必定暴击"
//   633（args_num=3）"造成伤害{0}{1}，则造成伤害的{2}%恢复体力"
//   639（args_num=4）"造成伤害{0}{1}，则下{2}回合所有技能附带{3}点固定伤害"
//
// 比较器口径（{0}）：0 = 高于（严格 >），1 = 低于（严格 <）。
//   证据：16329 翔舞煌天破同技同时挂 632 [0,350,1] 与 633 [1,350,20]——
//   同一阈值 350 的两条互补子句（高伤→下回合必暴 / 低伤→回血），说明 0/1 是方向码；
//   其余载体亦全为 0/1 两值。{1} 为阈值。只认**真正落地的红伤**（isRed && final>0）：
//   被完全挡下/未命中的 0 伤不算"造成伤害"。
//
// 实现口径（直系先例：J15 729 的 DETECT 相位回调）：
//   - 三条都在 SKILL_EFFECT 注册一条 DETECT 回调；回调里读 `ctx->resolvedDamage`
//     （attackerId/defenderId/final/isRed）。注册时把当前 `skill_use_seq[owner]` 记进
//     守卫槽，回调只认同一 token 并消费后置 -1——老回调（之前施放留下的）不会跨回合
//     误触发，同一发伤害也只结算一次。
//   - 632 = 触发后 register_next_rounds_effect(ROUND_START, {2})，tick 每回合写
//     `ws.must_crit[owner]=true`（J18 1204 / 58/681 同通道；ws 每回合 reset）。
//   - 633 = heal_amount(owner, final × {2} / 100)（恢复走原语，吃封回血/恢复修正；
//     {2} 在位即消费，显式 0 不回退）。
//   - 639 = 触发后 register_next_rounds_effect(FIRST/SECOND AFTER_ACTION, {2})，
//     tick 判 executed_skill_this_round(owner)（2026-10-03 判据统一）→
//     fixed_damage(对手, {3})；每回合用技才附带，被控/嗑药回合没有"技能"可附。
//   - 审计槽（9xxxx 私有段，场景精确断门）：
//       996320100 + owner = 632 触发时 final / -1 未触发
//       996330100 + owner = 633 触发时 final / -1 未触发
//       996390100 + owner = 639 触发时 final / -1 未触发
//       996390102 + owner = 639 窗内实际附加次数
//     守卫槽：996320200 / 996330200 / 996390200 + owner。
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算）：
//   632 [方向,阈值,回合]：17009 冰魂锁链（纯链）[0,250,2]、17646/17922/30743/30826
//     （纯链）[0,250,2]、16329 翔舞煌天破 [0,350,1]、38308 粉红波波拳 [1,300,1]。
//   633 [方向,阈值,pct]：33360 灵妙之触（纯链）[0,300,50]、18154 狂风摄魄击 [0,300,80]、
//     19865 死亡弥撒 [1,250,50]、16329 [1,350,20]、38064 神暮 [1,100,100]、
//     38335 方案提交 [0,300,100]、38500 沙海芒芒 [1,300,100]。
//   639 [方向,阈值,回合,固伤]：16964 万象天明劫（纯链）[0,300,1,300]、
//     16543 毁邪灭魔斩 [0,300,4,50]、17260 邪冥光斩 [0,250,4,50]、
//     17805 风舞耀世决 [0,300,4,100]、38116 热浪汇集 [0,300,2,200]、
//     38290 破甲角刺 [0,300,2,200]、16340 妖瞳幻魔术 [1,400,2,50]。
//
// 动工前置三查（2026-10-22）：①内核 grep 632/633/639 无行为硬编码 ②effect_info
//   模板唯一、custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集
//   （含各 worktree）无 SKILL_EFFECT(632/633/639) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5860_damage_comparator.cpp（号段 5860-5869）。

#include "common.h"

#include <functional>

namespace {

constexpr int k632AuditBase = 996320100;
constexpr int k633AuditBase = 996330100;
constexpr int k639AuditBase = 996390100;
constexpr int k639TickAuditBase = 996390102;
constexpr int k632GuardBase = 996320200;
constexpr int k633GuardBase = 996330200;
constexpr int k639GuardBase = 996390200;

// 比较器：0 = 高于（严格 >），1 = 低于（严格 <）。
bool j255_cmp_pass(int cmp, int value, int threshold) {
    return cmp == 0 ? value > threshold : value < threshold;
}

int j255_storage_int(BattleContext* ctx, int key, int fallback = -999) {
    if (!ctx) {
        return fallback;
    }
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? fallback : any_int_or(it->second, fallback);
}

// ── 632 tick：窗内每回合 ROUND_START 写必暴（J18 1204 同通道）──
EffectResult j255_632_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner >= 0 && owner <= 1) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

// ── 639 tick：窗内每回合用技 → 对手附加 {3} 点固定伤害 ──
// payload = [owner,target,fixed,arm_round]。⚠️ NextRounds 在 AFTER_ACTION 桶的
// roundCount 语义：施放回合 ROUND_START 时 roundCount==N-1，NextRounds 起点 start=R+1
// 在**施放当回合**的 AFTER_ACTION 尚未过期（roundCount < start）→ 必须用 arm_round
// 守卫挡掉当回合首跳（J70 同款：c->roundCount <= arm_round 直接跳过）。
EffectResult j255_639_fixed_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int fixed = args.int_args[2];
    const int arm_round = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount <= arm_round) {
        return EffectResult::kOk;   // 施放当回合不结算（"下{2}回合"从下一回合起）
    }
    if (!executed_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控/嗑药回合没有"技能"可附带
    }
    (void)g_core->fixed_damage(ctx, target, fixed);
    ctx->plugin_storage[k639TickAuditBase + owner] =
        j255_storage_int(ctx, k639TickAuditBase + owner, 0) + 1;
    return EffectResult::kOk;
}

// 注册一条 DETECT 回调；命中比较器后交给 on_pass(ctx, final)。
void j255_arm_detect(BattleContext* ctx, int owner, int target, int guard_base,
                     int cmp, int threshold,
                     std::function<void(BattleContext*, int)> on_pass) {
    if (!ctx || !g_core || !on_pass) {
        return;
    }
    const int token = ctx->skill_use_seq[owner];
    ctx->plugin_storage[guard_base + owner] = token;
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, guard_base, cmp, threshold, token, on_pass](BattleContext* c,
                                                                     int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (j255_storage_int(c, guard_base + owner) != token) {
                return;   // 不是本次施放武装的回调
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target) {
                return;
            }
            if (!d.isRed || d.final <= 0) {
                return;   // 未命中/被完全挡下：没有"造成伤害"
            }
            if (!j255_cmp_pass(cmp, d.final, threshold)) {
                return;
            }
            c->plugin_storage[guard_base + owner] = -1;   // 消费本次武装
            on_pass(c, d.final);
        });
}

// ── 632 —— 造成伤害{0}{1}，则下{2}回合必定暴击 ──
// int_args = [owner, target, 方向, 阈值, 回合]（args_num=3）。
EffectResult effect_632_damage_cmp_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 632, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 632, args,
                     "632 布局=[owner,target,方向,阈值,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int cmp = args.int_args[2];
    const int threshold = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     632, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(cmp == 0 || cmp == 1, 632, args, "方向码只能是 0(高于)/1(低于)");
    EFFECT_ARG_GUARD(threshold >= 0 && rounds > 0, 632, args, "需要 阈值>=0 且 回合>0");
    j255_arm_detect(ctx, owner, target, k632GuardBase, cmp, threshold,
                    [owner, target, rounds](BattleContext* c, int final_damage) {
                        c->plugin_storage[k632AuditBase + owner] = final_damage;
                        register_next_rounds_effect(c, State::BATTLE_ROUND_START, owner, 632,
                                                    rounds, {owner, target},
                                                    &j255_632_must_crit_tick);
                    });
    return EffectResult::kOk;
}

// ── 633 —— 造成伤害{0}{1}，则造成伤害的{2}%恢复体力 ──
// int_args = [owner, target, 方向, 阈值, pct]（args_num=3）。
EffectResult effect_633_damage_cmp_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 633, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 633, args,
                     "633 布局=[owner,target,方向,阈值,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int cmp = args.int_args[2];
    const int threshold = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     633, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(cmp == 0 || cmp == 1, 633, args, "方向码只能是 0(高于)/1(低于)");
    EFFECT_ARG_GUARD(threshold >= 0, 633, args, "需要 阈值 >= 0");
    j255_arm_detect(ctx, owner, target, k633GuardBase, cmp, threshold,
                    [owner, pct](BattleContext* c, int final_damage) {
                        c->plugin_storage[k633AuditBase + owner] = final_damage;
                        if (pct != 0 && g_core && g_core->heal_amount) {   // 在位即消费
                            (void)g_core->heal_amount(c, owner, final_damage * pct / 100);
                        }
                    });
    return EffectResult::kOk;
}

// ── 639 —— 造成伤害{0}{1}，则下{2}回合所有技能附带{3}点固定伤害 ──
// int_args = [owner, target, 方向, 阈值, 回合, 固伤]（args_num=4）。
EffectResult effect_639_damage_cmp_fixed_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 639, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 639, args,
                     "639 布局=[owner,target,方向,阈值,回合,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int cmp = args.int_args[2];
    const int threshold = args.int_args[3];
    const int rounds = args.int_args[4];
    const int fixed = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     639, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(cmp == 0 || cmp == 1, 639, args, "方向码只能是 0(高于)/1(低于)");
    EFFECT_ARG_GUARD(threshold >= 0 && rounds > 0, 639, args, "需要 阈值>=0 且 回合>0");
    EFFECT_ARG_GUARD(fixed > 0, 639, args, "需要 固伤 > 0");
    const int arm_round = ctx->roundCount;
    j255_arm_detect(ctx, owner, target, k639GuardBase, cmp, threshold,
                    [owner, target, rounds, fixed, arm_round](BattleContext* c,
                                                              int final_damage) {
                        c->plugin_storage[k639AuditBase + owner] = final_damage;
                        // 双探针：先/后手两个 AFTER_ACTION 时间线都覆盖（J171 同款）。
                        for (const State st : {State::BATTLE_FIRST_AFTER_ACTION,
                                               State::BATTLE_SECOND_AFTER_ACTION}) {
                            register_next_rounds_effect(c, st, owner, 639, rounds,
                                                        {owner, target, fixed, arm_round},
                                                        &j255_639_fixed_tick);
                        }
                    });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(632, effect_632_damage_cmp_must_crit)
SKILL_EFFECT(633, effect_633_damage_cmp_heal)
SKILL_EFFECT(639, effect_639_damage_cmp_fixed_window)
