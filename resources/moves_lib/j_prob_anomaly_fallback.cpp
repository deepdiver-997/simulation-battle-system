#include <utils/sbs_rand.h>
// J 组 J3 · 概率异常未触发族 —— 21 宏 / 113 载体（2026-10-01，worktree ../agents/j3）
//
// 家族语义（effect_info 模板全族同构 + 4399 双源逐字核对）：
//   "{0}%令对手{1}，未触发则<兜底Y>"——{1} = battle_effects 异常码（≤43，
//   is_valid_abnormal_status_id 校验）。主干 = 现代参数化施加（756 同款）走
//   apply_anomaly_checked 申报制；"未触发" = !anomaly_landed(r)（含 ROLL_FAILED
//   与各被挡档：免疫/抗性/弹控反弹——916/1255/19531 同口径，batch_a 注释原文）。
//
// 21 宏与兜底分支（args_num = side_effect.arg_count 全一致，无 R1 型分歧）：
//   1111 [4] 兜底={2}回合内自身造成的攻击伤害额外提升{3}% —— "额外提升"=
//            AMP_EXTRA 乘法通道（693 判据）；窗口=当回合直写 + ROUND_START tick
//            重写（950 双写口径，ws 每回合 reset）
//   1231 [3] 兜底=附加对手最大体力1/{2}百分比伤害（1255 同款 PERCENT_VALUE 粉）
//            ⚠️ 时点：模板无时点词；4399 两载体措辞分裂（时分公主"回合结束时"/
//            柯尔霍德立即）→ 从模板无"回合结束后"字样（2068 有）取**立即**，记档
//   1278 [3] 兜底=免疫下{2}次攻击伤害并反弹等量固定伤害 —— 816 弦月锋华
//            BLOCK 闭包变体（拿归零前 final），反馈改 FIXED 粉伤 100%
//   1335 [4] 兜底=随机吸取对手{2}项属性-{3}并附加给自己 —— effect_batch8 425
//            直系同文（stat_drop 对手吃免弱 + stat_change 自己独立必成）
//   1390 [4] 兜底={2}回合内每回合{3}%闪避对手攻击 —— hit_rate_mod[opp] 乘算
//            压低（78/950 命中率层；100% 档=必 miss，必中技可穿=内核统一行为）
//   1482 [2] 兜底=吸取对手能力提升状态 —— transfer_stat_boosts(target→self) 直通
//   1660 [3] 兜底=降低对手所有技能{2}点PP —— pp_reduce 直通（清 PP 必走原语）
//   1727 [4] 兜底=免疫下{2}次攻击伤害并反弹此攻击伤害{3}%固定伤害 —— 816 闭包
//            变体；4399 莫伊莱原文"附加给对手此攻击伤害100%的固定伤害"
//   1788 [3] 兜底=附加自身最大体力{2}%百分比伤害，对手免粉则转真实伤害 ——
//            is_immune(PINK_DAMAGE) 预查询（zhakes 4762 同款）
//   1819 [3] 兜底=自身免疫下{2}次受到的异常状态 —— 2059 同款 counts 票
//            （ON_STAGE 不可传承口径）
//   1823 [11] 三段级联：{0}%令{1}→未触发{2}%令{3}→均未触发令{4}的六槽变化。
//            {4}=对象旗（全库实载 1=对手）；args[7..12]=**语义序**六槽
//            （攻,防,特攻,特防,速,命中）→引擎槽 kStatMap{0,2,1,3,4,5}——PLD
//            后备核心铀 4399 原文"防御-2，特防-2，速度-2，命中-2"与切片
//            [1, 0,-2,0,-2,-2,-2] 逐位互证。对手负向走 stat_drop（吃免弱）
//   1824 [3] 兜底=对手每存在1层处决印记自身免疫{2}次异常 —— ⚠️ 记档：处决
//            印记（链前 1010/1124 族所在的效果群）引擎尚无表示，层数恒 0 →
//            兜底恒空转；主干照常实现，印记原语落地后此处接 counts 查询
//   1837 [3] 兜底=令自身全属性-{2} —— all_stats_up（自降走 stat_change 不吃免弱）
//   1862 [3] 兜底=降低自身所有技能{2}点PP —— pp_reduce(self)
//   1929 [3] 兜底=随机附加{2}种异常状态 —— attach_random_anomalies 全池
//            anomaly_pool_all()，兜底本身必中（probability_pct=100）
//   2005 [3] 兜底=为对手附加{2}点护盾 —— shield_bank_.add（4399 大雪纷飞原文
//            "为对手附加600点护盾"——盾上对手不是笔误）；普通盾 priority=0。
//            ⚠️ 载体 29000（地面守护）链切片欠参：1932(2)+1372(3)+2005(3)=8 参
//            但 DB 只给 6 → 2005 恒守卫拦截（R 组数据工单候选记档）；
//            场景用 28319（大雪纷飞 [100,6,600] 切片完整）作载体
//   2068 [2] 兜底=回合结束后消除对手回合类效果 —— 一次性 ROUND_END tick
//            （1655 注册面，rounds=1 InRounds 覆盖当回合）
//   2096 [4] 兜底=未触发**或**对手处于{2}状态 → {3}回合内免疫对手的攻击 ——
//            570 同款 DAMAGE 票但窗口型（counts=0 + duration_rounds={3}，
//            PINK_DAMAGE 注释"窗口型每段都查"）
//   2176 [2] 兜底=令对手威力最高的攻击技能PP归0 —— 逐槽扫 power（非属性技、
//            pp>0），pp_zero_slot 原语（2076 批 8 修正线同款）
//   2530 [2] 兜底=解除自身所处的异常状态 —— dispel_active_anomalies(self)
//   2533 [3] 兜底=恢复自身所有技能{2}点PP —— pp_restore(self, only_empty=false)
//
// 动工前置三查（2026-10-01）：①内核 grep 21 id 无行为硬编码（2533 仅 effect_meta
//   分类锚点 StatusInflict，"规则可命中作回归锚点"）②custom_effect_seed.sql 零命中
//   ③moves_lib 无既有注册——全族真缺口。载体数核对：全族 113 载体与工单一致。
//   载体切片抽查 30+ 条全部吻合 [chance, anomaly, ...] 布局。
// 先例：916（未触发口径）、1255（max_hp 粉伤）、816（免疫量反馈闭包）、425（随机
//   吸取）、2059（异常次免票）、570（攻击免票）、78/950（命中率层窗口）、1655
//   （ROUND_END tick）、zhakes 4762（免粉预查询）。
// 场景回归：test/scenario/scenario_900_prob_anomaly_fallback.cpp（900-904 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <cstdlib>
#include <vector>

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（encarlos kKingStatMap 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

