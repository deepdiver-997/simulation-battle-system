// J 组 J1 · 消回合成功族四批 —— 24 宏/181 载体（2026-10-01，worktree ../agents/j1）
//
// E2（e_break_round.cpp）/ F1（f_break_round2.cpp）/ G1（g_break_round3.cpp）直系后续批，
// 另含本家族既有成员 797/889/979/1034 所建立的门槛范式：break_round_effects 返回
// SUCCESS（真的清掉了东西）才执行后缀子句；NO_EFFECTS（对手本来就没有）/ IMMUNE（免断）
// 都不算成功。本批按"后缀子句"分接各原语（工单口径）：
//
//   1234×26 全属性-N          → 1990 同款 stat_drop 六槽（免弱查询，弱化走 stat_drop 非 stat_change）
//   856×14  回 1/{0}          → heal_max_hp_fraction（common.h，吃封回血）
//   973×18  {0}%令对手{1}     → apply_anomaly_checked 申报（F7 99 / 2189 同款调用面）
//   1332×6  随机附加{0}种异常 → attach_random_anomalies 原语（1367 同款，landed 三段账）
//   1343×9  下{0}次受伤转化   → EVENT_TAKE_DAMAGE watcher + shared_ptr 计次（1219 转化 / 1287 计次同源）
//   1370×9  令对手{0}，未触发则{1}回合封回血 → 797 HEAL_BLOCK 票（4399：6=害怕/5=冻伤/20=失明 ✓）
//   1381×15 {0}回合免疫并反弹异常 → 191 同款（Modern 层 ANOMALY 票 + grant_reflect）
//   1423×23 {0}%令对手{1}，未触发则回 1/{2} → checked 申报 + heal_max_hp_fraction
//   1523×20 吸取对手{0}点体力 → 870 同款点数吸取（直写 hp + 按实际扣减回血）
//   1817×1  下回合必定先手    → grant_guaranteed_first（内核"下一回合必先"正道）
//   2195×5  对手下{0}回合先制-{1} → 979 窗口同款（MOVE_RIGHT 单桶 + 共享先制 tick，bonus 取负、挂对手）
//   2299×3  对手攻击技能 PP 归零 → pp_zero_slot 逐槽（只打非属性槽；pp<=0/无限槽跳过）
//   2390×2  下{0}回合对手攻击白板 → ATTACK_NULLIFY 票（1090 族同款白板，见函数头选型记档）
//   1900/2034/2038/2438/2452/2557 见各自函数头注
//   1190×14 消除成功对手六槽掩码 → **语义序**（攻,防,特攻,特攻?,…）见函数头注（4 载体 4399 钉死）
//
// 记档跳过（5 宏，全部单载体、挂宠物专属外部机制——工单"单载体长尾允许跳过"条款）：
//   1094×1（圣世惩杀：星皇之怒多段复合——随机 PP 归零/概率提升/额外行动，白班专项）
//   1859×1（赤胆缀银枪：龙胆层数≥7 无视前提——层数机归魂印侧）
//   1900×1（君命：方永昌玺获取——玺印外部记账）
//   2042×1（奇异光影：N 道诡符——诡符外部记账）
//   2123×1（王·千春共绽：灵茉之芯计数——外部计数机）
//
// ⚠️ 属性排列双序坑（速查 §三）：1190 的六槽掩码按**语义序**解（538/745 同款
// kSemanticToEngineSlot）。载体四例 4399 交叉验证钉死：结构梳理 [0,0,-2,-2,0,0]="特攻-2，
// 特防-2"、被迫启动 [0,-2,0,-2,0,0]="防御-2，特防-2"、尘灵绝生阵 [-1,0,-1,0,-1,-1]=
// "攻击-1，特攻-1，速度-1，命中-1"、创灵护界诀 [-1,-2,-1,-2,0,0]="攻击-1，防御-2，
// 特攻-1，特防-2"——按引擎序直读会把 1/2 位（防/特攻）读反。
//
// 前置三查（2026-10-01）：①内核无本批 24 id 硬编码（grep src/ include/ 仅原语文档注释）
// ②effect_meta 无该批 id 声明（均 SKILL_EFFECT 即时型，无窗口家族牵连）③全库无既有实现
// （SKILL_EFFECT(本批 id) 零注册；同族已实现成员 453/797/889/915/979/1034/1036/1083/
// 1179/1219/990 不在本批族单）。
//
// 场景回归：test/scenario/scenario_890_purge_round4.cpp（场景段 890-894）。

