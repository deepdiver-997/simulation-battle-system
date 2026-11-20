// K338 提升态固伤/未触发吸取族 —— effect 701（1 宏 / 1 行）
//（千夜死神 2999 args[50,200]；号段 4940-4949，worktree ../agents/k338，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 701，2026-10-21）：
//   「自身每处于一种能力提升状态则附加50点固定伤害,若未触发则每回合{吸取}对手200点{体力}」
//   → db[0]=50 = 每种提升态固伤（字面自证）；db[1]=200 = 未触发吸取量（字面自证）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b701\b 无魂印硬编码；②effect_info
//    701 技能空间模板不同文；③soul_lib 全 worktree/registry 无 701 注册——真缺口。
// 实现（板行处方：K283 门读 + K164 rider + K302 拆原语）：
//   ① EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ 数提升态槽数 n（任一槽 >0
//   计 1 种，六槽全扫）→ n>0 → fixed_damage(对手, n×db[0])。
//   ② BATTLE_ROUND_END 桶：n==0（"未触发"）→ 吸取对手 min(db[1], 对手存量)：
//   deal_pink_damage(FIXED) + heal_amount 等量（K302 拆原语；吸取不越过存量）。
// 注册：SOUL_PROGRAM(701) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4940_raised_fixed_or_drain.cpp（4940-4949）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 701;

// 行参：db[0]=每态固伤（兜底 50）、db[1]=未触发吸取量（兜底 200）。
void k338_read_row(BattleContext* ctx, int owner, int& per_state, int& drain) {
    per_state = 50;
    drain = 200;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        per_state = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        drain = db[1];
    }
}

// 提升态槽数（任一槽 >0 计 1 种）。
int count_raised(BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ① EVENT_SKILL_USED：攻击技 → n×db[0] 固伤（n>0 时）。
EffectResult k338_701_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = 990838000 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"附加固定伤害"族惯例）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int n = count_raised(c, owner);
            if (n <= 0) {
                return;   // 未触发（②句接管）
            }
            int per_state = 50;
            int drain = 200;
            k338_read_row(c, owner, per_state, drain);
            (void)g_core->fixed_damage(c, 1 - owner, n * per_state);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② ROUND_END：未触发（无提升态）→ 吸取对手 db[1] 点。
EffectResult k338_701_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    if (count_raised(ctx, owner) > 0) {
        return EffectResult::kOk;   // 提升态在 → ①句已接管
    }
    int per_state = 50;
    int drain = 200;
    k338_read_row(ctx, owner, per_state, drain);
    int dmg = (drain > foe.hp) ? foe.hp : drain;   // 吸取不越过存量（K302 口径）
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, dmg);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（701 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k338_701_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k338_701_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