// 现代施加的"落地"判定（batch_a/burn_trio anomaly_landed 同口径）。
bool j3_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 主干共体：{0}%令对手{1}（申报制，916 同口径）；返回是否落地 ──
bool j3_try_main(BattleContext* ctx, int owner, int target, int chance, int anomaly) {
    if (!g_core || !g_core->apply_anomaly_checked || chance <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return false;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    return j3_landed(r);
}

// ── 1111 兜底 tick —— 每回合把"攻击伤害额外提升{3}%"写回 AMP_EXTRA 首空槽 ──
// args=[owner, target, pct]；ws 每回合 reset → 窗口每回合重写（950 双写纪律）。
EffectResult j3_1111_amp_extra_tick(BattleContext* ctx, const EffectArgs& args) {
    return effect_set_damage_amp_extra(ctx, args);
}

// ── 1390 兜底 tick —— 每回合把对手命中率修正乘 (1-{3}%) ──
// args=[owner, opp, pct]；ws.hit_rate_mod 每回合 reset(1.0f) → 每回合乘算重写。
EffectResult j3_1390_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[opp] *= (1.0f - static_cast<float>(pct) / 100.0f);
    return EffectResult::kOk;
}

// ── 2068 兜底 tick —— 回合结束一次性消除对手回合类效果 ──
EffectResult j3_2068_break_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    (void)g_core->break_round_effects(ctx, args.int_args[1]);
    return EffectResult::kOk;
}