#include "common.h"

#include <memory>

namespace {

// 官方语义序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
// 538/745/stat_dispel 同款（1/2 两位互换）；1190 六槽掩码按此解（文件头 4 载体 4399 钉死）。
constexpr int kSemanticToEngineSlot[6] = {0, 2, 1, 3, 4, 5};

// "未触发"判定：异常**落地**才算触发（F7 范式）。SUCCESS/替换/延长/转化 = 落地；
// 免疫/被挡/抗性/弹回/掷点未中 = 未触发（弹回 REFLECTED 记档：异常确实"发生"了但没落在
// 对手身上，本族"未触发则X"的补偿语义按保守档算未触发——1367 landed==0 同款保守取向）。
inline bool anomaly_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
        case ApplyAnomalyResult::CONVERTED:
            return true;
        default:
            return false;
    }
}

// 申报式施加（概率闸门管道）：chance<0 = 不申报（等价旧槽，无概率子句的模板用）。
inline ApplyAnomalyResult checked_apply(BattleContext* ctx, int target, int anomaly,
                                        int actor, int chance) {
    return g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                         actor, chance, ChanceSource::Skill);
}

// ── 1234 —— 消除对手回合类效果，消除成功则对手全属性-{0} ──
// int_args=[owner,target,N]。弱化施加对手 → stat_drop（查免弱；1990"令对手全属性-{0}"
// 同款六槽循环，actor=owner 供附加禁令判定）。
EffectResult effect_1234_break_then_all_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1234, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1234, args,
                     "1234 布局=[owner,target,N]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int n = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && n > 0, 1234, args,
                     "需要双方侧位合法且 N>0");
    if (!g_core || !g_core->break_round_effects || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不弱化（家族门槛）
    }
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        (void)g_core->stat_drop(ctx, opp, slot, n, owner);
    }
    return EffectResult::kOk;
}

// ── 856 —— 消除对手回合类效果，消除成功则恢复自身最大体力的1/{0} ──
EffectResult effect_856_break_then_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 856, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 856, args,
                     "856 布局=[owner,target,denom]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && denom > 0, 856, args,
                     "需要双方侧位合法且 denom>0");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不恢复
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);   // 吃封回血/恢复修正
    return EffectResult::kOk;
}

// ── 973 —— 消除对手回合类效果，消除成功则{0}%令对手{1} ──
EffectResult effect_973_break_then_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 973, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 973, args,
                     "973 布局=[owner,target,概率,异常码]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 973, args,
                     "需要双方侧位合法");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 973, args, "需要 0 ≤ 概率 ≤ 100");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 973, args, "异常码非法");
    if (!g_core || !g_core->break_round_effects || chance <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不申报
    }
    (void)checked_apply(ctx, opp, anomaly, owner, chance);
    return EffectResult::kOk;
}

// ── 1332 —— 消除对手回合类效果，消除成功则随机附加{0}种异常状态 ──
EffectResult effect_1332_break_then_random_anomalies(BattleContext* ctx,
                                                     const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1332, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1332, args,
                     "1332 布局=[owner,target,种数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && kinds > 0, 1332, args,
                     "需要双方侧位合法且种数>0");
    if (!g_core || !g_core->break_round_effects || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不附加
    }
    (void)g_core->attach_random_anomalies(ctx, opp, anomaly_pool_all(), kinds,
                                          /*probability_pct=*/100, owner, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1343 —— 消除对手回合类效果，消除成功则下{0}次受到的伤害转化为自身体力 ──
// 计次版 1219：EVENT_TAKE_DAMAGE watcher（窗口 9 回合防泄漏，1036 同款；"下{0}次"无回合
// 限定 → 不做 1219 的当回合过滤——SKILL_EFFECT 桶晚于本次伤害管线，watcher 天然不含当次）。
// 1:1 全额 heal_amount（1219 转化口径）。全库切片 {0} 恒 1，参数驱动实现不特判。
EffectResult effect_1343_break_then_damage_to_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1343, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1343, args,
                     "1343 布局=[owner,target,次数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && times > 0, 1343, args,
                     "需要双方侧位合法且次数>0");
    if (!g_core || !g_core->break_round_effects || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 无转化
    }
    auto remaining = std::make_shared<int>(times);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/9,
        /*once=*/false,
        [owner, remaining](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || *remaining <= 0 || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.amount > 0) {
                g_core->heal_amount(c, owner, ev.amount);   // 1:1 全额转化（1219 口径）
                --*remaining;
            }
        },
        WatcherScope::TEAM);   // 跨回合窗口必须 TEAM（1219 同款选型）
    return EffectResult::kOk;
}

