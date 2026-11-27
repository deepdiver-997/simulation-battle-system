#include <utils/sbs_rand.h>
// 属性升降 / 消除与反转 / 全属性族。585 的参数序是第三套官方码序（见其注释）。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// Effect 143 - 使对手的能力提升效果反转成能力下降效果（雨燕之影 17587 / 黑水啸天破 16041 等）。
// Args: [owner(0), target(1), ...]（其余参数本效果不用）。
// 走 stat_boost_reversal 真原语：**弱化类**动作 → 先查免弱(STAT_DROP)，免疫即失败；
// 可穿强化保护(STAT_CLEAR)；本体/视图一并同步。
EffectResult effect_143_reverse_opp_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->stat_boost_reversal) {
        g_core->stat_boost_reversal(ctx, target);
    }
    return EffectResult::kOk;
}

// Effect 1960 - 击败对手则令自身{0}回合内的能力提升状态无法被消除或吸取（希拓·神煌炎舞斩 37222）。
// Args: [owner(0), target(1), rounds(2)]。注册时点 = BATTLE_AFTER_DEFEATING_OPPONENT（见 skills.cpp）。
// 授予的是**窗口型免疫**（RuleCenter IMMUNE/STAT_CLEAR，coverage_all 全时点）：
//   - 三个查询点（消除 clear_stat_boosts / 转化 transfer_stat_boosts / 吸取 1287）都会命中它；
//   - 免疫条目**天然不可被断回合**（只随上下场清）——与官方"免疫类"一致；
//   - 窗口按"注册回合 + N 回合"过期（is_immune/cleanup），不 tick。
// ⚠️ 近似：注册在"击败对手后"时点而非"本技能击败对手"，若对手被本回合其它来源击杀也会触发。
EffectResult effect_1960_protect_stat_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];  // 调用方已保证 int_count>=3 时才有意义
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_CLEAR, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1960,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1960);
    return EffectResult::kOk;
}

// Effect 85 - 使对手的能力提升效果转化到自己身上（落芳天华 17912）。
// Args: [owner(0), target(1), ...]（其余参数本效果不用）。走 transfer_stat_boosts 真原语。
// 与 1287"吸取"是同一动作（差别只在吸取成功后的额外收益），故共用原语；同样受
// 免消除强化(STAT_CLEAR) 保护——对手有该保护时转化失败。
EffectResult effect_85_transfer_stat_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->transfer_stat_boosts) {
        g_core->transfer_stat_boosts(ctx, /*from=*/target, /*to=*/owner);
    }
    return EffectResult::kOk;
}

// effect 1221 - 反转自身能力下降，反转成功则自身免疫下 {n} 次受到的异常状态（王·酷烈风息）。
// 走 stat_reversal 真原语（**区别于** clear_stat_boosts：反转把下降翻成提升，不是消除提升）。
// Args=[owner,target,次数]。次数型异常免疫走 RuleCenter counts（apply_anomaly 挡下即扣一次）。
EffectResult effect_1221_reverse_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->stat_reversal
        && g_core->stat_reversal(ctx, owner) == StatReversalResult::REVERSED) {
        // 反转成功 → 免疫下{次数}次异常。真次数免疫：counts>0，命中一次扣一次，扣到 0 注销。
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), 0,
                            /*duration_rounds=*/0, /*source_id=*/1221,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/counts);
    }
    return EffectResult::kOk;
}

// effect 5 - 技能使用成功时 {chance}% 改变对手 stat 等级 delta（通用：奉献之火/火势蔓延/恐惧之气等）。
// Args(bind_participants 前缀): [0]=owner, [1]=target(对手), [2]=stat, [3]=chance, [4]=delta。
EffectResult effect_5_change_opp_stat(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int stat = args.int_args[2];
    const int chance = args.int_args[3];
    const int delta = args.int_args[4];
    if (stat < 0 || stat >= 6) {
        return EffectResult::kOk;
    }
    // 仅 >=100 短路；<=0 的必失败档仍消耗一次 RNG。
    if (chance < 100 && !roll_skill_gate(*ctx, 5, 0, owner, opp, chance, 100,
                                        ChanceSingleton::ConsumeDraw)) {
        return EffectResult::kOk;  // 概率未触发
    }
    // 负 delta = 弱化 → 走 stat_drop（查免弱 STAT_DROP，可穿强化保护，钳 -6）；
    // 正 delta = 给对手加提升（罕见）→ stat_change。
    if (delta < 0) {
        if (g_core && g_core->stat_drop) {
            g_core->stat_drop(ctx, opp, stat, -delta, owner);
        }
    } else if (g_core && g_core->stat_change) {
        g_core->stat_change(ctx, opp, stat, delta);
    }
    return EffectResult::kOk;
}

