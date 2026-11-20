#include <utils/sbs_rand.h>
// J 组 J8 · 属性技条件窗族 —— 9 宏/73 载体（2026-10-01，worktree ../agents/j8）
//
// 家族形状（H6 469/152/526 直系）："{0}回合内若对手使用属性技能则X" —— EVENT_SKILL_USED
// watcher（skills.cpp 发射点"使用即触发"，miss/被封属也算使用）+ used_attribute 判据，
// 窗口 = watcher duration={0}（InRounds，469 同款）。本批 9 宏按后缀子句分接：
//
//   163×15  随机 1/6 异常     → 451 同款本地选种（烧伤2/冻伤5/中毒1/麻痹0/害怕6/睡眠8）+
//                             apply_anomaly_checked(chance=100) 申报
//   1100×3  随机 1/6，未触发则下{1}回合伤害提高{2}% → landed 判定（REFLECTED 记未触发，
//           J1 1370 同款保守档）+ NextRounds ROUND_START tick 写 ws.damage_add_pct（789 同款）
//   1102×11 {1}异常，未触发则减对手 1/{2} 最大体力 → reduce_max_hp_pct（d5_D3 同款：
//           slot=on_stage[user]，pct=100/{2}，floor=1 守消逝不变量）
//   1348×12 随机{1}种异常，未触发则消除回合类 → attach_random_anomalies（1367 同款，
//           landed==0 保守补偿）+ break_round_effects 纯消除
//   1391×10 {1}异常，未触发则消除回合类 → 同上单异常版。⚠️ 模板"造成伤害前令对手{1}":
//           属性技本无伤害阶段（速查 §五），"造成伤害前"按使用时点落地即天然满足，记档
//   1637×7  使用属性技能后的下{1}回合其属性技命中效果失效 → grant_hit_invalid 直通
//           （rule_center 票，跨回合稳定；count={1}≈回合数次近似，738 同款偏差记档：
//           {1} 回合内多次属性技会多耗）
//   1717×7  使用属性技能后的下{1}回合攻击伤害-{2}点 → REDUCE_FLAT 管线回调（766 同款
//           attackerId 判定），回合界 [r0+1, r0+{1}]（NextRounds 语义，lambda 内判）
//   1266×7  使用属性技能后令对手下{1}次攻击技无效 → seal_skill 次数盔（1293 同款参数：
//           count={1}, duration=0；无后续子句 → consumed_when_pierced=false）
//   1890×1  自身下{1}次固伤/百分伤-{2}% → register_pink_damage_effect(REDUCE_EXTRA) +
//           shared_ptr 计次（2228 同族直系；选型记档：红伤管线的 REDUCE_PCT 回调读
//           resolvedDamage 快照，而粉伤结算写 resolvedPink——红伤回调拦不到粉伤，实测）
//
// 载体（场景用，全链伴生逐条核对；"u"=未注册文本解析件——2172 教训：解析件可能无视
// 条件直火，场景以对手异常免疫/行动设计隔离，见 scenario_925 头注）：
//   163 → 奇幻孢子 21454 [3]（全净）      1100 → 极限壁垒 25296 [3,2,100]（716R/191R）
//   1102 → 销神磁场 28793 [3,0,3]（全净）  1348 → 衰竭骷骨 28298 [3,1]（全净）
//   1391 → 火光烛天 26539 [3,2]（1058u/1239R）1637 → 辉啼讴歌 28488 [3,1]（全净）
//   1717 → 盈盈碧叶 37182 [3,2,200]（1585u：条件含"己方用攻击技"，场景用属性 filler 避开）
//   1266 → 此身为界 25959 [3,1]（191R/1151u：1151 条件含"己方用攻击技"，同上避开）
//   1890 → 人律再书 28166 [4,1,100]（1888u/1889u/1884u：解析产物指向对手侧，不扰断言）
//
// 前置三查（2026-10-01）：①内核 grep 无本批 9 id 硬编码 ②effect_meta 无该批 id 分类
// ③moves_lib 无既有注册（469/152/526/65 同族已实现但 id 不同）——真缺口。
//
// 场景回归：test/scenario/scenario_925_attr_cond_window.cpp（段 925-929）。

