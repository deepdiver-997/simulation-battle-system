// 莫塔里安 4541 技能组（2026-09-20）
//
// 官方技能全文 = reference/scrape_4399/out/pets_full.jsonl（莫塔里安节）；
// 效果参数与模板 = DB effect_info（args 序 = [owner, target, params...]，bind 时前两位注入）。
//
// 已实现：
//   36791 禁命永囚劫（第五 160）：951 反转对手提升→成功免异常 / 1809 200护罩+消失害怕 /
//        1478 **锁切**（挂对手、回合类窗口）/ 793 低伤→下3回合每回合300固伤
//   36786 黯狱责罚（先三 85）：942 断回合成功吸1/3 / **531 低伤概率害怕**
//        （官方公式 = DB effect_info 模板：「伤害低于280时每相差2点有1%概率使对手害怕」，
//          封顶100% —— 用户 2026-09-20 问询项，模板即权威数据，无需另查文章）
//   36790 尘沙结天咒（150）：1070 对手处于能力下降→先制+1 / 882 每处于一种提升威力+30 /
//        1209 消除双方能力升降，成功则附加段数×50固伤
//   36789 幻砂迷魄（140）：1077 3回合内受攻击则害怕，未触发则对手2回合属性技无效
//   36788 漫空埃葬（130）：1613 不处于提升则吸200，先手额外吸100
//   36787 彼方断罪（120）：744 体力大于对手附加300固伤
//   28406 伏灵罪印：922 4回合每回合结束解除并反馈自身下降（191/1122 已有）
//   28407 禁缚悼文：1910 全属性+1护罩时翻倍 / 717 吸1/3低于1/2翻倍（191/843 已有）/
//        602 3回合内使用攻击技能则对手特防-1速度-1
//   24508 泰坦之盔：693/481 已有
//   28405 此方镇魂：677 已有
//   18195 天地聚灵：85/521 已有；18196 泰坦震荡：447 已有
//
// ⚠️ 锁切挂**对手**身上（官方 1478「下{0}回合令对手无法主动切换精灵」），与冰魄·柯尔德
//    神谕属性技同款口径；查询口在 battleFsm 主动切换校验（RuleCenter SWITCH_LOCK 纯查询票）。
#include "common.h"

namespace {

// 注册时点补丁（skills.cpp 的 effect_register_state）：
//   531 / 793 读管线后 final → AFTER_ACTION；1070 读先手权判定 → MOVE_RIGHT。
// 见 lib_1.cpp 的注册段注释。

// ── 531 造成的伤害低于280时每相差2点有1%的概率使对手害怕 ──
// Args=[owner,target]。时点 = AFTER_ACTION（读 final，1256/1306 同族口径）。
// 官方公式：概率 = (280 - 最终伤害) / 2 %，封顶 100%（差 1 点不计，"每相差2点"）。
EffectResult effect_531_low_damage_fear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    constexpr int kThreshold = 280;
    const int dealt = ctx->resolvedDamage.final;
    if (dealt >= kThreshold) {
        return EffectResult::kOk;   // 伤害达标 → 不触发
    }
    int chance = (kThreshold - dealt) / 2;
    if (chance > 100) {
        chance = 100;
    }
    if (!roll_skill_percent(*ctx, 531, 0, owner, target, chance)) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, target, static_cast<int>(AbnormalStatusId::Fear),
                          random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 942 消除对手回合类效果，消除成功则吸取对手最大体力的1/{0} ──
EffectResult effect_942_break_and_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2] > 0 ? args.int_args[2] : 3;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->break_round_effects || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, target) == BreakResult::SUCCESS) {
        g_core->drain_hp(ctx, owner, target, denom);
    }
    return EffectResult::kOk;
}

