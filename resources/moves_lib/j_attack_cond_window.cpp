// J 组 J5 · 攻击技条件窗族 —— 10 宏 / 99 载体（2026-10-01，worktree ../agents/j5）
//
// 家族语义（effect_info 模板全族同构）："{0}回合内若对手使用攻击技能则<X>"。
// 条件窗实现 = **EVENT_SKILL_USED watcher**（工单点名的 663/800 范式终态；1255
// per-round watcher 同款注册面：duration={0} + start=round_effect_start_round 边界）。
// 判据 = ev.actor==对手 && !ev.used_attribute（"攻击技能"=物/特；EVENT_SKILL_USED
// 在 Skills::execute 起点发、miss/被盔也算"使用"→ 1269 的"命中前"天然成立）。
// ⚠️ 不用 AFTER_ACTION 双桶 tick：execute_registered_actions 只跑**出手侧**的桶，
// 先手方挂的窗在后手桶里永远不执行（本轮实测踩坑，663 的双桶同 owner 写法同病）。
//
// 10 宏与触发动作 X（args_num = side_effect.arg_count 全一致）：
//   764  [2] X=降低对手最大体力1/{1} —— reduce_max_hp_flat，削减量=ceil(max/{1})
//            （官方取整"削减向上"；floor=1 永不消逝，原语内建）
//   1151 [2] X=自身全属性+{1} —— all_stats_up（六槽含命中，自升走 stat_change）
//   614  [2] X=对手{1} —— apply_anomaly 直加（无概率子句，{1}=异常码）
//   559  [1] X=随机附加烧伤/冻伤/中毒/麻痹/害怕/睡眠之一 —— attach_random_anomalies
//            六池 100%（池=模板点名的 6 种：2/5/1/0/6/8）
//   772  [2] X={1}%随机附加同款六池之一 —— 同上，概率进门（原语闸门）
//   1228 [2] X=随机附加{1}种异常（全池），未触发则消除对手回合类效果 ——
//            landed<requested 即"未触发"（部分落地也算缺额，记档）→ break_round_effects
//   1269 [2] X=命中前令自身吸取对手最大体力1/{1} —— drain_hp(actor=self, denom={1})，
//            SKILL_USED 时点=使用即吸、先于命中判定（"命中前"字面）
//   1585 [2] X=自身下{1}回合必定暴击 —— 触发时 register_next_rounds_effect 开
//            NextRounds 必暴窗（[R+1, R+{1}]，e_must_crit 58 同款 ROUND_START tick 重写
//            ws.must_crit；重复触发重复开窗，写 true 幂等无害）
//   1833 [2] X=（对手）使用攻击技能后的下{1}回合攻击技能无法造成伤害且命中效果失效 ——
//            grant_attack_nullify(suppress=对手, rounds={1}, register_round=R+1)：
//            票窗=[register, register+rounds) → 传 R+1 恰为"下{1}回合"；
//            重复触发=覆盖键刷新（窗口终点=最近一次触发后推 {1}，记档）
//   1884 [2] X=令对手所有技能PP-{1} —— pp_reduce（清 PP 必走原语）
//
// 动工前置三查（2026-10-01）：①内核 grep 10 id 无硬编码 ②custom_effect_seed.sql 零命中
// ③moves_lib 无既有注册——真缺口；载体数 99 与工单一致，纯载体 764/559/772/1585。
// 先例：1255（watcher 窗注册面+边界）、663/800（条件语义）、e_must_crit（NextRounds
// 必暴窗）、613（grant_attack_nullify 用法）、effect_batch8 570（窗口票）。
// 场景回归：test/scenario/scenario_910_attack_cond_window.cpp（910-914 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <functional>
#include <vector>

namespace {

// 必暴窗 tick（1585 专用，e_must_crit 58 同款）：NextRounds 窗每回合重写 ws.must_crit。
EffectResult j5_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner >= 0 && owner <= 1) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

