// J 组 J2 · 消强成功族二 —— 16 宏/141 载体（2026-10-01，worktree ../agents/j2）
//
// 家族语义（effect_info 模板即权威）：全部"消除对手能力提升状态，消除成功则 <后缀子句>"。
// 消强门槛 = clear_stat_boosts(opp) 返回 >0（1217/766/999900 先例——STAT_CLEAR 免消除
// 被挡时返回 0，门槛不触发）。后缀子句逐效果接既有原语，直系先例：
//   1170 附加固伤 → fixed_damage（吃护罩/固定抗性）
//   1009 对手全属性-N → stat_drop×6（弱化原语：查附加禁令/免弱，magnitude 正数，5 同款换算）
//   994  下{0}次技能先制+{1} → 载荷 charges + MOVE_RIGHT 常驻节点消费（d5_D4 星皇之怒同款；
//        "技能"不限攻击技 → 用 used_skill_this_round，不做 selected_skill_is_attack 过滤）
//   1037 下{0}次攻击伤害额外提升{1}% → grant_attack_boost 延后 AFTER_ACTION 授予 + count+1
//        补偿（1850 直系同款：SKILL_EFFECT 直接授予会落进自己这次攻击的 snapshot）
//   1974 恢复自身所有技能 PP → 效果 87 同款直写（pp<0 无限 PP 不动）
//   1978 {0}%令对手{1}，未触发 → 附加自身最大体力 1/{2} 粉伤 → apply_anomaly_checked
//        （概率申报/亮节闸门管辖）+ deal_pink_damage(PERCENT_VALUE)；"未触发"= 未落地
//        （免疫/抗性/反弹都算，batch_a 同判据 anomaly_landed）
//   1361 消强成功 → 恢复自身全部体力；若对手不处于能力提升状态 → {0}回合内对手无法通过
//        自身技能恢复体力（797 的 HEAL_BLOCK 票直系——门槛不同：797 门槛=消回合）
//   1864 {0}%令对手{1}，未触发则 {2}%令自身{3} → 双 apply_anomaly_checked（1138 同构，
//        第二段目标=自身）
//   1897 {0}回合内对手攻击伤害不超过{1}（766 的 CAP 直系，含 697 凭证消费）+ 若对手不处于
//        能力提升状态 → {2}回合内对手恢复效果减少{3}%（761 的 heal_mod_pct 窗同款）
//   1869 附加{0}点护罩 → hood_bank_.add（1351 同款；护罩只响应粉伤 FIXED/PERCENT）
//   2227 对手下{0}回合无法主动切换 → grant_switch_lock（2144 同款票）
//   2241 {0}回合内对手恢复效果减少{1}% → 761 直系（heal_mod_pct 授予即生效 + ROUND_START
//        捕获-还原 tick，窗口 {0}+1 格）
//   827  下{0}回合必定先出手 且 下{1}回合攻击伤害翻倍 → 694 必先载荷 + 776 增伤载荷，
//        但窗口起点用 NextRounds（⚠️ InRounds N=1 中途授予 = 死窗，载体全 [1,1]，场景
//        895 T13 抓出；Z8/Z2 背书的 694/776 载体 N≥2 时两口径恰好等价，见实现处注）
//   988  对手下{0}回合技能失效 → seal_skill SEAL_ALL（attribute+attack，695/52 同款参数；
//        ⚠️ "下N回合"= NextRounds：seal 票走 tick 递减 = 授予当回合就计入 → 直接授予会
//        覆盖 [R, R+{0}-1]，与官方"下一回合才失效"差一回合。用 register_next_rounds_effect
//        把授予推迟到 R+1 的 ROUND_START（一次性），窗口恰为 [R+1, R+{0}]）
//   2558 令对手{0}、{1}、{2}（{i}=异常码）→ 三连 apply_anomaly_checked（100%，现代通道）
//
// ⛔ 1642 跳过记档（工单缓办条款：单载体长尾允许跳过）："消除成功则{0}%随机为对手任意
//    技能散布{1}枚致命印记"——"技能上的致命印记"内核无对应机制（grep include/src/resources
//    零命中；pet.marks 的 Mark(id,name,count) 是蚩庸之锁专用形状，无消费方），唯一载体
//    34807 湮灭（[867, 1642, 1643]|[2, 100, 2, 100]，39xxx 新技能）。等内核有印记机制后
//    回补；34807 的 1642 位在 registry 记"未实现"。
//
// ⚠️ 窗口口径两族（勿混）：
//   - "{0}回合内"（1897/2241/1361 的窗）= InRounds 含当回合 [R, R+{0}-1]（766/470 口径）；
//   - "下{0}回合"（827/988）= NextRounds [R+1, R+{0}]（58/81/83/843 官方通用规则；
//     827 的 694/776 形态由 039 场景 Z8/Z2 回归背书；988/2227 用推迟授予实现，见上）。
//
// 载体切片双源核对（unity 库 json_each 精确匹配；38xxx 新技能 4399 缺文按模板权威）：
//   1170 蛮力苏醒 30739 [1170]|[200]、神耀光冲 31910 [300]
//   1009 王之觖望 28210 [1009]|[1]
//   994  幻魔天喑 30261 [994]|[1,2]、夺命斩 30597 [1,2]
//   1037 不朽圣光 35132 [1037,555]|[1,100,2]（555 兜余位）
//   1974 咏叹 37641 [781,1974,2203]|[2]（781 args_num=1 吃 [2]，1974/2203 零参）
//   1978 疾掠狩恶 38238 [1978,1441]|[100,30,3,2]（{1}=30 沉默，异常码口径 2189 同款）
//   1361 圣魔两重 35528 [1361,925]|[3,100]、瀑川叶雨 [1361,425]|[2,…]
//   1864 湮灭 34807 链外另查（1864×25 载体独立分布，切片 [chance,anomaly,chance2,anomaly2]）
//   1897 兰帕尔特光弹 37517 [1897,894]|[2,300,2,50,150]、热忱凝铸 [1700,1897]|[1,3,200,3,50]
//   1869 判视 36754 [1869,1845]|[400,…]、弹跳飞踢 [889,1869]|[7,250]
//   2227 地拜·千里一息 38271 [2227,…]|[2,…]、迅猛踢击 [195,1103,2227]|[50,1]（1103 零参）
//   2241 免疫侵蚀 38297 [2241,436]|[2,50,30]
//   827  魔神幽冥 18002 [827]|[1,1]、超限极光 [827,1957,1958]|[1,1]
//   988  光明圣剑 30036 [988,101]|[1,50]、逆世强袭 [988,679]|[1,3]
//   2558 星界圣令 38525 [2556,954,2557,2558,514]|[22,15,0,2,5,10,40] → 2558=[2,5,10]
//        （烧伤/冻伤/混乱，异常码 ≤43 全集）
//
// 动工前置三查（2026-10-01）：①内核 grep 无 16 id 硬编码 ②custom_effect_programs/
// overrides 无声明（纯 SKILL_EFFECT 即时型 + 自管窗口）③moves_lib 无既有注册——真缺口。
//
// 场景回归：test/scenario/scenario_895_purge_success2.cpp（号段 895-899 一文件）。