// ── 1478 下{0}回合令对手无法主动切换精灵（锁切；挂对手）──
// 规则票 SWITCH_LOCK（纯查询、免断回合、窗口型）；同 source_effect_id 覆盖刷新 = 续窗。
EffectResult effect_1478_switch_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 官方措辞是"**下**{0}回合" → NextRounds 家族（本回合不算，从下一回合起算）——
    // 这正是"本回合出手锁住对手**下一次**操作期的切换"的语义。
    // 票**绑定对手**（binding_side=target）：对手切换/被断回合都失效（2026-09-21 口径）——
    // source_valid_id 传被锁方当前 epoch 快照，断回合 bump 后 invalidate_stale 删票。
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/1478, target, rounds, start,
                                        EffectScope::ON_STAGE,
                                        /*source_valid_id=*/ctx->round_effect_valid_id[target]);
    return EffectResult::kOk;
}

// ── 951 反转对手能力提升状态，反转成功则己方免疫下{0}次受到的异常状态 ──
// ⚠️ source_effect_id 与其它免异常票错开（覆盖键含 subtype，撞键会互相顶掉）。
EffectResult effect_951_boost_reversal_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_boost_reversal
        || g_core->stat_boost_reversal(ctx, target) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 未满足条件 / 被免弱挡下 → 不授票
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/45951, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/counts, /*source_effect_id=*/951);
    return EffectResult::kOk;
}

// ── 1809 获得{0}点护罩，护罩消失时{1}%令对手{2} ──
// 护罩按魂印专属优先级、source 锚定本效果；消失检测走 EVENT_SHIELD_BROKEN watcher
// + 自查那笔余额归零（⚠️ 事件不分盾/罩，靠 source 锚区分——v1 口径）。
EffectResult effect_1809_hood_break_fear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    const int pct = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).hood_bank_.add(ShieldBank::kSoulShieldPriority, amount,
                                     /*source_id=*/1809, /*duration=*/0);
    if (!is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    const int watch_key = 990180901 + owner;
    if (ctx->plugin_storage.count(watch_key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[watch_key] = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [pct, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target < 0 || ev.target > 1 || !g_core || !g_core->apply_anomaly) {
                return;
            }
            const int self = ev.target;
            if (c->getPet(self).hood_bank_.quantity_of(1809) > 0) {
                return;   // 我那笔护罩还在（破的是别的盾/罩）
            }
            if (!roll_skill_percent(*c, 1809, 0, self, 1 - self, pct)) {
                return;
            }
            g_core->apply_anomaly(c, 1 - self, anomaly, random_anomaly_duration(), self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 793 若造成的伤害低于{0}，则下{1}回合每回合造成{2}点固定伤害 ──
// 时点 = AFTER_ACTION（读 final）；后续每回合固伤 = ROUND_START 回合类条目（duration=rounds）。
EffectResult effect_793_low_damage_dot(BattleContext* ctx, const EffectArgs& args);
EffectResult effect_793_tick(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_793_low_damage_dot(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int rounds = args.int_args[3] > 0 ? args.int_args[3] : 1;
    const int amount = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // 伤害达标 → 不触发
    }
    Effect wrapper(793, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target, amount}), &effect_793_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 793;
    ce->scope_ = EffectScope::ON_STAGE;   // "下N回合"锚在施放方在场
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    return EffectResult::kOk;
}

EffectResult effect_793_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 1070 对手处于能力下降状态时自身先制+1 ──
// 时点 = MOVE_RIGHT（先手权判定时读对手等级；每回合重判）。
EffectResult effect_1070_priority_vs_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(target, stat) < 0) {
            ctx->ws.preemptive_level[owner] += 1;
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── 882 自身每处于一种能力提升状态，此技能威力提升{0} ──
// **变威力**：写 skill_power_view（自动打 rewritten 标记 → 引擎第二次结算；0 点也算变威力）。
EffectResult effect_882_power_per_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int per = args.int_args[2];
    if (owner < 0 || owner > 1 || per <= 0) {
        return EffectResult::kOk;
    }
    int boosts = 0;
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(owner, stat) > 0) {
            ++boosts;
        }
    }
    if (boosts > 0) {
        ctx->ws.skill_power_view[owner] += per * boosts;
    }
    return EffectResult::kOk;
}

