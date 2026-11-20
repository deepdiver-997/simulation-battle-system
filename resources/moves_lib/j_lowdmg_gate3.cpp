// J 组 J151 · 低伤门族三 —— 949×10 / 1041×10 / 1050×10（2026-10-04，worktree ../agents/j151）
//
// 家族语义（effect_info 官方模板即权威）：
//   949  [1] 造成伤害不足{0}则恢复自身全部体力
//   1041 [2] 造成的攻击伤害若低于{0}则令对手下{1}次使用的攻击技能无效
//   1050 [3] 未击败对手则下{0}次使用技能后令对手{1}%{2}
//
// 实现口径：
//   - 949/1041 = DETECT 攻方桶 final **严格 <**{0}（989/541 直系）。⚠️ 必须带
//     **seq 门**（skill_use_seq==武装时值）：管线回调跨回合存活（J133 勘误——条目仅在
//     换宠/清局作废），否则后续低伤攻击会重复触发。
//   - 1041 = seal_skill(target=opp, attack=true, attribute=false, count={1},
//     duration_rounds=0, penetrable=true, hit_invalid=false, chance=100)（884/1293
//     可穿族口径——4399 只写"无效"未量化可穿性，记档）。
//   - 1050 = 897 AFTER_ACTION 桥（duration=1、acting 过滤、"未击败"=对手在场 hp>0）
//     武装计数目标 {0}（arm_round 守卫防同回合自计）+ 长驻 AFTER_ACTION 计数 probe
//     （storage 旗防重复武装、99 回合空转无害——J133 1203 同款）：每次 executed_skill
//     计 1（判据统一 2026-10-03），计满 → roll {1}% → apply_anomaly_checked（申报制，
//     J3）→ 卸任（target=0）。
//
// 载体切片（unity 库 json_each 精确匹配 10+10+10=30；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   949  [阈值]：{280}/{300} 各半——纯链×2（炫宇清波 30062 [280] 特 / 心蜃刺 31366 [300] 物）
//   1041 [阈值,次数]：{280,1}×6/{200,1}×2/{300,1}×2 无纯链——场景载体 残影绝尘断
//     30234 [280,1] pri0（链 1052✓ 无提升不发声良性）
//   1050 [次数,概率,码]：{2,50,*}×6/{2,100,1}/{1,50,0}/… 纯链×2（淡雅花香 35904
//     [2,100,1] 特 100% 确定档 / 37707 [1,50,0]）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 3 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：989（DETECT final 阈值+seq 门教训）、897（未击败 AFTER_ACTION 桥，f6）、
//   seal_skill（CoreApi 83，884 调用形）、heal_max_hp_fraction（common.h 319）、
//   apply_anomaly_checked+random_anomaly_duration（J11/J3 申报制）、1203 计数 probe（J133）。
// 场景回归：test/scenario/scenario_2460_lowdmg_gate3.cpp（2460 号段，一文件三宏）。

#include "common.h"

namespace {

constexpr int k1050TargetBase = 990105000;   // +owner：计数目标（0=未武装）
constexpr int k1050CountBase  = 990105010;   // +owner：已计次数
constexpr int k1050ArmRoundBase = 990105020; // +owner：武装回合（守卫）
constexpr int k1050ProbeArmBase = 990105030; // +owner：probe 已武装旗
// 计数 probe 的合成 id：⚠️ TimedBucket 按 (effect_id,…) 键存——与武装探针同 id 会
// 把 AFTER_ACTION 桶里的武装探针整体覆盖（本次实测）。合成 id 走 J4 kNode2444Prio 先例。
constexpr int k1050TickNodeId = 990105001;

int j151_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// AFTER_ACTION 桶的当前行动方（J23/J109 同款小工具）。
int j151_bucket_acting_side(const BattleContext* ctx) {
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

// ── 949 —— 造成伤害不足{0}则恢复自身全部体力 ──
// int_args = [owner, target, threshold]（args_num=1）。
EffectResult effect_949_lowdmg_fullheal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 949, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 949, args,
                     "949 布局=[owner,target,阈值]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     949, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0, 949, args, "需要 阈值 > 0");
    const int seq_at_arm = ctx->skill_use_seq[owner];   // seq 门（J133 勘误）
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, seq_at_arm](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            if (c->skill_use_seq[owner] != seq_at_arm) {
                return;   // 只随武装那次攻击的红伤
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            if (d.final >= threshold) {
                return;   // "伤害不足{0}"（严格小于）
            }
            heal_max_hp_fraction(c, owner, 1);
        });
    return EffectResult::kOk;
}