#include "common.h"

namespace {

// ── 共用：消强门槛与"对手不处于能力提升状态"判据 ──────────────────────────

// 消除成功 = 清掉个数 >0（1217/766 先例；STAT_CLEAR 免消除被挡 → 返回 0 → 子句不触发）。
inline bool purge_succeeded(BattleContext* ctx, int opp) {
    return g_core && g_core->clear_stat_boosts && g_core->clear_stat_boosts(ctx, opp) > 0;
}

// "对手不处于能力提升状态"（1361/1897 条件子句）：本体六槽无正等级
// （962 同判据：含命中共 6 槽，用户 2026-09-18 口径）。
// 消强成功后此条件恒真（clear_stat_boosts 全清）——官方文本语序如此，显式判、不省略。
inline bool has_no_stat_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

// 现代施加"落地"判定（batch_a/burn_trio 同款口径）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// storage 整数读写（d5_D4 同款）。
int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// 桶/行动方小工具（batch_c_skills.cpp 同款）——1037 的 AFTER_ACTION 授予防误 consume。
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

// ── 2241 / 1897 共用：恢复效果减少窗（761 直系：授予即生效 + ROUND_START 捕获-还原）──
// tick payload = {owner, opp, rounds, pct, orig, arm_round}
EffectResult j2_heal_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    const int orig = args.int_args[4];
    const int arm = args.int_args[5];
    if (opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ctx->heal_mod_pct[opp] =
        (ctx->roundCount <= arm + rounds - 1) ? (orig - pct) : orig;
    return EffectResult::kOk;
}

// 授予恢复减少窗（effect_id 只作桶键，2241/1897 各传各的）。
void grant_heal_reduce_window(BattleContext* ctx, int owner, int opp, int effect_id,
                              int rounds, int pct) {
    const int orig = ctx->heal_mod_pct[opp];
    ctx->heal_mod_pct[opp] = orig - pct;   // 授予即生效（首手当回合）
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, effect_id, rounds + 1,
                          {owner, opp, rounds, pct, orig, ctx->roundCount},
                          &j2_heal_reduce_tick);
}

