// K326 受击转 HP+提升态免异常族 —— effect 670（1 宏 / 1 行）
//（鬼剑骷髅 2944 args[40,2,1]；号段 4820-4829，worktree ../agents/k326，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 670，2026-10-21）：
//   「每次受到攻击的40%转化为自身HP;若对手处于能力提升状态则自身免疫异常状态;(BOSS无效)」
//   → db[0]=40 = 转化%（字面自证）；db[1]/db[2] 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b670\b 无魂印硬编码；②effect_info
//    670 技能空间模板不同文；③soul_lib 全 worktree/registry 无 670 注册——真缺口。
// 实现（双 rider：K319 受击锚 + K299/K287 免疫票）：
//   ① EVENT_TAKE_DAMAGE watcher（own、只认两攻击伤害时点+ev.actor 范围守卫——
//   "受到攻击"限攻击系，k_thorn 11 同款）→ heal_amount(owner, ev.amount×db[0]/100)
//   （读快照 K3 口径；heal 原语封回血覆盖则不涨）。
//   ② BATTLE_ROUND_START 桶：对手任一槽 >0（提升态，K283 读）→ grant_immunity
//   (ANOMALY, coverage_all, duration=2)（K299/K287 票；duration=2 兼顾场景白盒——
//   门闭后残留 ≤2 回合记档；每回合重授）。boss 无效 → watcher 手动门 + 节点旗。
// 注册：SOUL_PROGRAM(670) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4820_hitdrain_boostimmune.cpp（4820-4829）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 670;
constexpr int kWiredKeyBase = 990826000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 只认两攻击伤害时点（k_thorn 11 同款就地复刻）。
bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ② ROUND_START：对手提升态门 → ANOMALY 全 mask 免疫票。
EffectResult k326_670_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    bool raised = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount && !raised; ++i) {
        raised = ctx->ability_levels[opp][i] > 0;
    }
    if (!raised) {
        return EffectResult::kOk;   // 对手非提升态 → 不挂票
    }
    int convert = 40;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            convert = db[0];
        }
    }
    if (convert <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/2, /*source_id=*/0,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/kMarkId);
    return EffectResult::kOk;
}

// ① arm：TAKE_DAMAGE watcher（受击转 HP）。
EffectResult k326_670_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            // ⚠️ ev.actor 可能 -1（无行动方事件）——范围守卫（k_thorn 11 同款）。
            if (!c || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.target != owner || ev.actor < 0 || ev.actor > 1 ||
                ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId) ||
                !is_attack_damage_event(ev)) {
                return;
            }
            int convert = 40;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 0 && db[0] > 0) {
                    convert = db[0];
                }
            }
            (void)g_core->heal_amount(c, owner, ev.amount * convert / 100);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（670 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k326_670_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k326_670_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