#include "common.h"

#include <memory>

namespace {

constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL

// "未触发"判定（J1 j_purge_round 同款保守档）：落地=SUCCESS/替换/延长/转化；
// 免疫/被挡/抗性/弹回/掷点未中均记未触发。
inline bool anomaly_landed_shared(ApplyAnomalyResult r) {
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

// 163/1100 的随机池：官方模板字面六种（烧伤、冻伤、中毒、麻痹、害怕、睡眠）。
const int kPool6[6] = {2, 5, 1, 0, 6, 8};

// 共享：注册"对手使用属性技能"事件窗（469 同款：使用即触发、TEAM 跨回合）。
// fn 回调约定：args=[self, ...]，回调内部自判 ev.target/used_attribute/actor。
template <typename Fn>
inline void watch_attr_use(BattleContext* ctx, int self, int rounds, std::vector<int> payload,
                           Fn fn) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, self, ctx->roundCount, rounds, /*once=*/false,
        [self, payload, fn](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core) {
                return;
            }
            if (ev.target != self || !ev.used_attribute) {
                return;   // 只认对手使用属性技（469 同款判据）
            }
            const int user = ev.actor;
            if (user < 0 || user > 1) {
                return;
            }
            std::vector<int> ints{self, user};
            ints.insert(ints.end(), payload.begin(), payload.end());
            EffectArgs args;
            args.int_args = ints.data();
            args.int_count = static_cast<int>(ints.size());
            fn(c, args);
        },
        WatcherScope::TEAM);   // 跨回合窗口必须 TEAM（469/1219 同款选型）
}

// ── 163 —— {0}回合内若对手使用属性技能则随机附加六选一异常 ──
EffectResult effect_163_attr_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 163, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 163, args,
                     "163 布局=[owner,target,回合]（int_count>=3）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 163, args,
                     "需要侧位合法且回合>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {}, [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int user = a.int_args[1];
        const int pick = kPool6[sbs_rand() % 6];
        (void)g_core->apply_anomaly_checked(c, user, pick, random_anomaly_duration(), self2,
                                            100, ChanceSource::Skill);
    });
    return EffectResult::kOk;
}

// 1100 tick 前置声明（watcher lambda 内引用）
EffectResult effect_1100_amp_tick(BattleContext* ctx, const EffectArgs& args);

// ── 1100 —— 随机六选一，未触发则下{1}回合造成的伤害提高{2}% ──
EffectResult effect_1100_attr_random_or_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1100, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1100, args,
                     "1100 布局=[owner,target,回合,增幅回合,增幅%]（int_count>=5）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int boost_rounds = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && boost_rounds > 0 && pct > 0,
                     1100, args, "需要侧位合法且回合/增幅>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {boost_rounds, pct},
                   [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int user = a.int_args[1];
        const int br = a.int_args[2];
        const int p = a.int_args[3];
        const int pick = kPool6[sbs_rand() % 6];
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, user, pick, random_anomaly_duration(), self2, 100, ChanceSource::Skill);
        if (anomaly_landed_shared(r)) {
            return;   // 触发：无补偿
        }
        // 未触发：下{1}回合自身伤害提高{2}%（789 同款 NextRounds + ROUND_START tick）
        const std::vector<int> tick{self2, 0, p};
        register_next_rounds_effect(c, State::BATTLE_ROUND_START, self2, 1100, br, tick,
                                    &effect_1100_amp_tick);
    });
    return EffectResult::kOk;
}

// 1100 tick：每回合开始把 ws.damage_add_pct[self] 累加 pct（ws 每回合 reset → 每回合重写）。
EffectResult effect_1100_amp_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[self] += static_cast<float>(pct);
    return EffectResult::kOk;
}