// ── 1170 —— 消除成功则附加{0}点固定伤害 ──
// int_args = [owner, target, {0}]（args_num=1 → int_count>=3）。
EffectResult effect_1170_purge_then_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1170, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1170, args,
                     "1170 布局=[owner,target,固伤]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1170,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 1170, args, "需要 固伤 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;   // "消除成功"门槛
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, amount);
    }
    return EffectResult::kOk;
}

// ── 1009 —— 消除成功则对手全属性-{0} ──
EffectResult effect_1009_purge_then_all_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1009, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1009, args,
                     "1009 布局=[owner,target,全属性降幅]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int levels = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1009,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(levels > 0, 1009, args, "需要 降幅 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, levels, owner);   // 弱化原语：查免弱（5 同款）
    }
    return EffectResult::kOk;
}

// ── 994 —— 消除成功则自身下{0}次技能先制+{1} ──
// 载荷 charges（storage）+ MOVE_RIGHT 常驻节点消费（d5_D4 星皇之怒同款）。
// 节点伪 id（10^9 段外，只作桶键；重复授予 = 同键替换刷新）。
constexpr int kNode994Priority = 990099401;
constexpr int k994ChargesBase = 990099411;   // +owner

EffectResult effect_994_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    const int key = k994ChargesBase + owner;
    const int charges = storage_int(ctx, key);
    // "下{0}次技能"不限攻击技——只要本回合真点了技能就消费。⚠️ 本 tick 在 MOVE_RIGHT
    // （先制授予必须落在排序前）→ 技能尚未执行，只能用**选择位**判据（判据统一 2026-10-03
    // 的选择期例外；executed_skill_this_round 在此恒 false）。
    if (charges <= 0 || !used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    ctx->plugin_storage[key] = charges - 1;
    return EffectResult::kOk;
}

EffectResult effect_994_purge_then_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 994, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 994, args,
                     "994 布局=[owner,target,次数,先制加成]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 994,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(times > 0 && bonus != 0, 994, args, "需要 次数>0 且 加成≠0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k994ChargesBase + owner] =
        storage_int(ctx, k994ChargesBase + owner) + times;
    // 常驻节点重复授予 = 同键替换（窗口刷新为 999 回合≈战斗长；payload 刷成最新加成）。
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, kNode994Priority,
                          /*rounds=*/999, {owner, bonus}, &effect_994_priority_tick);
    return EffectResult::kOk;
}

