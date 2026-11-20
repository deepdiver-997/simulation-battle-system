// 腐蚀者套装（套装 id 387，部件 1300768-1300772 头盔/面具/机甲/束带/胫甲，5 件）
//
// 官方全文（equip.bytes desc；suit.bytes id=387，五件逐件同文）：
//   背包内精灵攻击+10%、特攻+10%；所有攻击技能忽略对手防御值和特防值的15%；
//   受到攻击使下回合对手麻痹1回合，未触发则降低对手最大HP的1/6
//   （仅限赛尔与赛尔间对战）
//
// 拆解（数值部分走 custom_equip_stats 数据层，见 suit.cpp resolve_equipment）：
//   A 攻击+10%、特攻+10%        → 数据层 per_suit 行（不动效果）
//   B 忽略对手防御/特防 15%      → ROUND_START 节点：ws.defense_ignore_pct[对手] = 15
//       （伤害公式 Calculation::calculateDamage 消费；物理吃防御、特殊吃特防，同一字段）
//   C 受到攻击 → 下回合对手麻痹 1 回合；未触发 → 削对手最大 HP 1/6
//       → EVENT_HIT 监听（TEAM、永久）：对手命中己方且为攻击技能 →
//         apply_anomaly(麻痹, 1 回合)；未落地（免疫/挡下/抗性/反弹/转化…）→
//         reduce_max_hp_flat(对手在场，max/6，下限 1)。
//
// "未触发"判定：只有 麻痹确实落地（SUCCESS / 替换已有 / 延长持续）算触发；
//   其余结果（免疫、被效果挡下、抗性、弹控反弹、转化、目标已死）都算未触发 → 削上限。
// 时点粒度：EVENT_HIT 每次技能命中一次（见 chenxi_447.cpp 头注释）——多段攻击不重复触发。
// 生命周期：全部 TEAM 常驻（套装纪律，见 suit_reg.h）。

#include "common.h"

namespace {

constexpr int kSuitFushi = 387;
constexpr int kNodeInstall = 0;   // early：装 C 监听器（幂等）
constexpr int kNodeRoundStart = 1;

// C 的监听器安装（early，幂等）。
EffectResult fushi_install(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kSuitFushi * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = true;

    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT,
        owner,
        ctx->roundCount,
        /*duration_rounds=*/0,   // 永久
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core) {
                return;
            }
            // 对手命中己方且为攻击技能（属性技能不算"受到攻击"，同晨曦口径）。
            if (ev.actor < 0 || ev.actor > 1 || ev.actor == owner) {
                return;
            }
            if (wctx->ws.skill_type_view[ev.actor] == static_cast<int>(SkillType::Attribute)) {
                return;
            }
            // 受到攻击 → 尝试麻痹攻击方（"下回合麻痹1回合"：从回合中段施加要覆盖
            // 下一整回合 → duration=2，end_round=R+2，回合数语义 exclusive）。
            const ApplyAnomalyResult r =
                g_core->apply_anomaly(wctx, ev.actor, static_cast<int>(AbnormalStatusId::Paralysis),
                                      /*duration_rounds=*/2, owner);
            const bool landed = (r == ApplyAnomalyResult::SUCCESS
                                 || r == ApplyAnomalyResult::REPLACED_EXISTING
                                 || r == ApplyAnomalyResult::DURATION_EXTENDED);
            if (landed) {
                return;
            }
            // 未触发 → 削攻击方（在场精灵）最大 HP 的 1/6（点数版原语取整精确，下限 1）。
            ElfPet& attacker = on_stage_pet(wctx, ev.actor);
            const int max_hp = resolve_pet_max_hp(attacker);
            if (max_hp <= 0) {
                return;
            }
            g_core->reduce_max_hp_flat(wctx, ev.actor, wctx->on_stage[ev.actor],
                                       max_hp / 6, /*floor=*/1);
        },
        WatcherScope::TEAM,
        /*valid_id=*/0
    );
    return EffectResult::kOk;
}

// ── B：忽略对手防御/特防 15%（恒定条款，每回合重写）────────────────────
EffectResult fushi_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.defense_ignore_pct[1 - owner] = 15;
    return EffectResult::kOk;
}

}  // namespace

SUIT_PROGRAM(kSuitFushi,
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &fushi_install,
                             /*once=*/false, /*early=*/true),
             SoulMarkNodeRef(State::BATTLE_ROUND_START, &fushi_round_start))
