// J 组 J4 · 吸取强化成功族 —— 13 宏/110 载体（2026-10-01，worktree ../agents/j4）
//
// 家族语义（effect_info 模板即权威）：全部"吸取对手能力提升状态，吸取成功则 <后缀子句>"。
// 吸取门槛 = transfer_stat_boosts(opp→self) 返回 >0（85 转化/1287 吸取共用原语；batch_a
// 206/522 同款门槛判法——搬到自身，与 J2 消强族 clear_stat_boosts 门槛区分：吸取的强化
// **归我**）。后缀子句直系先例：
//   1159 免疫下{0}次异常 → 1099 直系（grant_immunity counts 型，duration=0）
//   1022 吸取{0}点体力 → 2274 点数吸血口径（fixed_damage + 按实际扣减回血；引擎无原语）
//   1261 附加{0}固伤 → fixed_damage（J2/1170 同款）
//   1776 {0}回合内对手攻击技无法造成伤害且命中效果失效 → hit_effect_invalid(kFullNull,
//        attack)——738 同形（738 以 count=null_rounds 表达回合窗，本族同口径：{0}回合内
//        ≈ {0}次攻击，1 次/回合近似，每回合额外行动不计记档）
//   1726 下{0}回合对手所有技能先制-{1} → effect_move_right_priority_bonus 负载荷挂
//        对手桶（MOVE_RIGHT 双侧桶都跑，battleFsm 1676/1677 实证）+ 条件全属性降
//   1542 令对手{0}(异常码) + 条件全属性降 → apply_anomaly_checked(100%)
//   1354 吸取{0}点体力 + 条件全属性降 → 2274 口径
//   1681 下{0}次受到的攻击伤害减少{1}点 → REDUCE_FLAT 管线回调 + 闭包计数（816 的
//        "计数放自己闭包"口径；811 是一次性、本条是次数型）
//   1310 自身下{0}回合必定先出手 → ws.guaranteed_first 窗（827 同款 NextRounds 口径）
//   2444 对手{0}回合内属性技能无效（695 同款 SEAL_ATTRIBUTE 回合窗）+ 条件：自身下{1}
//        回合攻击技能先制+{2}（481 同判据 selected_skill_is_attack 的 MOVE_RIGHT 桶 tick）
//   2385 免疫下{0}次异常（1159 同款）+ 条件：消除对手回合类效果（797 的 SUCCESS 门槛）
//   2361 使自身{0}(异常码) → apply_anomaly_checked 目标=自身（玄羽悼唁切片 {0}=14 狂暴）
//   2047 {0}回合内对手属性技能命中效果失效 → hit_effect_invalid(kEffectsOnly, attribute)
//        ——batch_b 1727 同款调用形
//
// ⚠️ "若对手不处于能力提升状态"条件子句（1726/1542/1354/2444/2385）：吸取成功后对手
//    的正等级已整体搬到自身 → 条件恒真（has_no_stat_boost 显式判、不省略；与 J2 1361
//    同款记档——官方文本语序如此）。
// ⚠️ 窗口口径："{0}回合内"（1776/2047/2444 前半）= InRounds 含当回合 → 立即授予；
//    "下{0}回合"（1726/1310/2444 后半）= NextRounds（register_next_rounds_effect，
//    InRounds N=1 中途授予=死窗，J2 场景 T13 抓出的坑）。
// ⚠️ 认证数据层：1159 有 anomaly_immunity_tier=modern 声明（grant_immunity 默认 modern
//    层，口径一致）。
//
// 载体切片双源核对（unity 库 json_each 精确匹配）：
//   1159 星河永寂 30679 [1159,594]|[1,…]、瞳·恩断义绝 [1080,1159,…]|[1,3]
//   1022 光火凯歌 19897 [1022,583]|[300,1]、暗魂枪 [1022,679]|[300,2]
//   1261 极度幻光 32250 [1261]|[300]（单载体）
//   1776 缭乱 35842 [1381,1776]|[2,2]、留白 [494,1776]|[2]（494 零参）
//   1726 烬火无息 36648 [2018,1726,776]|[1,2,1,2]→[1,2,1]、阴阳三合 [1726,…]|[1,2,1,…]
//   1542 藐世不语 34258 [1542,555]|[20,1,2]、滅天闪 [1542,977]|[30,1,…]
//   1354 X·磁暴 32330 [1354,776]|[300,1,2]、速射黑芒 [1354,628]|[200,1,50]
//   1681 天鹅星光 34977 [1681,693]|[1,200,…]、秋影月波 [1681,894]|[1,200,200]
//   1310 夜洛乌泽 31649 [1310,1030]|[1,6]、神羽逆卷 [942,1310,…]|[3,1,…]
//   2444 古神长嚎 38356 [2331,2444,1895,2443]|[2,1,2,300,100,6,2] → 2444=[2,1,2]
//   2385 万妖挽 38232 [2385,…]|[1,…]
//   2361 玄羽悼唁 38147 [2358,1371,2361,831,2105]|[5,14,20] → 2361=[14]
//   2047 诡魅幽冥诀 36762 [2047,…]|[2,…]
//
// 动工前置三查（2026-10-01）：①内核 grep 无 13 id 硬编码 ②custom_effect_overrides 仅
// 1159 anomaly_immunity_tier=modern（口径一致）③moves_lib 无既有注册——真缺口。
//
// 场景回归：test/scenario/scenario_905_drain_boost.cpp（号段 905-909 一文件）。