// ── 1037 —— 消除成功则自身下{0}次攻击技能伤害额外提升{1}% ──
// 1850 直系：授予必须延后到 AFTER_ACTION（直接授予会落进自己这次攻击的 snapshot）。
EffectResult effect_1037_grant_flush(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, times, pct, arm_round}
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int times = args.int_args[1];
    const int pct = args.int_args[2];
    const int arm_round = args.int_args[3];
    if (owner < 0 || owner > 1 || bucket_acting_side(ctx) != owner
        || arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    // 授 {0}+1 次（G1 发现：消费点 query_usage 在声明即扣、应用点读消费后余量 →
    // 授 N 实际命中 N-1；+1 补偿 = 1179/1850 同款）。
    ctx->grant_attack_boost(owner, 1037, /*count=*/times + 1, /*pct=*/pct);
    return EffectResult::kOk;
}

EffectResult effect_1037_purge_then_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1037, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1037, args,
                     "1037 布局=[owner,target,次数,提升%]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1037,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(times > 0 && pct > 0, 1037, args, "需要 次数>0 且 提升%>0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, times, pct, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1037, 0, owner, 1, EffectArgs(payload), &effect_1037_grant_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1037;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1974 —— 消除成功则恢复自身所有技能 PP 值 ──
EffectResult effect_1974_purge_then_pp_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1974, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1974, args,
                     "1974 布局=[owner,target]（0 参效果，装载器注入 owner/target）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1974,
                     args, "需要 owner/target ∈ {0,1}");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    for (Skills& sk : ctx->getPet(owner).skills) {
        if (sk.pp < 0) {
            continue;   // 无限 PP 技能不动（87 同款）
        }
        sk.pp = sk.maxPP;
    }
    return EffectResult::kOk;
}

// ── 1978 —— 消除成功则{0}%令对手{1}，未触发则附加自身最大体力 1/{2} 的百分比伤害 ──
EffectResult effect_1978_purge_anomaly_or_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1978, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1978, args,
                     "1978 布局=[owner,target,概率,异常码,粉伤分母]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1978,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && denom > 0, 1978, args, "需要 概率>0 且 分母>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1978, args,
                     "需要 异常码 ∈ [0,43]");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 概率申报版（亮节闸门管辖）；"未触发"= 未落地（免疫/抗性/反弹都算，batch_a 同判据）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (anomaly_landed(r)) {
        return EffectResult::kOk;
    }
    if (g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, max_hp_of(ctx, owner) / denom,
                                       DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1361 —— 消除成功则恢复自身全部体力；若对手不处于能力提升状态则{0}回合内对手无法
//    通过自身技能恢复体力 ──
EffectResult effect_1361_purge_heal_healblock(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1361, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1361, args,
                     "1361 布局=[owner,target,封回血回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1361,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1361, args, "需要 回合 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->heal_amount) {
        (void)heal_max_hp_fraction(ctx, owner, 1);   // 恢复自身全部体力（吃封回血/修正）
    }
    if (has_no_stat_boost(ctx, opp) && g_core) {
        // 封回血挂对手身上（797 的 HEAL_BLOCK 票同款；"{0}回合内"含当回合）
        ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                            /*duration_rounds=*/rounds, /*source_id=*/1361,
                            /*soul_immunity=*/false, EffectScope::ON_STAGE,
                            /*counts=*/0, /*source_effect_id=*/1361);
    }
    return EffectResult::kOk;
}

// ── 1864 —— 消除成功则{0}%令对手{1}，未触发则{2}%令自身{3} ──
EffectResult effect_1864_purge_double_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1864, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1864, args,
                     "1864 布局=[owner,target,概率,异常码,概率2,异常码2]（int_count>=6）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int chance2 = args.int_args[4];
    const int anomaly2 = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1864,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && chance2 > 0, 1864, args, "需要 两段概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly) && is_valid_abnormal_status_id(anomaly2),
                     1864, args, "需要 异常码 ∈ [0,43]");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (anomaly_landed(r)) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, owner, anomaly2, random_anomaly_duration(),
                                        owner, chance2, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1897 —— 消除成功则{0}回合内对手攻击伤害不超过{1}；若对手不处于能力提升状态则