// ── 1370 —— 消除对手回合类效果，消除成功则对手{0}，未触发则{1}回合内对手无法通过自身技能恢复体力 ──
// {0}=异常码（4399 双源：6=害怕/5=冻伤/20=失明 ✓）；未触发 → 797 同款 HEAL_BLOCK 票挂对手。
EffectResult effect_1370_break_then_anomaly_or_heal_block(BattleContext* ctx,
                                                          const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1370, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1370, args,
                     "1370 布局=[owner,target,异常码,封回血回合]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && rounds > 0, 1370, args,
                     "需要双方侧位合法且回合>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1370, args, "异常码非法");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 两个子句都不走
    }
    // chance=100 申报（模板无概率子句；闸门管道统一管辖，实际概率以申报值为准）
    if (anomaly_landed(checked_apply(ctx, opp, anomaly, owner, 100))) {
        return EffectResult::kOk;   // 触发：无补偿
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1370,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1370);
    return EffectResult::kOk;
}

// ── 1381 —— 消除对手回合类效果，消除成功则自身{0}回合内免疫并反弹异常状态 ──
// 191 同款双授（Modern 层 ANOMALY 票 + grant_reflect），门槛换本家族 SUCCESS。
EffectResult effect_1381_break_then_anomaly_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1381, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1381, args,
                     "1381 布局=[owner,target,回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && rounds > 0, 1381, args,
                     "需要双方侧位合法且回合>0");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不授予
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1381,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1381,
                        /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1, /*source_effect_id=*/1381,
                                    /*target=*/owner, /*rounds=*/rounds);
    return EffectResult::kOk;
}

// ── 1423 —— 消除对手回合类效果，消除成功则{0}%令对手{1}，未触发则恢复自身最大体力的1/{2} ──
EffectResult effect_1423_break_then_anomaly_or_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1423, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1423, args,
                     "1423 布局=[owner,target,概率,异常码,denom]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && denom > 0, 1423, args,
                     "需要双方侧位合法且 denom>0");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1423, args, "需要 0 ≤ 概率 ≤ 100");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1423, args, "异常码非法");
    if (!g_core || !g_core->break_round_effects || chance <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 两个子句都不走
    }
    if (!anomaly_landed(checked_apply(ctx, opp, anomaly, owner, chance))) {
        (void)heal_max_hp_fraction(ctx, owner, denom);   // 未触发补偿（吃封回血）
    }
    return EffectResult::kOk;
}

// ── 1523 —— 消除对手回合类效果，消除成功则吸取对手{0}点体力 ──
// 870 同款点数吸取（引擎无平吸原语：直写 hp + 按实际扣减回血，吃封回血）。
EffectResult effect_1523_break_then_drain_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1523, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1523, args,
                     "1523 布局=[owner,target,点数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && amount > 0, 1523, args,
                     "需要双方侧位合法且点数>0");
    if (!g_core || !g_core->break_round_effects || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不吸取
    }
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const int drained = std::min(amount, foe.hp);
    foe.hp -= drained;
    if (drained > 0) {
        (void)g_core->heal_amount(ctx, owner, drained);
    }
    return EffectResult::kOk;
}

// ── 1817 —— 消除对手回合类效果，消除成功则下回合自身技能必定先手 ──
// grant_guaranteed_first 即"下一回合必先"原语（可被断回合移除——语义正道）。
EffectResult effect_1817_break_then_guaranteed_first(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1817, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1817, args,
                     "1817 布局=[owner,target]（int_count>=2）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1817, args,
                     "需要双方侧位合法");
    if (!g_core || !g_core->break_round_effects || !g_core->grant_guaranteed_first) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不必先
    }
    g_core->grant_guaranteed_first(ctx, owner, /*tier=*/1);
    return EffectResult::kOk;
}