#include "common.h"

namespace {

// ── 共用：吸取门槛与条件判据 ──────────────────────────────────────────────

// 吸取成功 = 对手正等级整体搬到自身（返回搬走项数 >0；被免消除强化挡下返回 0）。
inline bool drain_succeeded(BattleContext* ctx, int owner, int opp) {
    return g_core && g_core->transfer_stat_boosts
        && g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) > 0;
}

// "对手不处于能力提升状态"（六槽无正等级；吸取成功后恒真，显式判——见文件头记档）。
inline bool has_no_stat_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 条件"全属性-{n}"（六槽 stat_drop，弱化原语查免弱；1009 同款）。
void all_stats_down(BattleContext* ctx, int owner, int opp, int n) {
    if (!g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, n, owner);
    }
}

// 点数吸血（2274 口径：fixed_damage 走固伤通道吃护盾/免粉，按**实际扣减**回血、
// 回血吃封回血与恢复修正）。
void drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
}

// 2444 条件半段：自身下{1}回合攻击技能先制+{2}（MOVE_RIGHT 桶 tick，481 同判据）。
EffectResult j4_attack_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    // ⚠️ MOVE_RIGHT 桶（先制授予必须落在排序前）→ 技能尚未执行，选择位判据（判据统一
    //    2026-10-03 的选择期例外，481 同款；executed_* 在此恒 false）。
    if (!used_skill_this_round(ctx, owner) || !selected_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // "攻击技能"限定：属性技不加分
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// ── 1159 —— 吸取成功则自身免疫下{0}次受到的异常状态 ──
EffectResult effect_1159_drain_then_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1159, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1159, args,
                     "1159 布局=[owner,target,免疫次数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1159,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(counts > 0, 1159, args, "需要 免疫次数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    // 1099 直系（count 型免异常票：duration=0、counts={0}；modern 层与认证声明一致）
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/1159,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/1159);
    return EffectResult::kOk;
}

// ── 1022 —— 吸取成功则吸取对手{0}点体力 ──
EffectResult effect_1022_drain_then_drain_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1022, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1022, args,
                     "1022 布局=[owner,target,吸取点数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1022,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 1022, args, "需要 吸取点数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    drain_points(ctx, owner, opp, points);
    return EffectResult::kOk;
}

// ── 1261 —— 吸取成功则附加{0}点固定伤害 ──
EffectResult effect_1261_drain_then_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1261, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1261, args,
                     "1261 布局=[owner,target,固伤]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1261,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 1261, args, "需要 固伤 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, amount);
    }
    return EffectResult::kOk;
}

