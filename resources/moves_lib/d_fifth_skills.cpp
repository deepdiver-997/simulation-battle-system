// D 组 · 第五技批产（2026-09-28，工单：docs_local/docs/05-任务清单/D组启动看板.md D 组表）
//
// 本文件实现 D 组精灵第五技（hide_moves/sp_hide_moves 族）的缺失效果。
// 当前：D1 湮灭之主·咤克斯 4762 —— 绝灭·万物哀鸣 37914
//（moves.side_effect=[2295, 698]、side_effect_arg=[100, 23]：2295 args_num=0 不切片，
//  698 切到 [100, 23]；切片规则见 official_data_repository.cpp build_skill_effects）：
//   2295（args_num=0）"废除此技能，恢复对方精灵的全部体力，消耗此技能的全部PP值并
//         获得等量的魔王咒怨，然后若魔王咒怨层数高于5则秒杀对手且造成的攻击伤害不低
//         于对方精灵体力上限之和"
//   698（2 参 [100,23]）"当回合击败对手时，对方下只精灵出战时{0}%{1}"（=100% 诅咒）
// 4399 图鉴原文（pets_full.jsonl 4762 条目）与 effect_info 模板逐字一致，无截断歧义。
//
// 口径与落点：
// ★ 2295「废除此技能」= `Skills::is_locked = true`（用户 2026-09-28 定稿，同第五技
//   可用性三分层的"选择锁"层——query_selectable→LOCKED，本体不可用→不可选，
//   技能判定流程与无效效果体系.md §7.6）。废除发生在 HIT 分支（效果结算时点）：
//   被盔/威/封属挡下（SKILL_INVALID）不废除——"用了但被无效"只扣 PP 不废技。
// ★ 2295 的 PP 消耗**直写**（skill.pp = 0），不发 EVENT_PP_REDUCED：与引擎自身
//   使用扣费（battleFsm consume_selected_skill_pp）同款——"使用技能消耗 PP"是
//   成本不是效果，走 pp 原语会发事件误触"PP 被效果降低/清零"类反应（莫妮卡
//   琼华之庇族监听的是被清除语义）。SKILL_EFFECT 时点 PP 尚未扣（引擎在
//   AFTER_ACTION 才扣当次 1 点；本效果先清零后，AFTER_ACTION 的扣费 min(0,·)=0）。
// ★ 魔王咒怨复用共享层 include/effects/curse_stack.h（咤 2378 魂印同款：层数存
//   pet.soulmark_storage、ensure_curse_wiring 幂等接线增减伤/FLOOR/免控检测器）。
//   "获得等量"的等量 = 消耗的 PP 点数（语料 idx=225"按当前剩余pp值获得等量"同源）。
//   无限 PP（pp=-1）无"全部"可计 → 不获层（保守档）。
// ★ 秒杀落点 = AFTER_ACTION 一次性节点（495 同款）：SKILL_EFFECT 先于伤害管线，
//   在管线回调里归零是死亡漏斗重入（mortarion 091 教训）；对手已被保底伤害打死则
//   不再补刀（存活门）。"秒杀"走 force_hp_to_zero 统一原语（免秒杀票/短路转化/
//   EVENT_HP_TO_ZERO 全链生效，归因写 last_damage_actor——698 的击败探测靠它）。
// ★ 伤害保底 = FLOOR 管线条目（咒怨魂印 3%/层保底同阶段：减伤后、锁伤前），本回合
//   一次性（roundCount 闭环守卫），把 final 抬到 ≥ 对方精灵体力上限之和——即使
//   秒杀被免票短路，伤害本身也足以致死。
//
// 已知口径取态（保守档，实测翻案再放）：
//   · "对方精灵体力上限之和" = 对方 6 槽**未消逝**精灵的体力上限合计（含已倒地槽）。
//     替代口径"仅存活"只影响保底数值大小（两者都 ≥ 在场宠上限），不改变击杀结论。
//   · 698"击败"归因 = 对手在场宠倒地 + last_damage_actor 为本方（2017 同款三段式；
//     检测节点挂 BATTLE_OPPONENT_DEFEAT_STATUS 官方"击败对手时"时点、当回合一次性）。
//   · 698 诅咒施加走 apply_anomaly_checked（概率申报版：受概率闸门管辖），"下只"
//     = 置位后首个登场的对方精灵，施加无论落地与否都消费（"下只"一次语义）。
//
// 场景回归：test/scenario/scenario_170_zhakes4762_2295_698.cpp。
#include "common.h"

#include <effects/curse_stack.h>

namespace {

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

// ═══════════════════════════════════════════════════════════════════════════
// 2295（绝灭·万物哀鸣 37914，args=[owner, target]，args_num=0）
//═══════════════════════════════════════════════════════════════════════════

// AFTER_ACTION 一次性补刀：保底伤害没能打死（秒杀被免票短路等）→ force_hp_to_zero。
EffectResult effect_2295_kill_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int arm_round = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || arm_round != ctx->roundCount || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    ElfPet& target = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (target.hp <= 0) {
        return EffectResult::kOk;   // 已被保底伤害打死：无需补刀
    }
    (void)g_core->force_hp_to_zero(ctx, opp, owner);
    return EffectResult::kOk;
}