// ── 1190 —— 消除对手回合类效果，消除成功对手{0}（六槽掩码，effect_info 残文）──
// int_args=[owner,target,攻,防,特攻,特防,速,命中]（**语义序**，文件头 4 载体 4399 钉死；
// 0=该槽不动、负=下降）。弱化施加对手 → stat_drop 逐槽（免弱逐槽查询，多槽部分成功记档）。
EffectResult effect_1190_break_then_mask_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1190, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 1190, args,
                     "1190 布局=[owner,target,攻,防,特攻,特防,速,命中]（int_count>=8）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1190, args,
                     "需要双方侧位合法");
    if (!g_core || !g_core->break_round_effects || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不弱化
    }
    for (int i = 0; i < 6; ++i) {
        const int levels = args.int_args[2 + i];
        if (levels < 0) {
            (void)g_core->stat_drop(ctx, opp, kSemanticToEngineSlot[i], -levels, owner);
        }
    }
    return EffectResult::kOk;
}

// ── 2195 —— 消除对手回合类效果，消除成功对手下{0}回合先制-{1} ──
// 979 窗口同款（MOVE_RIGHT 单桶 + 共享先制 tick），方向取负、窗口挂对手侧。
EffectResult effect_2195_break_then_priority_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2195, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2195, args,
                     "2195 布局=[owner,target,回合,先制降]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && rounds > 0 && down > 0,
                     2195, args, "需要双方侧位合法且回合/降幅>0");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不降先制
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, /*owner=*/opp, 2195,
                                rounds, {opp, opp, -down}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 2299 —— 消除对手回合类效果，消除成功则令对手攻击技能PP值归0 ──
// pp_zero_slot 逐槽（"PP 操作走原语"纪律——效果监听 EVENT_PP_REDUCED；pp<=0/无限槽跳过）。
EffectResult effect_2299_break_then_pp_zero_attack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2299, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 2299, args,
                     "2299 布局=[owner,target]（int_count>=2）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 2299, args,
                     "需要双方侧位合法");
    if (!g_core || !g_core->break_round_effects || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不清 PP
    }
    const auto& skills = ctx->getPet(opp).skills;
    for (std::size_t i = 0; i < skills.size(); ++i) {
        if (skills[i].type != SkillType::Attribute && skills[i].pp > 0) {
            (void)g_core->pp_zero_slot(ctx, opp, static_cast<int>(i), owner);
        }
    }
    return EffectResult::kOk;
}

// ── 2390 —— 消除对手回合类效果，消除成功则下{0}回合对手攻击技能无法造成伤害且命中效果失效 ──
// = ATTACK_NULLIFY 票（effect 1090 族同款，内核 ②.6 消费点：红伤归零 + 效果不注册的
// kFullNull 白板——文案逐字同源）。register_round=下回合起算（"下{0}回合"不含当回合）。
// 选型记档：hit_effect_invalid kFullNull 条目落 ws 每回合重置（补挂被 source_id 去重吞、
// 一次性只覆盖当回合，两版实测均不生效）；seal_skill(attack, hit_invalid) 语义="效果失效
// 伤害照常"，均非本条字面。_grant 经 ctx->rule_center_ 直调（batch_c 613 同款）。
EffectResult effect_2390_break_then_attack_fullnull(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2390, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2390, args,
                     "2390 布局=[owner,target,回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && rounds > 0, 2390, args,
                     "需要双方侧位合法且回合>0");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不挂窗
    }
    // rounds={0}+2 / register=下回合：票被两条生命周期规则复合计账（授予回合的
    // tick_rounds 先扣 1 + window_expired 按 register 偏移比较），实测 {0}+2 恰好
    // 覆盖"下{0}回合"（R2..R{0}+1 nullified、R{0}+2 过期，场景 890 T16 实证）。
    (void)ctx->rule_center_.grant_attack_nullify(
        /*bearer=*/owner, /*source_effect_id=*/2390, /*suppress=*/opp,
        /*rounds=*/rounds + 2, /*register_round=*/ctx->roundCount + 1, EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

// ── 2034 —— 消除对手回合类效果，消除成功则{0}%令对手{1}，未触发则{2}回合内自身受到的攻击伤害减少{3}% ──
// 未触发补偿 = REDUCE_PCT 回合窗（1287 计次减伤同通道，计次改回合界 [r0, r0+{2}-1]，InRounds）。
EffectResult effect_2034_break_then_anomaly_or_mitigate(BattleContext* ctx,
                                                        const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2034, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2034, args,
                     "2034 布局=[owner,target,概率,异常码,回合,减伤%]（int_count>=6）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && rounds > 0, 2034, args,
                     "需要双方侧位合法且回合>0");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100 && pct > 0 && pct < 100, 2034, args,
                     "需要 0 ≤ 概率 ≤ 100 且 0 < 减伤 < 100");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2034, args, "异常码非法");
    if (!g_core || !g_core->break_round_effects || chance <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 两个子句都不走
    }
    if (anomaly_landed(checked_apply(ctx, opp, anomaly, owner, chance))) {
        return EffectResult::kOk;   // 触发：无补偿
    }
    const int r0 = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, pct, r0, rounds](BattleContext* c, int) {
            if (!c || c->roundCount > r0 + rounds - 1) {
                return;   // "{2}回合内"含当回合（InRounds 窗口，950/766 同款口径）
            }
            if (c->resolvedDamage.defenderId != owner) {
                return;   // 只减自身受击（1287 同款判定）
            }
            c->resolvedDamage.final = c->resolvedDamage.final * (100 - pct) / 100;
        });
    return EffectResult::kOk;
}