// ── 1776 —— 吸取成功则{0}回合内对手攻击技能无法造成伤害且命中效果失效 ──
// hit_invalid 票语义（rule_center.h:19 + consume_hit_invalid 匹配 t.target==防守方）：
// target = **被保护侧**（打向它的技能被命中失效）→ "对手攻击无效"传 owner。
// count={0} 表达回合窗（1 次/回合近似，记档见文件头）；"{0}回合内"含当回合 → 立即授予。
EffectResult effect_1776_drain_then_attack_null(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1776, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1776, args,
                     "1776 布局=[owner,target,回合数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1776,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1776, args, "需要 回合数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->hit_effect_invalid) {
        g_core->hit_effect_invalid(ctx, /*target=*/owner, HitInvalidMode::kFullNull,
                                   /*count=*/rounds, /*is_attribute_skill=*/false, 1776);
    }
    return EffectResult::kOk;
}

// ── 1726 —— 吸取成功则下{0}回合对手所有技能先制-{1}，若对手不处于能力提升状态则
//    令对手全属性-{2} ──
EffectResult effect_1726_drain_then_prio_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1726, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1726, args,
                     "1726 布局=[owner,target,回合,先制降幅,全属性降幅]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int prio_down = args.int_args[3];
    const int all_down = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1726,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && prio_down > 0 && all_down > 0, 1726, args,
                     "需要 回合/降幅 均 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    // 对手 MOVE_RIGHT 桶负载荷（双侧桶都跑；"下{0}回合"= NextRounds）。
    // 载荷布局 = effect_move_right_priority_bonus 的 [owner, target, bonus]（bonus 第 3 位）。
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, opp, 1726, rounds,
                                {opp, 1 - opp, -prio_down},
                                &effect_move_right_priority_bonus);
    if (has_no_stat_boost(ctx, opp)) {
        all_stats_down(ctx, owner, opp, all_down);
    }
    return EffectResult::kOk;
}

// ── 1542 —— 吸取成功则令对手{0}，若对手不处于能力提升状态则令对手全属性-{1} ──
EffectResult effect_1542_drain_then_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1542, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1542, args,
                     "1542 布局=[owner,target,异常码,全属性降幅]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int all_down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1542,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly) && all_down > 0, 1542, args,
                     "需要 异常码 ∈ [0,43] 且 降幅 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                            owner, /*chance=*/100, ChanceSource::Skill);
    }
    if (has_no_stat_boost(ctx, opp)) {
        all_stats_down(ctx, owner, opp, all_down);
    }
    return EffectResult::kOk;
}

// ── 1354 —— 吸取成功则吸取对手{0}点体力，若对手不处于能力提升状态则令对手全属性-{1} ──
EffectResult effect_1354_drain_then_drain_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1354, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1354, args,
                     "1354 布局=[owner,target,吸取点数,全属性降幅]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    const int all_down = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1354,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0 && all_down > 0, 1354, args, "需要 点数/降幅 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    drain_points(ctx, owner, opp, points);
    if (has_no_stat_boost(ctx, opp)) {
        all_stats_down(ctx, owner, opp, all_down);
    }
    return EffectResult::kOk;
}

// ── 1681 —— 吸取成功则下{0}次受到的攻击伤害减少{1}点 ──
// REDUCE_FLAT + 闭包计数（816 口径：计数放自己闭包，一次注册一条、条内自减）。
EffectResult effect_1681_drain_then_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1681, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1681, args,
                     "1681 布局=[owner,target,次数,减伤点数]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    const int flat = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1681,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(times > 0 && flat > 0, 1681, args, "需要 次数/点数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    auto remaining = std::make_shared<int>(times);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [owner, flat, remaining](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || *remaining <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            --*remaining;   // "下{0}次"=逐次消费（n 用尽自然失效）
            d.final = std::max(0, d.final - flat);
        });
    return EffectResult::kOk;
}

// ── 1310 —— 吸取成功则自身下{0}回合必定先出手 ──
EffectResult effect_1310_drain_then_first(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1310, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1310, args,
                     "1310 布局=[owner,target,回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1310,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1310, args, "需要 回合 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1310, rounds,
                                {owner, 1 - owner, /*tier=*/9}, &effect_set_guaranteed_first);
    return EffectResult::kOk;
}

