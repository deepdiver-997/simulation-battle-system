// D17 · 极战·阿尔斯兰 4327 第五技「王·天决神威 35653」缺失效果 1734、840
//（夜跑 night-2，2026-09-28）
//
// 35653 链 = [760, 1734, 933, 840]、args [100, 6, 100, 1, 3]：760=[]（已接两处）、
//   1734=[100, 6, 100]（本文件）、933=[1]（lingchao_skills.cpp 已接）、840=[3]（本文件，
//   ★ 与 D16 镇世·乔特鲁德共享——后做方直接复用本实现，勿重复注册）。
// 4399 图鉴（王·天决神威）："必中；攻击时造成的伤害不会出现微弱；消除对手回合类效果，
//   消除成功则100%令对手害怕，若对手不处于回合类效果则造成的攻击伤害提升100%；
//   未击败对手则自身下次攻击技能必定[致命一击]（933，已接）"。
//
// 1734 "消除对手回合类效果，消除成功则{0}%令对手{1}，若对手不处于回合类效果则
//   造成的攻击伤害提升{2}%"：
//   - "不处于回合类效果"以消除前快照判（has_round_type，1032/1959 同款）→ 两分支互斥：
//     有回合类 → break_round_effects + roll_percent 掷{0}% 施加{1}（{1}=6 害怕，
//     apply_anomaly 申报 + random_anomaly_duration，1959 同款）；无回合类 → 增伤
//     ws.damage_add_pct += {2}（通用加法通道，1178/776 同款）。
// 840 "击败对手则对方下只精灵出战时己方在场精灵吸取其最大体力的1/{0}"（[3] → 1/3）：
//   - 两段式 = zhakes 2017 / batch_b 67 同款：① BATTLE_OPPONENT_DEFEAT_STATUS 探针
//     （本回合一次）按"对手在场已倒 + last_damage_actor 归因本方"判击败 → 置 arm；
//   - ② EVENT_ENTER_STAGE watcher（TEAM 作用域活过自身切换；幂等挂一次）：
//     对方下一只登场（ev.actor==对方）且 arm 在 → 消费 arm，吸取其 max_hp/denom——
//     deal_pink_damage(PERCENT_VALUE) 打新登场的它 + heal_amount 等量回**己方当前在场**
//     （"己方在场精灵"= 交换后场也不改归属，谁在场谁吸）。
//
// 场景回归：test/scenario/scenario_330_arslan_4327_1734_840.cpp。

#include "common.h"

namespace {

constexpr int k840ArmKeyBase = 990184010;    // 840 击败 arm 标记（owner 侧）
constexpr int k840WatchKeyBase = 990184020;  // 840 登场监听已挂旗标（owner 侧）

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 击败判定（batch_b_defeated_foe_slot 同款）：对手在场已倒且归因本方。
int defeated_foe_slot(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return -1;
    }
    const int foe = 1 - owner;
    const int foe_slot = ctx->on_stage[foe];
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp > 0) {
        return -1;
    }
    if (ctx->last_damage_actor[foe][foe_slot] != owner) {
        return -1;
    }
    return foe_slot;
}

// ── 1734 —— 消回合成功{0}%令对手{1}；对手无回合类效果则增伤{2}% ──
EffectResult effect_1734_break_fear_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int status = args.int_args[3];
    const int amp = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const bool had_round = ctx->rule_center_.has_round_type(target);
    if (!had_round) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);   // 通用加法通道
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    if (roll_percent(ctx->rng(), chance) && g_core->apply_anomaly) {
        (void)anomaly_landed(g_core->apply_anomaly(ctx, target, status,
                                                   random_anomaly_duration(), owner));
    }
    return EffectResult::kOk;
}

// ── 840 的击败探针：本回合本方击败了对手 → 置 arm（本回合一次）──
EffectResult effect_840_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k840ArmKeyBase + owner] = 1;
    return EffectResult::kOk;
}

// ── 840 的登场 watcher：对方下只出战 → 己方在场吸取其 max_hp/denom ──
EffectResult effect_840_drain_on_entry_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    // 840 切片 [3]：int_args = [owner, target, 3] —— denom 在 [2]（args_num=1 只有一个参数）。
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    // ① 击败探针（本回合一次性）
    Effect e(840, 0, owner, 1, EffectArgs(std::vector<int>{owner, 1 - owner}),
             &effect_840_kill_probe);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                 owner, /*duration=*/1, ctx->roundCount);
    ce->source_id_ = 840;
    ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, owner, std::move(ce));
    // ② 登场 watcher（幂等注册；arm 后对方下一只登场精灵挨吸）
    if (!ctx->plugin_storage.count(k840WatchKeyBase + owner)) {
        if (!g_core->deal_pink_damage || !g_core->heal_amount) {
            return EffectResult::kOk;
        }
        ctx->plugin_storage.emplace(k840WatchKeyBase + owner, 1);
        const int opp = 1 - owner;
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, opp, denom](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != opp) {
                    return;   // 只认"对方下只出战"的登场
                }
                const int opp = 1 - owner;
                auto it = c->plugin_storage.find(k840ArmKeyBase + owner);
                if (it == c->plugin_storage.end()) {
                    return;
                }
                const int* armed = std::any_cast<int>(&it->second);
                if (!armed || *armed == 0) {
                    return;
                }
                c->plugin_storage[k840ArmKeyBase + owner] = 0;   // "下只出战"一次
                if (g_core && g_core->deal_pink_damage && g_core->heal_amount) {
                    const int slot = c->on_stage[opp];
                    const int max_hp =
                        c->seerRobot[opp].elfPets[slot].numericalBase[NumericalPropertyIndex::HP];
                    const int drain = max_hp / denom;
                    if (drain <= 0) {
                        return;
                    }
                    g_core->deal_pink_damage(c, opp, drain, DamageKind::PERCENT_VALUE, owner);
                    // 吸取等量回"己方在场精灵"（当前在场者，谁在场谁吸）
                    g_core->heal_amount(c, owner, drain);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1734, effect_1734_break_fear_amp);
SKILL_EFFECT(840, effect_840_drain_on_entry_tick);