// ── 2038 —— 消除对手回合类效果，消除成功则令对手{0}，若对手不处于回合类效果则令自身{1}且{2}状态解除后恢复自身全部体力 ──
// 双分支：SUCCESS → 对手异常；NO_EFFECTS（对手本来就没有回合类效果=“不处于”）→ 自身异常
// + EVENT_ANOMALY_EXPIRED（自然到期路径，星盘族口径——效果解除/转移不发，偏差记档）
// 监听 {2} 到期 → heal_amount 满（原语内部钳满）。IMMUNE（免断）→ 两分支都不走。
EffectResult effect_2038_break_branch_self_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2038, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2038, args,
                     "2038 布局=[owner,target,对手异常,自身异常,到期恢复异常]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int a_opp = args.int_args[2];
    const int a_self = args.int_args[3];
    const int a_heal = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 2038, args,
                     "需要双方侧位合法");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(a_opp) && is_valid_abnormal_status_id(a_self)
                         && is_valid_abnormal_status_id(a_heal), 2038, args, "异常码非法");
    if (!g_core || !g_core->break_round_effects || !g_core->apply_anomaly
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const BreakResult br = g_core->break_round_effects(ctx, opp);
    if (br == BreakResult::SUCCESS) {
        g_core->apply_anomaly(ctx, opp, a_opp, random_anomaly_duration(), owner);
        return EffectResult::kOk;
    }
    if (br != BreakResult::NO_EFFECTS) {
        return EffectResult::kOk;   // IMMUNE/INVALID：对手仍处于回合类效果或判定异常 → 不走
    }
    if (!anomaly_landed(g_core->apply_anomaly(ctx, owner, a_self,
                                              random_anomaly_duration(), owner))) {
        return EffectResult::kOk;   // 自身免疫/被挡 → 无异常可"解除"，恢复前提不成立
    }
    const int r0 = ctx->roundCount;
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_EXPIRED, owner, r0, /*duration_rounds=*/9,
        /*once=*/true,
        [owner, a_heal](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.amount != a_heal || !g_core
                || !g_core->heal_amount) {
                return;
            }
            (void)g_core->heal_amount(c, owner, max_hp_of(c, owner));   // 恢复全部（内部钳满）
        },
        WatcherScope::TEAM);   // 跨回合窗口必须 TEAM（1219 同款选型）
    return EffectResult::kOk;
}