// ── 1278/1727 共体 —— 免疫下{count}次攻击伤害并反馈免疫量的{pct}%固定伤害 ──
// 816 弦月锋华 BLOCK 闭包变体：落在 BLOCK 相位拿**归零前 final**（次数型免伤票据
// 的默认回调先归零、取不到值，816 原注释）；计数放闭包状态（用两次=两条各自计数）。
void j3_arm_immune_reflect_fixed(BattleContext* ctx, int self, int count, int pct) {
    if (!ctx || self < 0 || self > 1 || count <= 0) {
        return;
    }
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, remaining = count, pct](BattleContext* c, int bucket_owner) mutable {
            if (!c || bucket_owner != self || remaining <= 0 || !g_core) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只免疫"对手的攻击伤害"（红伤）
            }
            const int attacker = d.attackerId;
            const int immune_amount = d.final;   // 免疫量 = 被归零前的伤害值
            if (attacker < 0 || attacker > 1) {
                return;
            }
            --remaining;
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            const int reflect = immune_amount * pct / 100;
            if (reflect > 0 && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(c, attacker, reflect, DamageKind::FIXED, self);
            }
        });
}

}  // namespace

// ════════════════════════ 21 宏 ════════════════════════

// ── 1111 —— {0}%令对手{1}，未触发则{2}回合内自身造成的攻击伤害额外提升{3}% ──
EffectResult effect_1111_chance_or_amp_extra(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1111, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1111, args,
                     "1111 布局=[owner,target,概率,异常,回合,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int rounds = args.int_args[4];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1111, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1111, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, chance, args.int_args[3])) {
        return EffectResult::kOk;
    }
    // 兜底：AMP_EXTRA 乘法通道窗口（当回合直写 + ROUND_START tick 重写）
    if (rounds > 0 && pct != 0) {
        std::vector<int> tick_args{owner, target, pct};
        effect_set_damage_amp_extra(ctx, EffectArgs(tick_args));   // 当回合直写
        register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 1111, rounds,
                              std::move(tick_args), &j3_1111_amp_extra_tick);
    }
    return EffectResult::kOk;
}

// ── 1231 —— {0}%令对手{1}，未触发则附加对手最大体力1/{2}的百分比伤害 ──
EffectResult effect_1231_chance_or_maxhp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1231, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1231, args,
                     "1231 布局=[owner,target,概率,异常,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1231, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1231, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (denom > 0 && g_core->deal_pink_damage && ctx->getPet(target).hp > 0) {
        g_core->deal_pink_damage(ctx, target, max_hp_of(ctx, target) / denom,
                                 DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1278 —— {0}%令对手{1}，未触发则免疫下{2}次受到的攻击伤害并反弹等量的固定伤害 ──
EffectResult effect_1278_chance_or_immune_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1278, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1278, args,
                     "1278 布局=[owner,target,概率,异常,次数]");
    const int owner = args.int_args[0];
    const int count = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1278, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1278, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    j3_arm_immune_reflect_fixed(ctx, owner, count, 100);   // 等量 = 100%
    return EffectResult::kOk;
}

// ── 1335 —— {0}%令对手{1}，未触发则随机吸取对手{2}项属性-{3}，并将该属性附加给自己 ──
// effect_batch8 425 直系同文：两个独立动作（stat_drop 对手吃免弱 / stat_change 自己必成）。
EffectResult effect_1335_chance_or_stat_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1335, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1335, args,
                     "1335 布局=[owner,target,概率,异常,项数,级数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[4];
    const int levels = std::abs(args.int_args[5]);
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1335, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1335, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, opp, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (count <= 0 || levels <= 0 || !g_core->stat_drop || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    std::vector<int> pool;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        pool.push_back(i);
    }
    for (int k = 0; k < count && !pool.empty(); ++k) {
        const int pick = sbs_rand() % static_cast<int>(pool.size());
        const int stat = pool[static_cast<std::size_t>(pick)];
        pool.erase(pool.begin() + pick);
        g_core->stat_drop(ctx, opp, stat, levels, owner);   // ① 对手下降（免弱可挡）
        g_core->stat_change(ctx, owner, stat, levels);      // ② 自己提升（独立必成）
    }
    return EffectResult::kOk;
}