// 条件窗通用注册：EVENT_SKILL_USED watcher，窗口 [start, start+rounds)（InRounds），
// 触发条件 = 对手使用**攻击技能**（属性技不触发）。act = 该宏的触发动作。
void j5_arm(BattleContext* ctx, int owner, int rounds,
            std::function<void(BattleContext*)> act) {
    if (!ctx || owner < 0 || owner > 1 || rounds <= 0 || !act) {
        return;
    }
    const int opp = 1 - owner;
    const int start =
        ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp, start, rounds, act = std::move(act)](BattleContext* c,
                                                          const BattleEvent& ev) {
            if (!c || ev.actor != opp || ev.used_attribute) {
                return;   // 只认对手的攻击技能（miss/被盔也算使用）
            }
            if (c->roundCount < start || c->roundCount >= start + rounds) {
                return;   // 窗口外
            }
            act(c);
        },
        WatcherScope::TEAM);
}

// 559/772 六池：模板点名序 烧伤、冻伤、中毒、麻痹、害怕、睡眠。
std::vector<int> j5_pool6() {
    return {static_cast<int>(AbnormalStatusId::Burn),
            static_cast<int>(AbnormalStatusId::Frostbite),
            static_cast<int>(AbnormalStatusId::Poison),
            static_cast<int>(AbnormalStatusId::Paralysis),
            static_cast<int>(AbnormalStatusId::Fear),
            static_cast<int>(AbnormalStatusId::Sleep)};
}

}  // namespace

// ════════════════════════ 10 宏 ════════════════════════

// ── 764 —— {0}回合内若对手使用攻击技能降低对手最大体力的1/{1} ──
EffectResult effect_764_maxhp_cut_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 764, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 764, args,
                     "764 布局=[owner,target,回合,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 764, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp, denom](BattleContext* c) {
        if (!g_core->reduce_max_hp_flat || denom <= 0 || c->getPet(opp).hp <= 0) {
            return;
        }
        const int cut = (max_hp_of(c, opp) + denom - 1) / denom;   // 削减向上取整
        if (cut > 0) {
            (void)g_core->reduce_max_hp_flat(c, opp, c->on_stage[opp], cut, /*floor=*/1);
        }
    });
    return EffectResult::kOk;
}

// ── 1151 —— {0}回合内若对手使用攻击技能则自身全属性+{1} ──
EffectResult effect_1151_all_boost_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1151, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1151, args,
                     "1151 布局=[owner,target,回合,级数]");
    const int owner = args.int_args[0];
    const int levels = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1151, args, "需要 owner ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, levels](BattleContext* c) {
        if (levels > 0) {
            all_stats_up(c, owner, levels);
        }
    });
    return EffectResult::kOk;
}

// ── 614 —— {0}回合内若对手使用攻击技能则对手{1} ──
EffectResult effect_614_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 614, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 614, args,
                     "614 布局=[owner,target,回合,异常]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 614, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 614, args, "{1} 须为合法异常码");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp, anomaly](BattleContext* c) {
        if (g_core->apply_anomaly) {
            (void)g_core->apply_anomaly(c, opp, anomaly, random_anomaly_duration(), owner);
        }
    });
    return EffectResult::kOk;
}

// ── 559 —— {0}回合内若对手使用攻击技能则随机附加（六池）一种异常状态 ──
EffectResult effect_559_pool6_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 559, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 559, args,
                     "559 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 559, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp](BattleContext* c) {
        if (g_core->attach_random_anomalies) {
            (void)g_core->attach_random_anomalies(c, opp, j5_pool6(), /*count=*/1,
                                                  /*probability_pct=*/100, owner,
                                                  ChanceSource::Skill);
        }
    });
    return EffectResult::kOk;
}

// ── 772 —— {0}回合内若对手使用攻击技能则{1}%随机附加（六池）一种异常状态 ──
EffectResult effect_772_pool6_chance_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 772, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 772, args,
                     "772 布局=[owner,target,回合,概率]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 772, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp, chance](BattleContext* c) {
        if (g_core->attach_random_anomalies) {
            (void)g_core->attach_random_anomalies(c, opp, j5_pool6(), /*count=*/1, chance,
                                                  owner, ChanceSource::Skill);
        }
    });
    return EffectResult::kOk;
}

