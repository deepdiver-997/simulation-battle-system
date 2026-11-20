// J 组 J133 · 先制窗族 —— 1026×13 / 635×12 / 1203×12（2026-10-04，worktree ../agents/j133）
//
// 家族语义（effect_info 官方模板即权威）：
//   1026 [2] 下{0}回合，每回合都使对手的先制-{1}
//   635  [1] 吸收对手能力上升状态，吸收成功，下回合先制+{0}
//   1203 [3] 造成的伤害低于{0}则自身下{1}次技能先制+{2}
//
// 实现口径：
//   - 1026 = register_next_rounds_effect(**FIRST_MOVE_RIGHT**, opp, rounds={0}) +
//     effect_move_right_priority_bonus 负载荷 {opp, 1-opp, -{1}}（J4 1726 前半直系——
//     ⚠️ 先制修正必须挂 FIRST_MOVE_RIGHT 桶：handle_BattleFirstMoveRight 进入 memset、
//     ROUND_START 挂的值活不到比较，J115 勘误）。
//   - 635 = 吸收门 transfer_stat_boosts(opp→self)>0（J4/J19 直系）→ NextRounds(1)
//     FIRST_MOVE_RIGHT 桶 +{0}（629 的"下回合先制"语义、J115 勘误后的正位通道）。
//   - 1203 = DETECT 攻方桶（989 直系：resolvedDamage.final 严格 <{0}）→ plugin_storage
//     计数器（k1203CounterBase+owner）+= {1}；FIRST_MOVE_RIGHT probe（storage 旗防重复
//     武装、rounds=99 长驻——计数耗尽后空转无害，记档）读计数 >0 → preemptive+={2} 并
//     自减（"下{1}次技能"按选择期逐次消费）。
//
// 载体切片（unity 库 json_each 精确匹配 13+12+12=37；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   1026 [回合,降幅]：{3,2}×8/{2,2}/{4,2}/… 纯链×2（26243/35805 均 prio0）
//   635  [先制]：{1}×8/{3}×4——全族 prio=3（葬身术 19080 [3] 纯链；⚠️ 场景断言走
//     preemptive_level 读数，pri 泄漏不敏感）
//   1203 [阈值,次数,先制]：{300,1,3}×9/{300,2,2}/{350,1,2}/… 纯链×4（心灵穿刺击
//     31362 [300,1,3] 等，全 prio0）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 3 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：1726（NextRounds FIRST_MOVE_RIGHT 负载荷，j_drain_boost_success）、
//   drain_succeeded（J4）、989（DETECT final 阈值形，j_miss_drain_pierce）、
//   effect_move_right_priority_bonus（common.h 39）。
// 场景回归：test/scenario/scenario_2280_prio_window.cpp（2280 号段，一文件三宏）。

#include "common.h"

namespace {

constexpr int k1203CounterBase = 990120300;   // +owner：剩余加成次数
constexpr int k1203ProbeArmBase = 990120310;  // +owner：probe 已武装旗

int j133_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1203 probe：选择期读计数 → 加先制并自减（"下{1}次技能"逐次消费）──
EffectResult j133_1203_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int prio = a.int_args[2];
    if (self < 0 || self > 1 || prio <= 0) {
        return EffectResult::kOk;
    }
    const int key = k1203CounterBase + self;
    if (j133_stor(c, key) <= 0) {
        return EffectResult::kOk;   // 计数耗尽（长驻 probe 空转）
    }
    c->plugin_storage[key] = j133_stor(c, key) - 1;
    c->ws.preemptive_level[self] += prio;
    return EffectResult::kOk;
}

// ── 1026 —— 下{0}回合每回合对手先制-{1} ──
// int_args = [owner, target, rounds, down]（args_num=2）。
EffectResult effect_1026_opp_prio_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1026, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1026, args,
                     "1026 布局=[owner,target,回合,降幅]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1026, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && down > 0, 1026, args, "需要 回合/降幅 > 0");
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, opp, 1026, rounds,
                                {opp, 1 - opp, -down},
                                &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 635 —— 吸收对手提升成功 → 下回合自身先制+{0} ──
// int_args = [owner, target, prio]（args_num=1）。
EffectResult effect_635_drain_then_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 635, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 635, args,
                     "635 布局=[owner,target,先制]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int prio = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     635, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(prio > 0, 635, args, "需要 先制 > 0");
    if (!g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // "吸收成功"门槛
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 635,
                                /*rounds=*/1, {owner, 1 - owner, prio},
                                &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 1203 —— 伤害低于{0} → 自身下{1}次技能先制+{2} ──
// int_args = [owner, target, threshold, count, prio]（args_num=3）。
EffectResult effect_1203_lowdmg_prio_counts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1203, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1203, args,
                     "1203 布局=[owner,target,阈值,次数,先制]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int count = args.int_args[3];
    const int prio = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1203, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && count > 0 && prio > 0, 1203, args,
                     "需要 阈值/次数/先制 > 0");
    const int counter_key = k1203CounterBase + owner;
    const int arm_key = k1203ProbeArmBase + owner;
    // ⚠️ 管线 DETECT 回调**跨回合存活**（只在换宠/清局作废，DamagePipeline 无单次语义）
    //    ——必须门"仅武装那次攻击"：seq 在执行起点 ++、SKILL_EFFECT/DETECT 时已含本次，
    //    后续攻击 seq 变大即失配（989/729 等已落地 DETECT 附着族疑同坑，晨报记档）。
    const int seq_at_arm = ctx->skill_use_seq[owner];
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, count, counter_key, seq_at_arm](BattleContext* c,
                                                                   int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (c->skill_use_seq[owner] != seq_at_arm) {
                return;   // 只随武装那次攻击的红伤
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤
            }
            if (d.final >= threshold) {
                return;   // "造成的伤害低于{0}"（严格小于）
            }
            c->plugin_storage[counter_key] = j133_stor(c, counter_key) + count;
        });
    // probe 长驻（99 回合；计数耗尽后空转无害——记档）。storage 旗防重复武装。
    if (j133_stor(ctx, arm_key) == 0) {
        ctx->plugin_storage[arm_key] = 1;
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1203,
                                    /*rounds=*/99, {owner, 1 - owner, prio},
                                    &j133_1203_probe);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1026, effect_1026_opp_prio_down)
SKILL_EFFECT(635, effect_635_drain_then_prio)
SKILL_EFFECT(1203, effect_1203_lowdmg_prio_counts)