EffectResult effect_2295_abolish_heal_pp_curse(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }

    // ① 废除此技能（选择锁：is_locked → 不可选；§7.6 三分层，用户 2026-09-28 定稿）。
    const int slot = ctx->executing_skill_slot(owner);
    if (slot < 0 || slot >= static_cast<int>(ctx->getPet(owner).skills.size())) {
        return EffectResult::kOk;
    }
    Skills& self_skill = ctx->getPet(owner).skills[static_cast<std::size_t>(slot)];
    self_skill.is_locked = true;

    // ② 恢复对方精灵的全部体力（heal 原语：吃封回血——官方"恢复体力"就是恢复效果）。
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, target, max_hp_of(ctx, target));
    }

    // ③ 消耗此技能的全部 PP 值并获得等量魔王咒怨（直写见文件头★；本时点 PP 未扣）。
    int consumed = 0;
    if (self_skill.pp > 0) {   // pp=-1（无限）无"全部"可计：不消耗不获层
        consumed = self_skill.pp;
        self_skill.pp = 0;
    }
    if (consumed > 0) {
        ensure_curse_wiring(ctx);   // 幂等：首次获得咒怨时把增减伤/FLOOR/免控接线装齐
        add_curse_stacks_slot(ctx, owner, ctx->on_stage[owner], consumed);
    }

    // ④ 层数高于 5 → 秒杀 + 本次攻击伤害保底对方体力上限之和。
    const int stacks = get_curse_stacks_slot(ctx, owner, ctx->on_stage[owner]);
    if (stacks <= 5) {
        return EffectResult::kOk;
    }
    // ④a 伤害保底：FLOOR 阶段一次性条目（本回合闭环；对方未消逝 6 槽上限合计）。
    const int arm_round = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::FLOOR, owner, DamageEffectCategory::AMP,
        [owner, target, arm_round](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->roundCount != arm_round) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final < 0) {
                return;
            }
            int sum = 0;
            for (int i = 0; i < 6; ++i) {
                if (c->is_vanished(target, i)) {
                    continue;   // 消逝已离场，不计入"对方精灵"
                }
                sum += c->seerRobot[target].elfPets[i].numericalBase[NumericalPropertyIndex::HP];
            }
            if (d.final < sum) {
                d.final = sum;
            }
        });
    // ④b 秒杀补刀：AFTER_ACTION 一次性（495 同款，理由见文件头★）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2295, 0, owner, -1, EffectArgs(std::vector<int>{owner, target, arm_round}),
                 &effect_2295_kill_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 2295;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 698 —— 当回合击败对手时，对方下只精灵出战时{0}%{1}
//（绝灭·万物哀鸣 37914 携带 [100, 23] = 100% 诅咒）
//
// 三段式（本库 2017「击败对手则对方下只出战精灵首回合先制效果失效」同款）：
//   ① 击败检测：BATTLE_OPPONENT_DEFEAT_STATUS 一次性节点（官方"击败对手时"时点，
//      execute(-1) 双侧都跑；归因=对手在场宠倒地 + last_damage_actor 为本方——
//      force_hp_to_zero 也写 last_damage_actor，2295 的秒杀补刀同样被覆盖）→ 置 pending。
//   ② 登场监听：EVENT_ENTER_STAGE（perform_switch 统一漏斗，主动中切与死亡换宠都
//      发；drain 时新精灵已完全就位，on_stage 已是登场宠）→ pending 在手即施加
//      {1} 异常（{0}% 概率申报版）并消费 pending、自删监听器。
//   为什么不像 2017 那样等 ROUND_START 消费：2017 写的 ws.priority_nullified 会被
//   回合 reset 冲掉，必须回合开始时再写；本效果落的是异常状态（pet 绑定、不随
//   ws reset），"出战时"直给的时点反而更贴官方字面。
constexpr int k698PendingBase = 69800001;   // plugin_storage +self：击败置位

EffectResult effect_698_defeat_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int foe = 1 - self;
    const int foe_slot = ctx->on_stage[foe];
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp > 0
        || ctx->last_damage_actor[foe][foe_slot] != self) {
        return EffectResult::kOk;   // 本回合没有"本方击败对手"
    }
    ctx->plugin_storage[k698PendingBase + self] = true;
    return EffectResult::kOk;
}

EffectResult effect_698_defeat_next_curse(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (self < 0 || self > 1 || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // ① 击败检测：当回合一次性（击败时点在 SKILL_EFFECT 之后同回合到达）。
    {
        Effect e(698, 0, self, 1, EffectArgs(std::vector<int>{self, args.int_args[1]}),
                 &effect_698_defeat_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                     self, 1, ctx->roundCount);
        ce->source_id_ = 698;
        ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, self, std::move(ce));
    }
    // ② 登场监听：对方任一精灵登场且 pending 在手 → 施加 {1}、消费、自删。
    auto watcher_id = std::make_shared<int>(-1);
    *watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, chance, anomaly, watcher_id](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || ev.actor != 1 - self) {
                return;
            }
            auto it = c->plugin_storage.find(k698PendingBase + self);
            if (it == c->plugin_storage.end()
            || !(std::any_cast<bool>(&it->second) && *std::any_cast<bool>(&it->second))) {
                return;   // 没有 pending：不是"击败后的下只"
            }
            it->second = false;               // 消费（"下只"一次；落地与否都不重试）
            c->event_center_.remove_watcher(*watcher_id);
            (void)g_core->apply_anomaly_checked(c, ev.actor, anomaly,
                                                random_anomaly_duration(), self, chance,
                                                ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）────────────────────────
SKILL_EFFECT(2295, effect_2295_abolish_heal_pp_curse)
SKILL_EFFECT(698, effect_698_defeat_next_curse)