//    {2}回合内对手所有体力恢复效果减少{3}% ──
EffectResult effect_1897_purge_cap_healreduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1897, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1897, args,
                     "1897 布局=[owner,target,回合,锁伤上限,回合2,减少%]（int_count>=6）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int cap = args.int_args[3];
    const int rounds2 = args.int_args[4];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1897,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && cap > 0 && rounds2 > 0 && pct > 0, 1897, args,
                     "需要 回合/上限/回合2/减少% 均 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    // ① 锁伤 CAP（766 直系，含 697 凭证消费与 ON_STAGE 绑定）
    const int r0 = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [owner, opp, rounds, cap, r0](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != opp || !d.isRed || d.final <= 0) {
                return;
            }
            if (c->roundCount >= r0 + rounds) {
                return;   // "{0}回合内"含当回合（[r0, r0+rounds-1]，766 同款）
            }
            if (c->ws.attack_credential[d.attackerId].ignore_damage_limit) {
                return;   // 697「无视伤害限制效果」凭证可穿锁伤（766 同款）
            }
            if (d.final > cap) {
                d.final = cap;
            }
        });
    // ② 条件恢复减少窗（761 直系；条件=对手不处于能力提升状态，消强成功后恒真、显式判）
    if (has_no_stat_boost(ctx, opp)) {
        grant_heal_reduce_window(ctx, owner, opp, 1897, rounds2, pct);
    }
    return EffectResult::kOk;
}

// ── 1869 —— 消除成功则为自身附加{0}点护罩 ──
EffectResult effect_1869_purge_then_hood(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1869, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1869, args,
                     "1869 布局=[owner,target,护罩点数]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1869,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 1869, args, "需要 护罩点数 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).hood_bank_.add(/*priority=*/0, amount, /*source_id=*/1869,
                                      /*duration=*/0);
    return EffectResult::kOk;
}

// ── 2227 —— 消除成功则对手下{0}回合无法主动切换精灵 ──
// "下N回合"= NextRounds：锁切票的 register_round 显式锚定 → 推迟到 R+1 的 ROUND_START
// 一次性授予（锚 = 届时 roundCount），窗口恰为 [R+1, R+{0}]（988 同款推迟，见文件头）。
constexpr int kNode2227Lock = 990099421;

EffectResult effect_2227_lock_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/2227, opp, rounds,
                                        ctx->roundCount);
    return EffectResult::kOk;
}

EffectResult effect_2227_purge_then_switch_lock(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2227, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2227, args,
                     "2227 布局=[owner,target,锁切回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2227,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 2227, args, "需要 回合 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, kNode2227Lock,
                                /*rounds=*/1, {owner, opp, rounds}, &effect_2227_lock_tick);
    return EffectResult::kOk;
}

// ── 2241 —— 消除成功则{0}回合内对手体力恢复效果减少{1}% ──
EffectResult effect_2241_purge_then_healreduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2241, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2241, args,
                     "2241 布局=[owner,target,回合,减少%]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2241,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 2241, args, "需要 回合>0 且 减少%>0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    grant_heal_reduce_window(ctx, owner, opp, 2241, rounds, pct);
    return EffectResult::kOk;
}