// ── 2444 —— 吸取成功则对手{0}回合内属性技能无效，对手不处于能力提升状态时自身下{1}
//    回合攻击技能先制+{2} ──
constexpr int kNode2444Prio = 990244401;

EffectResult effect_2444_drain_then_seal_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2444, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2444, args,
                     "2444 布局=[owner,target,封属回合,先制回合,先制加成]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int seal_rounds = args.int_args[2];
    const int prio_rounds = args.int_args[3];
    const int prio_bonus = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2444,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(seal_rounds > 0 && prio_rounds > 0 && prio_bonus > 0, 2444, args,
                     "需要 三参数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    // 前半：{0}回合内属性技能无效（695 同款 SEAL_ATTRIBUTE 回合窗，含当回合）
    if (g_core && g_core->seal_skill) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/2444,
                                 /*attribute=*/true, /*attack=*/false, /*count=*/1,
                                 /*duration_rounds=*/seal_rounds, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    // 后半（条件恒真，显式判）：自身下{1}回合攻击技能先制+{2}（481 同判据 tick）
    if (has_no_stat_boost(ctx, opp)) {
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, kNode2444Prio,
                                    prio_rounds, {owner, prio_bonus},
                                    &j4_attack_priority_tick);
    }
    return EffectResult::kOk;
}

// ── 2385 —— 吸取成功自身免疫下{0}次受到的异常状态，若对手不处于能力提升状态则消除
//    对手回合类效果 ──
EffectResult effect_2385_drain_immune_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2385, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2385, args,
                     "2385 布局=[owner,target,免疫次数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2385,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(counts > 0, 2385, args, "需要 免疫次数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/2385,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/2385);
    if (has_no_stat_boost(ctx, opp) && g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, opp);   // 797 同款（SUCCESS 才算，结果不分支）
    }
    return EffectResult::kOk;
}

// ── 2361 —— 吸取成功使自身{0}（{0}=异常码，玄羽悼唁切片=14 狂暴）──
EffectResult effect_2361_drain_then_self_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2361, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2361, args,
                     "2361 布局=[owner,target,自身异常码]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2361,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2361, args,
                     "需要 异常码 ∈ [0,43]");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, owner, anomaly, random_anomaly_duration(),
                                            owner, /*chance=*/100, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 2047 —— 吸取成功则{0}回合内对手属性技能命中效果失效 ──
// target=被保护侧（1776 同款口径）：对手属性技打向自身 → 失效。
EffectResult effect_2047_drain_then_attr_hit_invalid(BattleContext* ctx,
                                                     const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2047, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2047, args,
                     "2047 布局=[owner,target,回合数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2047,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 2047, args, "需要 回合数 > 0");
    if (!drain_succeeded(ctx, owner, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->hit_effect_invalid) {
        g_core->hit_effect_invalid(ctx, /*target=*/owner, HitInvalidMode::kEffectsOnly,
                                   /*count=*/rounds, /*is_attribute_skill=*/true, 2047);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1159, effect_1159_drain_then_immune)
SKILL_EFFECT(1022, effect_1022_drain_then_drain_hp)
SKILL_EFFECT(1261, effect_1261_drain_then_fixed)
SKILL_EFFECT(1776, effect_1776_drain_then_attack_null)
SKILL_EFFECT(1726, effect_1726_drain_then_prio_down)
SKILL_EFFECT(1542, effect_1542_drain_then_anomaly)
SKILL_EFFECT(1354, effect_1354_drain_then_drain_down)
SKILL_EFFECT(1681, effect_1681_drain_then_reduce)
SKILL_EFFECT(1310, effect_1310_drain_then_first)
SKILL_EFFECT(2444, effect_2444_drain_then_seal_prio)
SKILL_EFFECT(2385, effect_2385_drain_immune_break)
SKILL_EFFECT(2361, effect_2361_drain_then_self_anomaly)
SKILL_EFFECT(2047, effect_2047_drain_then_attr_hit_invalid)