// ── 1102 —— {1}异常，未触发则减少对手 1/{2} 最大体力 ──
EffectResult effect_1102_attr_anomaly_or_shrink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1102, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1102, args,
                     "1102 布局=[owner,target,回合,异常码,分母]（int_count>=5）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && denom > 0, 1102, args,
                     "需要侧位合法且回合/分母>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1102, args, "异常码非法");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->reduce_max_hp_pct) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {anomaly, denom},
                   [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int user = a.int_args[1];
        const int anomaly2 = a.int_args[2];
        const int denom2 = a.int_args[3];
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, user, anomaly2, random_anomaly_duration(), self2, 100, ChanceSource::Skill);
        if (anomaly_landed_shared(r)) {
            return;
        }
        // 未触发：减对手 1/{2} 最大体力（d5_D3 同款：slot=on_stage，floor=1）
        const int slot = c->on_stage[user];
        (void)g_core->reduce_max_hp_pct(c, user, slot, 100 / denom2, /*floor=*/1);
    });
    return EffectResult::kOk;
}

// ── 1348 —— 随机附加{1}种异常，未触发则消除对手回合类效果 ──
EffectResult effect_1348_attr_random_or_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1348, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1348, args,
                     "1348 布局=[owner,target,回合,种数]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int kinds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && kinds > 0, 1348, args,
                     "需要侧位合法且回合/种数>0");
    if (!g_core || !g_core->attach_random_anomalies || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {kinds}, [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int user = a.int_args[1];
        const int k = a.int_args[2];
        const RandomAnomalyResult r = g_core->attach_random_anomalies(
            c, user, anomaly_pool_all(), k, /*probability_pct=*/100, self2,
            ChanceSource::Skill);

        if (r.landed > 0) {
            return;   // 触发（1367 同款：landed==0 才补偿）
        }
        (void)g_core->break_round_effects(c, user);   // 纯消除，无后续子句
    });
    return EffectResult::kOk;
}

// ── 1391 —— {1}异常（"造成伤害前"，属性技无伤害阶段→使用时点即满足，文件头记档），
//             未触发则消除对手回合类效果 ──
EffectResult effect_1391_attr_anomaly_or_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1391, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1391, args,
                     "1391 布局=[owner,target,回合,异常码]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1391, args,
                     "需要侧位合法且回合>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1391, args, "异常码非法");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {anomaly}, [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int user = a.int_args[1];
        const int anomaly2 = a.int_args[2];
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, user, anomaly2, random_anomaly_duration(), self2, 100, ChanceSource::Skill);
        if (!anomaly_landed_shared(r)) {
            (void)g_core->break_round_effects(c, user);
        }
    });
    return EffectResult::kOk;
}

// ── 1637 —— 使用属性技能后的下{1}回合其属性技命中效果失效 ──
// rule_center grant_hit_invalid 票（跨回合稳定；选型记档：g_core->hit_effect_invalid
// 落 ws 每回合重置——2390 同坑，R2 实测失效）。count={1} ≈ "下{1}回合"（738 同款次
// 近似，多次属性技多耗）。kEffectsOnly：属性技本无红伤，效果失效即全效失效。
EffectResult effect_1637_attr_hit_invalid(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1637, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1637, args,
                     "1637 布局=[owner,target,回合,失效回合]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int null_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && null_rounds > 0, 1637, args,
                     "需要侧位合法且回合>0");
    if (!g_core) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {null_rounds}, [](BattleContext* c, const EffectArgs& a) {
        const int user = a.int_args[1];
        const int nr = a.int_args[2];
        c->rule_center_.grant_hit_invalid(/*target=*/user, /*source_slot=*/-1,
                                          /*source_effect_id=*/1637,
                                          /*mode=*/static_cast<int>(HitInvalidMode::kEffectsOnly),
                                          /*count=*/nr, /*is_attribute_skill=*/true);
    });
    return EffectResult::kOk;
}