// ── 827 —— 消除成功则自身下{0}回合必定先出手且下{1}回合造成的攻击伤害翻倍 ──
// 必先（694 同形）+ 伤害翻倍（776 同形）。⚠️ 窗口起点用 **NextRounds**（显式 R+1 起算）：
// InRounds N=1 的窗在回合中途授予时 [R, R] 已无未来时点 = 死窗（827 载体全是 [1,1]，
// 实测场景 895 T13 抓出）；Z2/Z8 的 694/776 形态 N≥2 时 InRounds 恰好等价 NextRounds，
// N=1 才暴露差别。NextRounds 对全部 N 正确（"下N回合"官方语义，58/81/83/843 同族）。
EffectResult effect_827_purge_then_first_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 827, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 827, args,
                     "827 布局=[owner,target,必先回合,翻倍回合]（int_count>=4）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int rounds2 = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 827,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && rounds2 > 0, 827, args, "需要 两段回合 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    // 必先：R+1..R+{0} 的 MOVE_RIGHT 每回合置 ws.guaranteed_first（tier=9，694 同款载荷）
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 827, rounds,
                                {owner, 1 - owner, /*tier=*/9}, &effect_set_guaranteed_first);
    // 翻倍：R+1..R+{1} 的 ROUND_START 每回合往 ws.damage_add_pct 累加 100（776 同款载荷）
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 827, rounds2,
                                {owner, 1 - owner, /*pct=*/100}, &effect_set_damage_amp);
    return EffectResult::kOk;
}

// ── 988 —— 消除成功则对手下{0}回合技能失效 ──
// SEAL_ALL 封技（attribute+attack，52/695 参数同款：可穿、hit_invalid=false）。
// "下N回合"= NextRounds：推迟到 R+1 的 ROUND_START 一次性授予（见文件头口径记档）。
constexpr int kNode988Seal = 990099431;

EffectResult effect_988_seal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->seal_skill) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/988,
                                 /*attribute=*/true, /*attack=*/true, /*count=*/1,
                                 /*duration_rounds=*/rounds, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

EffectResult effect_988_purge_then_seal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 988, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 988, args,
                     "988 布局=[owner,target,封技回合]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 988,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 988, args, "需要 回合 > 0");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, kNode988Seal,
                                /*rounds=*/1, {owner, opp, rounds}, &effect_988_seal_tick);
    return EffectResult::kOk;
}

// ── 2558 —— 消除成功则令对手{0}、{1}、{2}（{i}=异常码，100% 现代通道）──
EffectResult effect_2558_purge_then_triple_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2558, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2558, args,
                     "2558 布局=[owner,target,异常码×3]（int_count>=5）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int a1 = args.int_args[2];
    const int a2 = args.int_args[3];
    const int a3 = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 2558,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(a1) && is_valid_abnormal_status_id(a2)
                         && is_valid_abnormal_status_id(a3), 2558, args,
                     "需要 异常码 ∈ [0,43]");
    if (!purge_succeeded(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    for (const int status : {a1, a2, a3}) {
        (void)g_core->apply_anomaly_checked(ctx, opp, status, random_anomaly_duration(),
                                            owner, /*chance=*/100, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1170, effect_1170_purge_then_fixed)
SKILL_EFFECT(1009, effect_1009_purge_then_all_down)
SKILL_EFFECT(994, effect_994_purge_then_priority)
SKILL_EFFECT(1037, effect_1037_purge_then_boost)
SKILL_EFFECT(1974, effect_1974_purge_then_pp_full)
SKILL_EFFECT(1978, effect_1978_purge_anomaly_or_pink)
SKILL_EFFECT(1361, effect_1361_purge_heal_healblock)
SKILL_EFFECT(1864, effect_1864_purge_double_anomaly)
SKILL_EFFECT(1897, effect_1897_purge_cap_healreduce)
SKILL_EFFECT(1869, effect_1869_purge_then_hood)
SKILL_EFFECT(2227, effect_2227_purge_then_switch_lock)
SKILL_EFFECT(2241, effect_2241_purge_then_healreduce)
SKILL_EFFECT(827, effect_827_purge_then_first_double)
SKILL_EFFECT(988, effect_988_purge_then_seal)
SKILL_EFFECT(2558, effect_2558_purge_then_triple_anomaly)
// 1642 跳过（"致命印记"无内核机制，见文件头记档）