// ── 1390 —— {0}%令对手{1}，未触发则{2}回合内每回合{3}%闪避对手攻击 ──
EffectResult effect_1390_chance_or_dodge_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1390, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1390, args,
                     "1390 布局=[owner,target,概率,异常,回合,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[4];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1390, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1390, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, opp, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (rounds > 0 && pct > 0) {
        std::vector<int> tick_args{owner, opp, pct};
        j3_1390_dodge_tick(ctx, EffectArgs(tick_args));   // 当回合直写（950 双写口径）
        register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 1390, rounds,
                              std::move(tick_args), &j3_1390_dodge_tick);
    }
    return EffectResult::kOk;
}

// ── 1482 —— {0}%令对手{1}，未触发则吸取对手能力提升状态 ──
EffectResult effect_1482_chance_or_steal_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1482, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1482, args,
                     "1482 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1482, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1482, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts) {
        (void)g_core->transfer_stat_boosts(ctx, target, owner);
    }
    return EffectResult::kOk;
}

// ── 1660 —— {0}%令对手{1}，未触发则降低对手所有技能{2}点PP值 ──
EffectResult effect_1660_chance_or_pp_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1660, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1660, args,
                     "1660 布局=[owner,target,概率,异常,点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int points = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1660, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1660, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (points > 0 && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, target, points);
    }
    return EffectResult::kOk;
}

// ── 1727 —— {0}%令对手{1}，未触发则免疫下{2}次所受攻击的伤害并反弹{3}%的百分比伤害 ──
// 4399 莫伊莱原文："未触发则免疫对手下1次攻击技能造成的伤害，并附加给对手此攻击
// 伤害100%的固定伤害"——反弹量 = 免疫量 × {3}%、FIXED 粉伤。
EffectResult effect_1727_chance_or_immune_reflect_pct(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1727, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1727, args,
                     "1727 布局=[owner,target,概率,异常,次数,pct]");
    const int owner = args.int_args[0];
    const int count = args.int_args[4];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1727, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1727, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    j3_arm_immune_reflect_fixed(ctx, owner, count, pct);
    return EffectResult::kOk;
}

// ── 1788 —— {0}%令对手{1}，未触发则附加自身最大体力{2}%的百分比伤害，
//             若对手免疫百分比伤害则转变为真实伤害 ──
EffectResult effect_1788_chance_or_selfhp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1788, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1788, args,
                     "1788 布局=[owner,target,概率,异常,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1788, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1788, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (pct <= 0 || ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;
    }
    const int amount = max_hp_of(ctx, owner) * pct / 100;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_immune(target, ImmunityType::PINK_DAMAGE, ctx->currentState)) {
        if (g_core->deal_true_damage) {
            (void)g_core->deal_true_damage(ctx, target, amount, owner);   // 转真实伤害
        }
        return EffectResult::kOk;
    }
    if (g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1819 —— {0}%令对手{1}，未触发则自身免疫下{2}次受到的异常状态 ──
// 2059 同款 counts 票（ON_STAGE、不可传承口径，见 2059 原注释）。
EffectResult effect_1819_chance_or_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1819, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1819, args,
                     "1819 布局=[owner,target,概率,异常,次数]");
    const int owner = args.int_args[0];
    const int counts = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1819, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1819, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (counts > 0) {
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/0, /*source_id=*/1819, /*soul_immunity=*/false,
                            EffectScope::ON_STAGE, /*counts=*/counts,
                            /*source_effect_id=*/1819);
    }
    return EffectResult::kOk;
}

// ── 1823 —— {0}%令对手{1}，未触发则{2}%令对手{3}，均未触发则令{4}的六槽变化 ──
// args[7..12] = 语义序六槽（PLD 后备核心铀 4399 原文逐位互证，见文件头）。
EffectResult effect_1823_chance_cascade(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1823, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 13, 1823, args,
                     "1823 布局=[owner,target,概率1,异常1,概率2,异常2,对象旗,六槽×6]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1823, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3])
                         && is_valid_abnormal_status_id(args.int_args[5]),
                     1823, args, "{1}/{3} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (j3_try_main(ctx, owner, target, args.int_args[4], args.int_args[5])) {
        return EffectResult::kOk;
    }
    // 第三段：{4}=对象旗（全库实载 1=对手；其余值按自身处理，记档待官方实测）
    const int dst = (args.int_args[6] == 1) ? target : owner;
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[7 + i];
        if (delta == 0) {
            continue;
        }
        const int slot = kStatMap[i];
        if (delta < 0) {
            if (g_core->stat_drop) {
                if (dst == target) {
                    g_core->stat_drop(ctx, dst, slot, -delta, owner);   // 对手下降吃免弱
                } else {
                    g_core->stat_change(ctx, dst, slot, delta);         // 自降不吃免弱
                }
            }
        } else if (g_core->stat_change) {
            g_core->stat_change(ctx, dst, slot, delta);
        }
    }
    return EffectResult::kOk;
}