// ── Effect 1238 - 全属性+{0}，对手不为{1}系时强化效果翻倍（光荣之梦）──
// Args: [self, target, delta, element]
// "翻倍" = **提升的等级数**翻倍（+1 → +2），不是把已有等级翻倍（用户 2026-09-13 口径）。
// "对手不为{1}系" 看对手**固有属性**（pet.elementalAttributes，双属性任一命中即算）。
EffectResult effect_1238_all_stats_doubled(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int delta = args.int_args[2];
    const int element = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    const ElfPet& opp_pet = ctx->getPet(opp);
    const bool opp_has_element = opp_pet.elementalAttributes[0] == element
                              || opp_pet.elementalAttributes[1] == element;
    all_stats_up(ctx, self, opp_has_element ? delta : delta * 2);
    return EffectResult::kOk;
}

// ══════════════════════════════════════════════════════════════════════════
// 批 4 —— 圣灵谱尼 5000 的经典基础技能（官方 effect_info 分析列逐条核对）
//
// 数据（moves.side_effect / side_effect_arg）：
//   极光10993[]  断空破10994[29:30]  旋灭裂空阵10995[60:5,30]  神圣之光10996[]
//   灵光之怒10997[35]  圣堂之门10998[33]  千烈虚光闪10999[3]  圣灵魔闪光11000[28:8]
//   元素11261[29:200]  能量11274[34:2]  虚无20498[52:2]  灵魂干涉20499[22:100,2]
//   圣光气20500[58:2]  圣洁20605[86:5]  生命20606[77:5,100]  轮回20607[43:1]  永恒20608[87]
// ══════════════════════════════════════════════════════════════════════════

// ── effect 3 「解除自身的能力下降状态」—— 千烈虚光闪 10999 ──
// Args = [owner, target]。⚠️ 清**下降**走 `clear_stat_drops`，它与 `clear_stat_boosts`
// 故意不对称：清弱化对目标有利，**不查任何免疫**（用户 2026-09-13 口径）。
EffectResult effect_3_clear_own_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->clear_stat_drops) {
        g_core->clear_stat_drops(ctx, owner);
    }
    return EffectResult::kOk;
}

// ── effect 33 「消除对手能力提升状态」—— 圣堂之门 10998 ──
// Args = [owner, target]。走 `clear_stat_boosts`（查免消除强化 STAT_CLEAR，返回清掉个数）。
EffectResult effect_33_clear_opp_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, target);
    }
    return EffectResult::kOk;
}

// effect 47 —— {0}回合内免疫能力下降状态（671 个技能在用，头部家族最大）。
// Args=[owner,target,rounds]。窗口型免弱票（STAT_DROP）：stat_drop / stat_boost_reversal
// 的入口查询会命中。覆盖键 source_effect_id=47，与其它免弱来源各占一条。
EffectResult effect_47_stat_drop_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/47, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/47);
    return EffectResult::kOk;
}

// effect 180 —— 消除对手回合类效果（288 个技能在用；args_num=0）。
// break_round_effects 直通：吃目标免断（IMMUNE）+ EVENT_BREAK；
// 本族模板没有"按结果分支"的后续子句，返回值忽略。
EffectResult effect_180_break_round(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->break_round_effects(ctx, target);
    return EffectResult::kOk;
}

// effect 521 —— 反转自身能力下降状态（290 个技能在用；args_num=0）。stat_reversal 直通。
// 挂 AFTER_ACTION（同 1221 口径）：攻击技能**先结算伤害再反转**——反转不参与本次伤害
//（本次用反转前等级，提升留给下次）。
EffectResult effect_521_reverse_own_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_reversal(ctx, owner);
    return EffectResult::kOk;
}

// ── 全属性族（885/946/962/1001/1014）：全属性+{0}，条件成立则强化效果翻倍 ──
// 一次性效果（用技能即触发），条件在**生效时刻**求值。"翻倍" = delta×2（到 +6 照常钳）。

// effect 885 —— 自身当前体力**高于**对手时翻倍（先驱 29066 等，105 技）
EffectResult effect_885_all_stats_hp_higher(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    const bool doubled = ctx->getPet(self).hp > ctx->getPet(opp).hp;
    all_stats_up(ctx, self, doubled ? delta * 2 : delta);
    return EffectResult::kOk;
}