// ── 1209 消除双方能力上升、下降状态，消除成功则附加消除总段数乘以{0}的固定伤害 ──
// 上升走 clear_stat_boosts（查 STAT_CLEAR）、下降走 clear_stat_drops（不查免疫）。
EffectResult effect_1209_clear_both_sides(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_seg = args.int_args[2] > 0 ? args.int_args[2] : 50;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->clear_stat_boosts || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    int segs = 0;
    segs += g_core->clear_stat_boosts(ctx, target) + g_core->clear_stat_drops(ctx, target);
    segs += g_core->clear_stat_boosts(ctx, owner) + g_core->clear_stat_drops(ctx, owner);
    if (segs > 0 && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, segs * per_seg, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 1613 自身不处于能力提升状态则吸取对手{0}点体力，若先出手则额外吸取{1}点 ──
// 引擎无"点数吸血"原语 → 对手 fixed_damage + 自己 heal_amount 等量近似（v1）。
EffectResult effect_1613_no_boost_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    int amount = args.int_args[2];
    const int extra = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(owner, stat) > 0) {
            return EffectResult::kOk;   // 处于提升 → 不触发
        }
    }
    if (!ctx->is_second_mover(owner)) {
        amount += extra;   // 先出手额外吸取
    }
    if (g_core && g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, amount);
    }
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

// ── 744 攻击时若自身体力大于对手体力，则附加{0}点固定伤害 ──
EffectResult effect_744_hp_advantage_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp > ctx->getPet(target).hp) {
        g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

// ── 1910 全属性+{0}，自身拥有护罩时强化效果翻倍 ──
EffectResult effect_1910_boost_hood_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    int delta = args.int_args[2];
    if (owner < 0 || owner > 1 || delta <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hood_bank_.total() > 0) {
        delta *= 2;   // 拥有护罩（任意来源）→ 翻倍
    }
    for (int stat = 0; stat <= 4; ++stat) {   // 攻/防/特攻/特防/速
        g_core->stat_change(ctx, owner, stat, delta);
    }
    return EffectResult::kOk;
}

// ── 922 {0}回合内每回合结束后解除并反馈自身能力下降状态 ──
// ROUND_END 回合类条目：清自己下降 → 等量降对手（"反馈"）。
EffectResult effect_922_feedback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    std::vector<std::pair<int, int>> drops;   // {stat, 负等级}
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        const int lv = ctx->ability_level(owner, stat);
        if (lv < 0) {
            drops.emplace_back(stat, lv);
        }
    }
    for (const auto& [stat, lv] : drops) {
        g_core->stat_change(ctx, owner, stat, -lv);          // 解除自身下降
        g_core->stat_drop(ctx, target, stat, -lv, owner);    // 等量反馈给对手
    }
    return EffectResult::kOk;
}

EffectResult effect_922_feedback_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 4;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    Effect wrapper(922, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target}), &effect_922_feedback_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_END, owner, rounds, ctx->roundCount);
    ce->source_id_ = 922;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_END, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 157 {0}回合内若受到攻击，对手防御/特防/命中各-1 ──