// ── 1228 —— {0}回合内若对手使用攻击技能则随机附加{1}种异常状态，未触发则消除对手回合类效果 ──
EffectResult effect_1228_random_or_break_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1228, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1228, args,
                     "1228 布局=[owner,target,回合,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1228, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp, count](BattleContext* c) {
        if (!g_core->attach_random_anomalies || !g_core->break_round_effects || count <= 0) {
            return;
        }
        const RandomAnomalyResult r =
            g_core->attach_random_anomalies(c, opp, anomaly_pool_all(), count,
                                            /*probability_pct=*/100, owner,
                                            ChanceSource::Skill);
        if (r.landed < r.requested) {   // 有缺额（含全被挡）= "未触发" → 消除对手回合类效果
            (void)g_core->break_round_effects(c, opp);
        }
    });
    return EffectResult::kOk;
}

// ── 1269 —— {0}回合内若对手使用攻击技能则命中前令自身吸取对手最大体力的1/{1} ──
EffectResult effect_1269_prehit_drain_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1269, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1269, args,
                     "1269 布局=[owner,target,回合,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1269, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp, denom](BattleContext* c) {
        if (g_core->drain_hp && denom > 0) {
            (void)g_core->drain_hp(c, /*actor=*/owner, /*target=*/opp, denom);
        }
    });
    return EffectResult::kOk;
}

// ── 1585 —— {0}回合内若对手使用攻击技能则自身下{1}回合必定暴击 ──
EffectResult effect_1585_must_crit_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1585, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1585, args,
                     "1585 布局=[owner,target,回合,必暴回合]");
    const int owner = args.int_args[0];
    const int next = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1585, args, "需要 owner ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, next](BattleContext* c) {
        if (next > 0) {
            // NextRounds 必暴窗：[R+1, R+{next}]，ROUND_START tick 每回合重写 ws.must_crit
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, owner, 1585, next,
                                        {owner}, &j5_must_crit_tick);
        }
    });
    return EffectResult::kOk;
}

// ── 1833 —— {0}回合内若对手使用攻击技能，则使用攻击技能后的下{1}回合攻击技能无法造成伤害且命中效果失效 ──
EffectResult effect_1833_attack_nullify_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1833, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1833, args,
                     "1833 布局=[owner,target,回合,压制回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int next = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1833, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [owner, opp, next](BattleContext* c) {
        if (next <= 0) {
            return;
        }
        // 票窗=[register, register+rounds) → register=R+1 恰为"下{next}回合"。
        // bearer=owner + 断回合 epoch 锚（613 同款：我方被断回合则压制随之失效）。
        (void)c->rule_center_.grant_attack_nullify(
            /*bearer=*/owner, /*source_effect_id=*/1833, /*suppress=*/opp,
            /*rounds=*/next, /*register_round=*/c->roundCount + 1, EffectScope::ON_STAGE,
            c->round_effect_valid_id[owner]);
    });
    return EffectResult::kOk;
}

// ── 1884 —— {0}回合内若对手使用攻击技能则令对手所有技能PP值-{1} ──
EffectResult effect_1884_pp_down_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1884, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1884, args,
                     "1884 布局=[owner,target,回合,点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1884, args,
                     "需要 owner/target ∈ {0,1}");
    j5_arm(ctx, owner, args.int_args[2], [opp, points](BattleContext* c) {
        if (g_core->pp_reduce && points > 0) {
            (void)g_core->pp_reduce(c, opp, points);
        }
    });
    return EffectResult::kOk;
}

SKILL_EFFECT(764, effect_764_maxhp_cut_window);
SKILL_EFFECT(1151, effect_1151_all_boost_window);
SKILL_EFFECT(614, effect_614_anomaly_window);
SKILL_EFFECT(559, effect_559_pool6_window);
SKILL_EFFECT(772, effect_772_pool6_chance_window);
SKILL_EFFECT(1228, effect_1228_random_or_break_window);
SKILL_EFFECT(1269, effect_1269_prehit_drain_window);
SKILL_EFFECT(1585, effect_1585_must_crit_window);
SKILL_EFFECT(1833, effect_1833_attack_nullify_window);
SKILL_EFFECT(1884, effect_1884_pp_down_window);