// ── 1041 —— 攻击伤害低于{0} → 对手下{1}次使用的攻击技能无效 ──
// int_args = [owner, target, threshold, count]（args_num=2）。
EffectResult effect_1041_lowdmg_seal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1041, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1041, args,
                     "1041 布局=[owner,target,阈值,次数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int count = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1041, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && count > 0, 1041, args, "需要 阈值/次数 > 0");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int seq_at_arm = ctx->skill_use_seq[owner];   // seq 门（J133 勘误）
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, count, seq_at_arm](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->seal_skill) {
                return;
            }
            if (c->skill_use_seq[owner] != seq_at_arm) {
                return;   // 只随武装那次攻击的红伤
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            if (d.final >= threshold) {
                return;
            }
            // "下{1}次使用的攻击技能无效"：次数型封技（884/1293 可穿族口径，记档）。
            (void)g_core->seal_skill(c, /*source=*/owner, /*target=*/target,
                                     /*effect_id=*/1041, /*attribute=*/false,
                                     /*attack=*/true, /*count=*/count,
                                     /*duration_rounds=*/0, /*penetrable=*/true,
                                     /*source_slot=*/-1, EffectScope::ON_STAGE,
                                     /*hit_invalid=*/false, /*chance_pct=*/100,
                                     /*consumed_when_pierced=*/false);
        });
    return EffectResult::kOk;
}

// ── 1050 计数 probe：每次用技 +1，计满 roll 申报异常并卸任 ──
EffectResult j151_1050_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[2];
    const int anomaly = a.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (j151_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在己方行动末计数
    }
    const int target_key = k1050TargetBase + self;
    const int count_key = k1050CountBase + self;
    const int arm_key = k1050ArmRoundBase + self;
    const int target = j151_stor(c, target_key);
    if (target <= 0 || c->roundCount <= j151_stor(c, arm_key)) {
        return EffectResult::kOk;   // 未武装 / 武装当回合不自计（arm_round 守卫）
    }
    if (!executed_skill_this_round(c, self)) {
        return EffectResult::kOk;   // "使用技能"门（判据统一 2026-10-03）
    }
    const int done = j151_stor(c, count_key) + 1;
    c->plugin_storage[count_key] = done;
    if (done < target) {
        return EffectResult::kOk;   // 还没到 {0} 次
    }
    // 计满：roll {1}% → 申报异常，卸任
    c->plugin_storage[target_key] = 0;
    c->plugin_storage[count_key] = 0;
    if (is_valid_abnormal_status_id(anomaly)) {
        (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                            self, chance, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 1050 武装探针（897 桥，duration=1）：未击败 → 武装计数目标 ──
EffectResult j151_1050_arm_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int target_count = a.int_args[2];
    if (self < 0 || self > 1 || target_count <= 0) {
        return EffectResult::kOk;
    }
    if (j151_bucket_acting_side(c) != self) {
        return EffectResult::kOk;
    }
    if (c->getPet(1 - self).hp <= 0) {
        return EffectResult::kOk;   // "未击败"门：对手在场 hp>0（897/1218/1307 判据）
    }
    c->plugin_storage[k1050TargetBase + self] = target_count;
    c->plugin_storage[k1050CountBase + self] = 0;
    c->plugin_storage[k1050ArmRoundBase + self] = c->roundCount;
    return EffectResult::kOk;
}

// ── 1050 —— 未击败对手则下{0}次使用技能后令对手{1}%{2} ──
// int_args = [owner, target, count, chance, anomaly]（args_num=3）。
EffectResult effect_1050_nodefeat_counted_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1050, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1050, args,
                     "1050 布局=[owner,target,次数,概率,异常码]");
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1050, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count > 0 && chance > 0, 1050, args, "需要 次数/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1050, args,
                     "需要 异常码 ∈ [0,43]");
    // 武装探针（897 桥）：仅本次技能使用当回合判"未击败"。
    const std::vector<int> arm_payload(args.int_args, args.int_args + args.int_count);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1050, 0, owner, -1, EffectArgs(arm_payload), &j151_1050_arm_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1050;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    // 计数 probe 长驻（storage 旗防重复武装；99 回合空转无害——J133 1203 同款）。
    const int probe_arm_key = k1050ProbeArmBase + owner;
    if (j151_stor(ctx, probe_arm_key) == 0) {
        ctx->plugin_storage[probe_arm_key] = 1;
        const std::vector<int> tick_payload{owner, 1 - owner, chance, anomaly};
        for (const State st :
             {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
            register_next_rounds_effect(ctx, st, owner, k1050TickNodeId, /*rounds=*/99,
                                        tick_payload, &j151_1050_probe);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(949, effect_949_lowdmg_fullheal)
SKILL_EFFECT(1041, effect_1041_lowdmg_seal)
SKILL_EFFECT(1050, effect_1050_nodefeat_counted_anomaly)