// 窗口：ROUND_START 回合类条目（可断）；受击检测：EVENT_HIT watcher（一次注册，
// 窗口有效性由条目 epoch 保证——条目被断/过期后 watcher 不再匹配，用存储窗口自查）。
EffectResult effect_157_retaliate_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 3;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // ⚠️ 键位必须按 owner*2 错开：连续序号 + owner 会跨界碰撞（002+0 == 001+1），
    //   守卫布尔会落进对面读键位（fuzz seed 446 同款事故，452 修复先例）。
    ctx->plugin_storage[990157001 + owner * 2] = rounds;   // 窗口回合数（watcher 自查用）
    ctx->plugin_storage[990157001 + owner * 2 + 1] = ctx->roundCount;
    const int watch_key = 990157005 + owner;
    if (!ctx->plugin_storage.count(watch_key)) {
        ctx->plugin_storage[watch_key] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [target](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor < 0 || ev.actor > 1 || !g_core || !g_core->stat_drop) {
                    return;
                }
                const int owner = ev.target;   // 被打的一方
                auto rd = c->plugin_storage.find(990157001 + owner * 2);
                auto st = c->plugin_storage.find(990157001 + owner * 2 + 1);
                if (rd == c->plugin_storage.end() || st == c->plugin_storage.end()) {
                    return;
                }
                const int start = any_int_or(st->second);
                const int rounds = any_int_or(rd->second);
                if (c->roundCount < start || c->roundCount >= start + rounds) {
                    return;   // 窗口已过
                }
                g_core->stat_drop(c, 1 - owner, /*stat=*/1 /*防御*/, 1, owner);
                g_core->stat_drop(c, 1 - owner, /*stat=*/3 /*特防*/, 1, owner);
                g_core->stat_drop(c, 1 - owner, /*stat=*/5 /*命中*/, 1, owner);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 717 {0}回合内每回合使用技能吸取对手最大体力的1/{1}…低于最大体力1/{2}恢复翻倍 ──
// ROUND_START 回合类条目；"使用技能"门 = 本回合 roundChoice 选了技能（ROUND_START 时
// 选择已写定——handle_BattleRoundStart 保留 roundChoice）。
EffectResult effect_717_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2] > 0 ? args.int_args[2] : 3;
    const int low_denom = args.int_args[3] > 0 ? args.int_args[3] : 2;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(ctx, owner) && ctx->roundChoice[owner][0] != 1) {
        return EffectResult::kOk;   // 本回合没使用技能 → 不吸
    }
    const int max_hp = max_hp_of(ctx, target);
    int amount = max_hp / denom;
    const int self_max = max_hp_of(ctx, owner);
    if (ctx->getPet(owner).hp * low_denom < self_max) {
        amount *= 2;   // 自身体力低于最大体力的 1/{2} → 翻倍
    }
    if (amount > 0 && g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, amount);
    }
    if (amount > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

EffectResult effect_717_drain_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 4;
    const int denom = args.int_args[3] > 0 ? args.int_args[3] : 3;
    const int low_denom = args.int_args[4] > 0 ? args.int_args[4] : 2;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    Effect wrapper(717, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target, denom, low_denom}),
                   &effect_717_drain_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 717;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 602 {0}回合内若自身使用攻击技能则对手特防-1、速度-1 ──
// （官方模板 {1}/{2}/{3} 为可变槽，莫塔里安禁缚悼文取值 = 自身使用攻击技能→对手特防/速度-1）
EffectResult effect_602_drop_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // 仅"使用攻击技能"才触发
    }
    g_core->stat_drop(ctx, target, /*stat=*/3 /*特防*/, 1, owner);
    g_core->stat_drop(ctx, target, /*stat=*/4 /*速度*/, 1, owner);
    return EffectResult::kOk;
}