// effect 946 —— 自身当前体力**低于**对手时翻倍（紧那罗歌 24911 等，119 技）
EffectResult effect_946_all_stats_hp_lower(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    const bool doubled = ctx->getPet(self).hp < ctx->getPet(opp).hp;
    all_stats_up(ctx, self, doubled ? delta * 2 : delta);
    return EffectResult::kOk;
}

// effect 962 —— 自身**不处于能力提升状态**时翻倍（剑指风云 24950 等，103 技）。
// "能力提升状态"查本体正等级（5 项战斗属性，同 all_stats_up 的范围）。
EffectResult effect_962_all_stats_no_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    bool has_boost = false;
    // "自身是否已有任何能力提升"要含**命中**（用户 2026-09-18：提到等级提升下降一律指含命中的 6 种）
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[self][i] > 0) {
            has_boost = true;
            break;
        }
    }
    all_stats_up(ctx, self, has_boost ? delta : delta * 2);
    return EffectResult::kOk;
}

// effect 1001 —— 对手**处于异常状态**时翻倍（魂断天涯 25005 等，176 技）。
// "处于异常状态" = 任一异常槽的结束回合非 0（同 EffectUnit TargetHasAnomaly 判据）。
EffectResult effect_1001_all_stats_opp_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    bool opp_has_anomaly = false;
    for (int end : ctx->abnormal_status_end_round[opp]) {
        if (end != 0) {
            opp_has_anomaly = true;
            break;
        }
    }
    all_stats_up(ctx, self, opp_has_anomaly ? delta * 2 : delta);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 批 6：全属性条件翻倍族**余量**（2026-09-17，277 技）——同 885/946/962/1001/1014 套路：
// 一次性 SKILL_EFFECT，条件在生效时刻求值，成立则 delta×2。
//   893(96技) 自身**满体力**时翻倍          （渡世渡劫 24736）
//   911(92技) 自身**处于能力提升状态**时翻倍（气压万军 24832）
//   1283(89技) **先出手**时翻倍             （余音袅袅 25896）
// ════════════════════════════════════════════════════════════════════

// effect 893 —— 全属性+{0}，自身**满体力**时强化效果翻倍
EffectResult effect_893_all_stats_full_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    const bool doubled = ctx->getPet(self).hp >= max_hp_of(ctx, self);
    all_stats_up(ctx, self, doubled ? delta * 2 : delta);
    return EffectResult::kOk;
}

// effect 911 —— 全属性+{0}，自身**处于能力提升状态**时强化效果翻倍
// （判据同 962：5 项战斗属性任一正等级；差别只是 962 是"不处于"、911 是"处于"）
EffectResult effect_911_all_stats_has_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    bool has_boost = false;
    // 同上：含命中
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[self][i] > 0) {
            has_boost = true;
            break;
        }
    }
    all_stats_up(ctx, self, has_boost ? delta * 2 : delta);
    return EffectResult::kOk;
}

// effect 1283 —— 全属性+{0}，**先出手**时强化效果翻倍
// 判据用 is_second_mover（先手权在 MOVE_RIGHT 已结算）——本效果在 SKILL_EFFECT 跑，晚于它。
EffectResult effect_1283_all_stats_first_move(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    const bool first = !ctx->is_second_mover(self);
    all_stats_up(ctx, self, first ? delta * 2 : delta);
    return EffectResult::kOk;
}

// effect 1014 —— 自身当前体力**高于**最大体力的1/{1}时翻倍（破阵霸力 25034 等，101 技）
// Args=[owner,target,N,denom]
EffectResult effect_1014_all_stats_hp_above_ratio(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int delta = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || delta == 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const bool doubled = ctx->getPet(self).hp > max_hp / denom;
    all_stats_up(ctx, self, doubled ? delta * 2 : delta);
    return EffectResult::kOk;
}