// ── 1717 —— 使用属性技能后的下{1}回合其攻击伤害-{2}点 ──
// REDUCE_FLAT 管线回调（766 同款 attackerId 判定），回合界 [r0+1, r0+{1}]（NextRounds）。
EffectResult effect_1717_attr_flat_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1717, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1717, args,
                     "1717 布局=[owner,target,回合,减伤回合,减伤点]（int_count>=5）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int rd = args.int_args[3];
    const int flat = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && rd > 0 && flat > 0, 1717, args,
                     "需要侧位合法且回合/点数>0");
    if (!g_core) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {rd, flat}, [](BattleContext* c, const EffectArgs& a) {
        const int user = a.int_args[1];
        const int rd2 = a.int_args[2];
        const int flat2 = a.int_args[3];
        const int r0 = c->roundCount;
        c->register_damage_effect(
            DamagePhase::REDUCE_FLAT, a.int_args[0], DamageEffectCategory::MITIGATE,
            [user, flat2, r0, rd2](BattleContext* cc, int) {
                if (!cc || cc->roundCount <= r0 || cc->roundCount > r0 + rd2) {
                    return;   // "下{1}回合"：[r0+1, r0+{1}]
                }
                if (cc->resolvedDamage.attackerId != user || !cc->resolvedDamage.isRed) {
                    return;   // 只减该用户打出的红伤
                }
                cc->resolvedDamage.final =
                    std::max(0, cc->resolvedDamage.final - flat2);
            });
    });
    return EffectResult::kOk;
}

// ── 1266 —— 使用属性技能后令对手下{1}次攻击技无效 ──
// seal_skill 次数盔（1293 同款：count={1}, duration=0；无后续子句 → 不吃被穿消费）。
EffectResult effect_1266_attr_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1266, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1266, args,
                     "1266 布局=[owner,target,回合,次数]（int_count>=4）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int counts = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && counts > 0, 1266, args,
                     "需要侧位合法且回合/次数>0");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {counts}, [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int user = a.int_args[1];
        const int n = a.int_args[2];
        (void)g_core->seal_skill(c, /*source=*/self2, /*target=*/user, /*effect_id=*/1266,
                                 /*attribute=*/false, /*attack=*/true, /*count=*/n,
                                 /*duration_rounds=*/0, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    });
    return EffectResult::kOk;
}

// ── 1890 —— 自身下{1}次受到的固伤/百分伤-{2}% ──
// REDUCE_PCT + shared_ptr 计次（1287 同款）；判据 !isRed && !isTrueDamage（粉伤通道）。
EffectResult effect_1890_attr_pink_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1890, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1890, args,
                     "1890 布局=[owner,target,回合,次数,减伤%]（int_count>=5）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int counts = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && counts > 0 && pct > 0 && pct <= 100,
                     1890, args, "需要侧位合法且回合/次数>0、0<减伤≤100");
    if (!g_core) {
        return EffectResult::kOk;
    }
    watch_attr_use(ctx, self, rounds, {counts, pct}, [](BattleContext* c, const EffectArgs& a) {
        const int self2 = a.int_args[0];
        const int n = a.int_args[2];
        const int p = a.int_args[3];
        auto remaining = std::make_shared<int>(n);
        c->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA, self2,
            [self2, p, remaining](BattleContext* cc, int bucket_owner) {
                if (!cc || bucket_owner != self2 || *remaining <= 0) {
                    return;
                }
                PinkDamageResolved& pk = cc->resolvedPink;
                if (pk.target != self2 || pk.final <= 0) {
                    return;   // 只减自身受到的固伤/百分伤（模板无限定施加方，不查 actor）
                }
                pk.final -= pk.final * p / 100;
                --*remaining;
            });
    });
    return EffectResult::kOk;
}

}  // namespace

// ── 本批注册 ──
SKILL_EFFECT(163, effect_163_attr_random_anomaly)
SKILL_EFFECT(1100, effect_1100_attr_random_or_amp)
SKILL_EFFECT(1102, effect_1102_attr_anomaly_or_shrink)
SKILL_EFFECT(1348, effect_1348_attr_random_or_break)
SKILL_EFFECT(1391, effect_1391_attr_anomaly_or_break)
SKILL_EFFECT(1637, effect_1637_attr_hit_invalid)
SKILL_EFFECT(1717, effect_1717_attr_flat_reduce)
SKILL_EFFECT(1266, effect_1266_attr_seal_attack)
SKILL_EFFECT(1890, effect_1890_attr_pink_reduce)