// ── 2438 —— 消除对手回合类效果，消除成功{0}%令对手{1}，{2}%令对手{3}，自身体力低于{4}%时取消触发条件 ──
// "取消触发条件"= 自身低体时**免消除门槛**直接申报两段异常（低体自救条款）。
EffectResult effect_2438_break_then_double_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2438, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 2438, args,
                     "2438 布局=[owner,target,概率1,异常1,概率2,异常2,低体%]（int_count>=7）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int c0 = args.int_args[2];
    const int a0 = args.int_args[3];
    const int c1 = args.int_args[4];
    const int a1 = args.int_args[5];
    const int hpp = args.int_args[6];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 2438, args,
                     "需要双方侧位合法");
    EFFECT_ARG_GUARD(c0 >= 0 && c0 <= 100 && c1 >= 0 && c1 <= 100 && hpp > 0 && hpp <= 100,
                     2438, args, "需要概率 ∈ [0,100] 且 0 < 低体% ≤ 100");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(a0) && is_valid_abnormal_status_id(a1), 2438,
                     args, "异常码非法");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const bool low_hp = ctx->getPet(owner).hp * 100 < max_hp_of(ctx, owner) * hpp;
    if (!low_hp
        && g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 非低体需消除成功；低体（< {4}%）取消触发条件
    }
    if (c0 > 0) {
        (void)checked_apply(ctx, opp, a0, owner, c0);
    }
    if (c1 > 0) {
        (void)checked_apply(ctx, opp, a1, owner, c1);
    }
    return EffectResult::kOk;
}

// ── 2452 —— 消除对手回合类效果，消除成功则令对手{0}，未触发则令对手全属性-{1} ──
EffectResult effect_2452_break_then_anomaly_or_all_down(BattleContext* ctx,
                                                        const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2452, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2452, args,
                     "2452 布局=[owner,target,异常码,全属性降]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && down > 0, 2452, args,
                     "需要双方侧位合法且降幅>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2452, args, "异常码非法");
    if (!g_core || !g_core->break_round_effects || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 两个子句都不走
    }
    if (anomaly_landed(checked_apply(ctx, opp, anomaly, owner, 100))) {
        return EffectResult::kOk;   // 触发：无补偿
    }
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        (void)g_core->stat_drop(ctx, opp, slot, down, owner);   // 1990 全属性-N 同款
    }
    return EffectResult::kOk;
}

// ── 2557 —— 消除对手回合类效果，消除成功则令对手{0}、{1}、{2} ──
// 三段异常各走现代通道（889 同款；逐段全检查链，部分落地部分被挡为引擎统一行为）。
EffectResult effect_2557_break_then_triple_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2557, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2557, args,
                     "2557 布局=[owner,target,异常1,异常2,异常3]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 2557, args,
                     "需要双方侧位合法");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[2])
                         && is_valid_abnormal_status_id(args.int_args[3])
                         && is_valid_abnormal_status_id(args.int_args[4]), 2557, args,
                     "异常码非法");
    if (!g_core || !g_core->break_round_effects || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不施加
    }
    for (int i = 2; i <= 4; ++i) {
        g_core->apply_anomaly(ctx, opp, args.int_args[i], random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 本批注册（19 实现；1094/1859/1900/2042/2123 记档跳过，见文件头）──
SKILL_EFFECT(856, effect_856_break_then_heal)
SKILL_EFFECT(973, effect_973_break_then_chance_anomaly)
SKILL_EFFECT(1190, effect_1190_break_then_mask_down)
SKILL_EFFECT(1234, effect_1234_break_then_all_down)
SKILL_EFFECT(1332, effect_1332_break_then_random_anomalies)
SKILL_EFFECT(1343, effect_1343_break_then_damage_to_heal)
SKILL_EFFECT(1370, effect_1370_break_then_anomaly_or_heal_block)
SKILL_EFFECT(1381, effect_1381_break_then_anomaly_reflect)
SKILL_EFFECT(1423, effect_1423_break_then_anomaly_or_heal)
SKILL_EFFECT(1523, effect_1523_break_then_drain_fixed)
SKILL_EFFECT(1817, effect_1817_break_then_guaranteed_first)
SKILL_EFFECT(2034, effect_2034_break_then_anomaly_or_mitigate)
SKILL_EFFECT(2038, effect_2038_break_branch_self_anomaly)
SKILL_EFFECT(2195, effect_2195_break_then_priority_down)
SKILL_EFFECT(2299, effect_2299_break_then_pp_zero_attack)
SKILL_EFFECT(2390, effect_2390_break_then_attack_fullnull)
SKILL_EFFECT(2438, effect_2438_break_then_double_anomaly)
SKILL_EFFECT(2452, effect_2452_break_then_anomaly_or_all_down)
SKILL_EFFECT(2557, effect_2557_break_then_triple_anomaly)