EffectResult effect_602_attack_drop_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 3;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    Effect wrapper(602, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target}), &effect_602_drop_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 602;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 1077 {0}回合内对手使用攻击技能后使对手{1}，未触发则对手下{2}回合属性技能命中效果失效 ──
// ROUND_START 回合类窗口 + EVENT_HIT watcher（对手出手命中后判）。
EffectResult effect_1077_punish_payload(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int penalty_rounds = args.int_args[3] > 0 ? args.int_args[3] : 2;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 施加异常（现代通道）。
    bool landed = false;
    if (g_core->apply_anomaly && is_valid_abnormal_status_id(anomaly)) {
        const ApplyAnomalyResult r = g_core->apply_anomaly(
            ctx, target, anomaly, random_anomaly_duration(), owner);
        landed = r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
              || r == ApplyAnomalyResult::DURATION_EXTENDED
              || r == ApplyAnomalyResult::CONVERTED;
    }
    if (!landed) {
        // 未触发 → 对手下 N 回合属性技能命中效果失效（封属族票）。
        const int start = ctx->round_effect_start_round(owner, penalty_rounds,
                                                       EffectWindowKind::InRounds);
        ctx->rule_center_.grant_skill_ban(owner, /*source_effect_id=*/1077, target,
                                          penalty_rounds, start, EffectScope::ON_STAGE);
    }
    return EffectResult::kOk;
}

EffectResult effect_1077_attack_punish(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 3;
    const int anomaly = args.int_args[3];
    const int penalty_rounds = args.int_args[4] > 0 ? args.int_args[4] : 2;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 窗口存在性凭证（回合类条目，可被断回合清）+ 每次受击时由 watcher 判活。
    Effect wrapper(1077, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target, anomaly, penalty_rounds}),
                   &effect_1077_punish_payload);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 1077;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    // ⚠️ 窗口有效性：由上面的回合类条目承载（被断/过期后不再有效）——受击触发点挂
    //    EVENT_HIT 一次性 watcher，回调内查上面条目是否仍活着（epoch + 回合窗口）。
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）────────────────
// ⚠️ 531 / 793 / 1070 需要非默认注册时点，已在 skills.cpp 的 effect_register_state 补 case
//    （531/793 → AFTER_ACTION 读管线后 final；1070 → MOVE_RIGHT 先手权判定）。

    // 第五技能·禁命永囚劫（36791）
    SKILL_EFFECT(951, effect_951_boost_reversal_immunity);   // 反转对手提升→成功免异常
    SKILL_EFFECT(1809, effect_1809_hood_break_fear);         // 200护罩+消失时害怕
    SKILL_EFFECT(1478, effect_1478_switch_lock);             // 下N回合锁切（挂对手）
    SKILL_EFFECT(793, effect_793_low_damage_dot);            // 低伤→下N回合每回合固伤

    // 黯狱责罚（先三 36786）
    SKILL_EFFECT(942, effect_942_break_and_drain);           // 断回合成功吸1/3
    SKILL_EFFECT(531, effect_531_low_damage_fear);           // 低伤概率害怕（官方公式）

    // 尘沙结天咒（36790）
    SKILL_EFFECT(1070, effect_1070_priority_vs_drop);        // 对手处于下降→先制+1
    SKILL_EFFECT(882, effect_882_power_per_boost);           // 每处于一种提升威力+30（变威力）
    SKILL_EFFECT(1209, effect_1209_clear_both_sides);        // 消除双方升降+段数×50固伤

    // 幻砂迷魄（36789）/ 漫空埃葬（36788）/ 彼方断罪（36787）
    SKILL_EFFECT(1077, effect_1077_attack_punish);           // 受攻击则害怕，未触发→封属
    SKILL_EFFECT(1613, effect_1613_no_boost_drain);          // 不处于提升则吸血（先手额外）
    SKILL_EFFECT(744, effect_744_hp_advantage_fixed);        // 体力大于对手附加固伤

    // 伏灵罪印（28406）/ 禁缚悼文（28407）
    SKILL_EFFECT(922, effect_922_feedback_drops);            // 每回合结束解除并反馈下降
    SKILL_EFFECT(157, effect_157_retaliate_drop);            // 受攻击→对手防/特防/命中-1
    SKILL_EFFECT(1910, effect_1910_boost_hood_double);       // 全属性+1护罩时翻倍
    SKILL_EFFECT(717, effect_717_drain_window);              // 吸1/3低于1/2翻倍
    SKILL_EFFECT(602, effect_602_attack_drop_window);        // 用攻击技能→对手特防/速度-1