// ── 1824 —— {0}%令对手{1}，未触发则对手每存在1层处决印记自身免疫{2}次受到的异常状态 ──
// ⚠️ 记档：处决印记（链前 1010/1124 所在效果群）引擎尚无表示 → 层数恒 0、
// 兜底恒空转（免疫 0 次 = 无操作）。主干照常；印记原语落地后在此接层数查询。
EffectResult effect_1824_chance_or_mark_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1824, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1824, args,
                     "1824 布局=[owner,target,概率,异常,每层次数]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1824, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1824, args,
                     "{1} 须为合法异常码");
    (void)j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3]);
    return EffectResult::kOk;   // 兜底空转（见上记档）
}

// ── 1837 —— {0}%令对手{1}，未触发则令自身全属性-{2} ──
EffectResult effect_1837_chance_or_self_all_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1837, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1837, args,
                     "1837 布局=[owner,target,概率,异常,级数]");
    const int owner = args.int_args[0];
    const int levels = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1837, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1837, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (levels > 0) {
        all_stats_up(ctx, owner, -levels);   // 六槽含命中；自降走 stat_change
    }
    return EffectResult::kOk;
}

// ── 1862 —— {0}%令对手{1}，未触发则降低自身所有技能{2}点PP值 ──
EffectResult effect_1862_chance_or_self_pp_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1862, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1862, args,
                     "1862 布局=[owner,target,概率,异常,点数]");
    const int owner = args.int_args[0];
    const int points = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1862, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1862, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (points > 0 && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, owner, points);
    }
    return EffectResult::kOk;
}

// ── 1929 —— {0}%令对手{1}，未触发则随机附加{2}种异常状态 ──
EffectResult effect_1929_chance_or_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1929, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1929, args,
                     "1929 布局=[owner,target,概率,异常,种数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1929, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1929, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (count > 0 && g_core->attach_random_anomalies) {
        (void)g_core->attach_random_anomalies(ctx, target, anomaly_pool_all(), count,
                                              /*probability_pct=*/100, owner,
                                              ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 2005 —— {0}%令对手{1}，未触发则为对手附加{2}点护盾 ──
// 4399 大雪纷飞原文"未触发则为对手附加600点护盾"——盾上**对手**不是笔误。
EffectResult effect_2005_chance_or_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2005, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2005, args,
                     "2005 布局=[owner,target,概率,异常,点数]");
    const int target = args.int_args[1];
    const int amount = args.int_args[4];
    EFFECT_ARG_GUARD(args.int_args[0] >= 0 && args.int_args[0] <= 1 && target >= 0
                         && target <= 1,
                     2005, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 2005, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, args.int_args[0], target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (amount > 0) {
        ctx->getPet(target).shield_bank_.add(/*priority=*/0, amount, /*source_id=*/2005,
                                             /*duration=*/0);
    }
    return EffectResult::kOk;
}

// ── 2068 —— {0}%令对手{1}，未触发则回合结束后消除对手回合类效果 ──
EffectResult effect_2068_chance_or_round_end_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2068, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2068, args,
                     "2068 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 2068, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 2068, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    // 一次性 ROUND_END tick：rounds=1 InRounds 窗口 [当回合, 当回合+1) → 本回合末恰跑一次
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 2068, /*rounds=*/1,
                          {owner, target}, &j3_2068_break_tick);
    return EffectResult::kOk;
}

