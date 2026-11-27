#include <utils/sbs_rand.h>
// 龙神哈莫 3809 —— 召唤龙神双 BUFF：战之龙魂（effect 1452）/ 御之龙魂（effect 1453）
//（2026-09-28；工单外直派任务，场景回归 test/scenario/scenario_171_hamo3809_longhun_1452_1453.cpp）
//
// 官方形态（4399 魂印页「敖」+ effect_info）：龙·守地灵 26595 / 龙·御天神 26596 的
// HIT 分支各带一条「召唤龙神，使自身下{0}回合获得X之龙魂效果」（effect 1452/1453，
// arg_count=1 → [2]）。BUFF 数值只存在于魂印页 BUFF 说明（无官方 effect 模板，按文本手写）：
//   战之龙魂（1452）：所有技能先制+3 且必定命中、攻击造成的伤害随机提升 50%-100%；
//   御之龙魂（1453）：受到的固定伤害和百分比伤害减少 25%、攻击令对手随机 1 个技能 PP 值归零。
//
// ★ 架构（2026-09-28 与用户定案）：**RuleCenter 载体票为真源 + 战斗级读者查票**
//   与咒怨（pet.soulmark_storage 层数真源）同构的印记类外壳，真源换成 RuleCenter
//   回合类票（grant_carrier / has_carrier，见 rule_center.h RuleCategory::CARRIER 注）：
//     · 断回合整组失效——票被 clear_round_type / invalidate_stale 作废后读者全员哑火。
//       这绕开了"管线 epoch 与回合 epoch 分离、断回合不清管线条目"的既存裂缝
//       （battleContext.h pipeline_valid_id 注；effect 525 口径未定区），不动旧口径；
//     · 换宠清——票 scope=ON_STAGE（v1 绑定持有者在场，与 1123/681 窗口族同口径；
//       "下场的哈莫带不带龙魂"官方无实证，实测翻案再放）；
//     · "下{0}回合"起算——grant 传 NextRounds 起点（roundCount+1），has_carrier
//       含起算门（起点前不生效），票与窗口节点（1123/681 同款）同窗同步。
//   读者**不做精灵 ID 判定**：谁持有票谁受益——技能被复制/偷走时新施放者 arm 自己
//   的票即生效（官方实现路径待实测，2026-09-28 记档）。
//
// 数值口径取态（保守档，实测翻案再放）：
//   · "攻击造成的伤害随机提升50%-100%"：每次攻击独立掷（红伤管线 AMP 段内 rand，
//     咒怨 FLOOR"管线内掷点"同形）；通用增伤区（加法，与咒怨 AMP 等叠加）。
//   · "攻击令对手随机1个技能PP值归零"：每回合一次（AFTER_ACTION + used_skill 门，
//     1647 同款保守档）；按次攻击触发（额外行动各算一次）待实测翻案。
//     归零走 pp_zero_slot 原语（actor=攻击方——琼华之庇的"对手来源清零保留"口径
//     天然生效，PP 反应族监听可见）。
#include "common.h"

