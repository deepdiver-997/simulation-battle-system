// J 组 J311 · 伤害高于门→下N回合先制族 —— 591×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   591（args_num=3）"造成伤害大于{0}，则下{1}回合自己所有直接攻击先制+{2}"
//
// 直系先例（照抄口径，不另造机制）：
//   - **触发面** = J255 632/633/639「伤害比较器反馈族」同款：SKILL_EFFECT 时点武装一条
//     伤害管线 **DETECT 相位**回调，回调里读 `ctx->resolvedDamage`
//     （attackerId/defenderId/final/isRed）——J15 729 伤害阈值门同款。
//   - **授予面** = 481「下{0}回合自身**攻击**先制+{1}」同款（round_window.cpp）：
//     `register_next_rounds_effect(State::BATTLE_FIRST_MOVE_RIGHT, …)`（"下N回合"=
//     NextRounds，起点 = 触发回合 +1）+ MOVE_RIGHT 桶 tick 写 `ws.preemptive_level`。
//     攻击限定用 `selected_skill_is_attack`（common.h，**选择期语义**：读 roundChoice 的
//     静态 type，MOVE_RIGHT 时技能尚未执行 → 可用；属性技不吃）。
//
// 口径决策（写死并记档）：
//   - "大于{0}" = **严格大于**阈值（final > threshold；等于阈值不触发）。
//   - 只认**落地红伤**（isRed）：属性技无伤害相位自然不触发；被完全挡下 final=0
//     时 0 ≯ 阈值 → 门不开。
//   - "自己所有直接攻击" = 效果持有者自己的**攻击技**（物理/特殊）——与 481 的"自身攻击"
//     同一口径（"直接攻击"= 非属性技）；**对手**的技能不受影响。
//   - 窗口 = "下{1}回合" = NextRounds（触发回合的**下一回合**起算，N 回合），
//     先/后手都从下回合起算（round_effect_start_round 的 NextRounds 分支）。
//   - 行参在位即消费（红线7）：回合=0 → 不开窗（register_next_rounds_effect 自己拒收
//     rounds<=0）；先制=0 → tick 直接跳过（481 同款）；阈值=0 → 任何落地红伤都触发。
//   - 多段攻击（1141 族）一技多打：首段命中即消费授权（守卫槽置 -1），后续段不再触发。
//
// 载体切片（side_effect.arg_count 优先 / args_num 兜底，2026-10-22 脚本核算）：
//   591 [阈值,回合,先制]：16629 极光追魂灭 [591]|[260,2,1]（**纯链**，场景载体，物 150）、
//        16835 噬血杀 [591]|[300,1,1]（**纯链**，物 80）、
//        16169 魔帝千裂破 [594,591]|[300,1,1]（594 已实现：伤害低于阈值→%令对手异常）、
//        16626 苍穹斗转决 [43,591,594]|[280,2,1]、17144 黑死波脉冲 [591]|[250,2,1]、
//        30435 万灵兽奔 [959,591]|[280,2,1]、36812 多菲跃滔震 [856,591,864]|[280,3,1]
//   → 阈值 ∈ {250,260,280,300}；回合 ∈ {1,2,3}；先制恒 1。
//
// 动工前置三查（2026-10-22）：①内核 grep 591 无行为硬编码 ②effect_info 模板仅本条
//   （无同文其它 id）③全库 moves_lib/soul_lib 并集（jbatch/kbatch/hub/agents）
//   无 SKILL_EFFECT(591) 注册——真缺口。
//
// 审计槽（9xxxx 私有段，场景精确断门；+owner*1）：
//   991591100 + owner = 591 触发那一发的 final 伤害（-999 = 未触发）
//   991591101 + owner = 591 授予的窗口回合数（{1}）
//   991591200 + owner = 591 DETECT 守卫 token（-1 = 已消费）
//
// 场景回归：test/scenario/scenario_6440_damage_over_priority.cpp（号段 6440-6449）。

#include "common.h"

#include <functional>

namespace {

constexpr int k591AuditBase = 991591100;   // +owner：触发那一发的 final 伤害
constexpr int k591GrantBase = 991591101;   // +owner：授予的窗口回合数
constexpr int k591GuardBase = 991591200;   // +owner：DETECT 守卫 token

int j311_storage_int(const BattleContext* ctx, int key, int fallback = -999) {
    if (!ctx) {
        return fallback;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return fallback;
    }
    return any_int_or(it->second, fallback);
}

// MOVE_RIGHT tick：只有**攻击技**吃先制（"所有直接攻击"限定；481/J4 同判据）。
EffectResult j591_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // "自己所有**直接攻击**"：属性技不吃
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// 武装一条 DETECT 回调：本次施放造成的红伤 final **严格大于** threshold → on_pass(c, final)。
// 守卫三道（防"上一次施放留下的回调"跨回合误触发，J255 令牌守卫的加强版）：
//   ① 守卫槽 == 本次武装的 token；② skill_use_seq[owner] 仍等于 token；
//   ③ attackerId/defenderId 匹配本次施放双方。
void j311_arm_detect(BattleContext* ctx, int owner, int target, int guard_base, int threshold,
                     std::function<void(BattleContext*, int)> on_pass) {
    if (!ctx || !on_pass) {
        return;
    }
    const int token = ctx->skill_use_seq[owner];
    ctx->plugin_storage[guard_base + owner] = token;
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, guard_base, threshold, token, on_pass](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (j311_storage_int(c, guard_base + owner) != token) {
                return;   // 不是本次武装的回调
            }
            if (c->skill_use_seq[owner] != token) {
                return;   // 期间又用过别的技能 → 陈旧回调
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target) {
                return;
            }
            if (!d.isRed || d.final <= 0) {
                return;   // 没有"造成伤害"（miss / 被完全挡下）
            }
            if (d.final <= threshold) {
                return;   // "大于{0}" = 严格大于阈值
            }
            c->plugin_storage[guard_base + owner] = -1;   // 消费本次武装（多段只触发首段）
            on_pass(c, d.final);
        });
}

// ── 591 —— 造成伤害大于{0}，则下{1}回合自己所有直接攻击先制+{2} ──
// int_args = [owner, target, 阈值, 回合, 先制]（args_num=3）。
EffectResult effect_591_damage_over_next_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 591, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 591, args,
                     "591 布局=[owner,target,阈值,回合,先制]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int rounds = args.int_args[3];
    const int bonus = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     591, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold >= 0 && rounds >= 0, 591, args, "需要 阈值>=0 且 回合>=0");
    j311_arm_detect(ctx, owner, target, k591GuardBase, threshold,
                    [owner, rounds, bonus](BattleContext* c, int final_damage) {
        c->plugin_storage[k591AuditBase + owner] = final_damage;
        c->plugin_storage[k591GrantBase + owner] = rounds;
        // "下{1}回合" = NextRounds（起点 = 触发回合 +1）；窗口挂 MOVE_RIGHT 桶（481 同款）。
        register_next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, owner, 591, rounds,
                                    {owner, bonus}, &j591_priority_tick);
    });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(591, effect_591_damage_over_next_priority)