// ── effect 585 —— 技能使用成功时，{0}（六项属性提升模板；雷皇现世 23360 等）──
// Args=[owner,target,s0..s5]：s_i = 第 i 项属性的等级增量，0 = 跳过。
// ⚠️ **官方参数序是第三套码序**：[攻, 防, 特攻, 特防, 速, 命中]（= new_se Eid 34 的
// 0攻1防2特攻3特防4速 + 命中）。证据：雷皇现世 args [1,1,0,1,1,1] ↔ 图鉴原文
// 「攻击防御特防速度命中+1」——特攻跳过、0 恰好落在第 3 位（若按引擎序 0 会落在防御位）。
// 引擎序转换表 kStatMap[i] = 官方第 i 项对应的引擎 stat 下标。
EffectResult effect_585_multi_stat_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8) {
        return EffectResult::kOk;
    }
    // 官方序 → 引擎序：0攻→0、1防→2、2特攻→1、3特防→3、4速→4、5命中→5
    static constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        const int stat = kStatMap[i];
        if (delta > 0 && g_core && g_core->stat_change) {
            g_core->stat_change(ctx, self, stat, delta);
        } else if (delta < 0 && g_core && g_core->stat_drop) {
            g_core->stat_drop(ctx, self, stat, -delta, args.int_args[0]);
        }
    }
    return EffectResult::kOk;
}

// ── effect 749 —— {0}%令对手全属性-{1}（狂化电流 24009 等）──
// Args=[owner,target,chance,n]。概率门掷**一次**（整段效果要么发生要么不发生）；
// 命中后 5 项战斗属性逐项 stat_drop（各自查免弱、各自钳 -6）。
EffectResult effect_749_chance_all_stats_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int n = args.int_args[3];
    if (target < 0 || target > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    if (!roll_skill_percent(*ctx, 749, 0, args.int_args[0], target, chance)) {
        return EffectResult::kOk;
    }
    // "全属性-n"含**命中**（同一口径）；走 stat_drop → 各查免弱、各钳 -6
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        g_core->stat_drop(ctx, target, i, n, args.int_args[0]);
    }
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    SKILL_EFFECT(85, effect_85_transfer_stat_boosts);          // 对手能力提升转化到自己(落芳天华)
    SKILL_EFFECT(1960, effect_1960_protect_stat_boosts);      // 击败对手→N回合强化无法被消除/吸取(希拓)
    SKILL_EFFECT(143, effect_143_reverse_opp_boosts);         // 对手能力提升反转成下降(雨燕之影)
    SKILL_EFFECT(1221, effect_1221_reverse_immunity);        // 反转自身下降→免疫异常(王·酷烈风息)
    SKILL_EFFECT(5, effect_5_change_opp_stat);              // 通用: {n}%改变对手属性等级
    SKILL_EFFECT(1238, effect_1238_all_stats_doubled);        // 光荣之梦: 全属性+N(非混沌翻倍)
    // ── 批 4：圣灵谱尼 5000 的经典基础技能 ──
    SKILL_EFFECT(3, effect_3_clear_own_drops);          // 千烈虚光闪: 解除自身下降
    SKILL_EFFECT(33, effect_33_clear_opp_boosts);       // 圣堂之门: 消除对手提升
    SKILL_EFFECT(47, effect_47_stat_drop_immunity);    // {0}回合内免疫能力下降状态
    SKILL_EFFECT(180, effect_180_break_round);         // 消除对手回合类效果
    SKILL_EFFECT(521, effect_521_reverse_own_drops);   // 反转自身能力下降状态(AFTER_ACTION)
    SKILL_EFFECT(885, effect_885_all_stats_hp_higher); // 全属性+N,自身体力高于对手时翻倍
    SKILL_EFFECT(946, effect_946_all_stats_hp_lower);  // 全属性+N,自身体力低于对手时翻倍
    SKILL_EFFECT(962, effect_962_all_stats_no_boost);  // 全属性+N,自身不处于提升状态时翻倍
    SKILL_EFFECT(1001, effect_1001_all_stats_opp_anomaly); // 全属性+N,对手处于异常状态时翻倍
    SKILL_EFFECT(1014, effect_1014_all_stats_hp_above_ratio); // 全属性+N,体力高于最大体力1/N时翻倍
    // ── 批 6：全属性条件翻倍余量（2026-09-17）──
    SKILL_EFFECT(893, effect_893_all_stats_full_hp);     // 全属性+N,自身满体力时翻倍
    SKILL_EFFECT(911, effect_911_all_stats_has_boost);   // 全属性+N,自身处于提升状态时翻倍
    SKILL_EFFECT(1283, effect_1283_all_stats_first_move); // 全属性+N,先出手时翻倍
    SKILL_EFFECT(585, effect_585_multi_stat_up);       // 六项属性各带等级的提升(0=跳过)
    SKILL_EFFECT(749, effect_749_chance_all_stats_down); // {0}%令对手全属性-{1}

}  // namespace