namespace {

// plugin_storage 接线守卫键（插件自约定段 3809xxxx，已 grep 全库无撞）。
constexpr int kLonghunWiredFlag = 38090001;

// ── AFTER_ACTION 桶的当前行动方（batch_a_skills.cpp 同款）────────────────────
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

// ── 接线（幂等，arm 时调一次）：双龙魂的战斗级读者 ─────────────────────────
// 双方桶各装一条（TEAM 常驻，不受切换作废——载体票在谁身上谁生效）。
void ensure_longhun_wiring(BattleContext* ctx) {
    if (!ctx || ctx->plugin_storage.count(kLonghunWiredFlag)) {
        return;
    }
    ctx->plugin_storage[kLonghunWiredFlag] = true;
    for (int owner = 0; owner < 2; ++owner) {
        // 战之龙魂：攻击造成的伤害随机提升 50%-100%（红伤 AMP 段，每次攻击独立掷；
        // 查票在掷点**之前**——没票不烧 rand，断回合/过期后伤害回归确定值）。
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || d.final <= 0) {
                    return;
                }
                if (!c->rule_center_.has_carrier(bucket_owner, 1452, c->roundCount)) {
                    return;
                }
                const int pct = 50 + c->rng().next() % 51;   // 50~100
                d.final += d.final * pct / 100;
            });
        // 御之龙魂：受到的固定伤害和百分比伤害减少 25%（粉伤 REDUCE_EXTRA 乘法免减区，
        // 承受方桶；固定/百分比两档同段——粉伤管线不分型免减在此阶段）。
        ctx->register_default_pink_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                PinkDamageResolved& p = c->resolvedPink;
                if (p.target != bucket_owner || p.final <= 0) {
                    return;
                }
                if (!c->rule_center_.has_carrier(bucket_owner, 1453, c->roundCount)) {
                    return;
                }
                p.final -= p.final * 25 / 100;
            });
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 1452 —— 召唤龙神，使自身下{0}回合获得战之龙魂效果（args=[owner, target, rounds]）
//═══════════════════════════════════════════════════════════════════════════

// 窗口 tick：先制+3 与必中（MOVE_RIGHT 桶，先手权 memset 之后写 → 本回合生效；
// ws 每回合 reset → 窗口每回合重写，1123/681 同款）。
EffectResult effect_1452_preempt_musthit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[self] += 3;   // 所有技能先制+3
    ctx->ws.must_hit_grant[self] = true;   // 必定命中（"所有技能"——无 681 的攻击技能门）
    return EffectResult::kOk;
}

EffectResult effect_skill_1452(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ensure_longhun_wiring(ctx);
    // 真源票：NextRounds 起点起算（has_carrier 起算门挡住当回合），随断回合/换宠作废。
    ctx->rule_center_.grant_carrier(
        owner, /*source_effect_id=*/1452, rounds,
        ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds),
        EffectScope::ON_STAGE, /*source_valid_id=*/ctx->round_effect_valid_id[owner]);
    // 先制+3 & 必中：NextRounds 窗口节点（自身即可断的回合类，与票同窗同步）。
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1452, rounds,
                                {owner, 1 - owner}, &effect_1452_preempt_musthit_tick);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1453 —— 召唤龙神，使自身下{0}回合获得御之龙魂效果（args=[owner, target, rounds]）
//═══════════════════════════════════════════════════════════════════════════

// 窗口 tick：攻击令对手随机 1 个技能 PP 值归零（自身 AFTER_ACTION，每回合一次；
// used_skill 门——嗑药/被控空过不触发；候选 = pp>0 不重复挑 1 个，1419/1532 同款选法）。
EffectResult effect_1453_pp_zero_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (bucket_acting_side(ctx) != self || !used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (!ctx->rule_center_.has_carrier(self, 1453, ctx->roundCount)) {
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->getPet(target);
    std::vector<int> candidates;
    for (std::size_t i = 0; i < opp.skills.size(); ++i) {
        if (opp.skills[i].pp > 0) {
            candidates.push_back(static_cast<int>(i));
        }
    }
    if (candidates.empty()) {
        return EffectResult::kOk;
    }
    const int pick = candidates[ctx->rng().next() % candidates.size()];
    (void)g_core->pp_zero_slot(ctx, target, pick, /*actor=*/self);
    return EffectResult::kOk;
}

EffectResult effect_skill_1453(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ensure_longhun_wiring(ctx);
    ctx->rule_center_.grant_carrier(
        owner, /*source_effect_id=*/1453, rounds,
        ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds),
        EffectScope::ON_STAGE, /*source_valid_id=*/ctx->round_effect_valid_id[owner]);
    // PP 归零：自身 AFTER_ACTION 窗口节点（双侧注册 + 行动方门，1647 同款形态）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(ctx, st, owner, 1453, rounds, {owner, 1 - owner},
                                    &effect_1453_pp_zero_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）────────────────────────
SKILL_EFFECT(1452, effect_skill_1452)
SKILL_EFFECT(1453, effect_skill_1453)