// ── 2096 —— {0}%令对手{1}，未触发或对手处于{2}状态则{3}回合内免疫对手的攻击 ──
// 570 同款 DAMAGE 票的窗口型（counts=0 + duration_rounds={3}，4399 蔓古/孤烛灯使/
// 薇丝珀三载体逐字："未触发或对手处于寄生/烧伤状态则N回合内免疫对手的攻击"）。
EffectResult effect_2096_chance_or_attack_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2096, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2096, args,
                     "2096 布局=[owner,target,概率,异常,状态,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int state_id = args.int_args[4];
    const int rounds = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 2096, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 2096, args,
                     "{1} 须为合法异常码");
    const bool landed = j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3]);
    const bool foe_in_state = g_core && g_core->has_active_anomaly
        && is_valid_abnormal_status_id(state_id)
        && g_core->has_active_anomaly(ctx, target, state_id);
    if (!landed || foe_in_state) {
        ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/rounds, /*source_id=*/2096,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE, /*counts=*/0,
                            /*source_effect_id=*/2096);
    }
    return EffectResult::kOk;
}

// ── 2176 —— {0}%令对手{1}，未触发则令对手威力最高的攻击技能PP值归0 ──
EffectResult effect_2176_chance_or_best_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2176, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2176, args,
                     "2176 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 2176, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 2176, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (!g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const auto& skills = ctx->getPet(target).skills;
    int best = -1;
    int best_power = -1;
    for (int i = 0; i < static_cast<int>(skills.size()); ++i) {
        const Skills& s = skills[static_cast<std::size_t>(i)];
        if (s.type == SkillType::Attribute) {
            continue;   // "攻击技能" = 物理/特殊
        }
        if (s.pp <= 0) {
            continue;   // 已空的槽无可归零
        }
        if (s.power > best_power) {
            best_power = s.power;
            best = i;
        }
    }
    if (best >= 0) {
        (void)g_core->pp_zero_slot(ctx, target, best, owner);
    }
    return EffectResult::kOk;
}

// ── 2530 —— {0}%令对手{1}，未触发则解除自身所处的异常状态 ──
EffectResult effect_2530_chance_or_self_dispel(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2530, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2530, args,
                     "2530 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0
                         && args.int_args[1] <= 1,
                     2530, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 2530, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (g_core->dispel_active_anomalies) {
        (void)g_core->dispel_active_anomalies(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── 2533 —— {0}%令对手{1}，未触发则恢复自身所有技能{2}点PP值 ──
EffectResult effect_2533_chance_or_pp_restore(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2533, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2533, args,
                     "2533 布局=[owner,target,概率,异常,点数]");
    const int owner = args.int_args[0];
    const int points = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 2533, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 2533, args,
                     "{1} 须为合法异常码");
    if (j3_try_main(ctx, owner, args.int_args[1], args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    if (points > 0 && g_core->pp_restore) {
        (void)g_core->pp_restore(ctx, owner, points, /*only_empty=*/false);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1111, effect_1111_chance_or_amp_extra);
SKILL_EFFECT(1231, effect_1231_chance_or_maxhp_pink);
SKILL_EFFECT(1278, effect_1278_chance_or_immune_reflect);
SKILL_EFFECT(1335, effect_1335_chance_or_stat_drain);
SKILL_EFFECT(1390, effect_1390_chance_or_dodge_window);
SKILL_EFFECT(1482, effect_1482_chance_or_steal_boosts);
SKILL_EFFECT(1660, effect_1660_chance_or_pp_down);
SKILL_EFFECT(1727, effect_1727_chance_or_immune_reflect_pct);
SKILL_EFFECT(1788, effect_1788_chance_or_selfhp_pink);
SKILL_EFFECT(1819, effect_1819_chance_or_anomaly_immune);
SKILL_EFFECT(1823, effect_1823_chance_cascade);
SKILL_EFFECT(1824, effect_1824_chance_or_mark_immune);
SKILL_EFFECT(1837, effect_1837_chance_or_self_all_down);
SKILL_EFFECT(1862, effect_1862_chance_or_self_pp_down);
SKILL_EFFECT(1929, effect_1929_chance_or_random_anomaly);
SKILL_EFFECT(2005, effect_2005_chance_or_shield);
SKILL_EFFECT(2068, effect_2068_chance_or_round_end_break);
SKILL_EFFECT(2096, effect_2096_chance_or_attack_immune);
SKILL_EFFECT(2176, effect_2176_chance_or_best_pp_zero);
SKILL_EFFECT(2530, effect_2530_chance_or_self_dispel);
SKILL_EFFECT(2533, effect_2533_chance_or_pp_restore);
