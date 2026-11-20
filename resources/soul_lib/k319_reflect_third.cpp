// K319 反弹族 —— effect 602（1 宏 / 1 行）
//（天影无名 2840 args[3,1]；号段 4750-4759，worktree ../agents/k319，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 602，2026-10-21）：
//   「反弹1/3受到的伤害给对手（boss无效）」
//   → db[0]=3 = 反弹分母（"1/3"字面自证）；db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b602\b 无魂印硬编码；②effect_info
//    602 无模板行；③soul_lib 全 worktree/registry 无 602 注册——真缺口。
// 实现（板行处方：K3 thorn TAKE_DAMAGE 反弹 watcher 同款）：
//   EVENT_TAKE_DAMAGE watcher（own、只认被对手打的攻击伤害、ev.actor 范围守卫——
//   k_thorn_reflect 11 同款含 -1 无行动方事件守卫）→ heal 无、deal_pink_damage
//   (攻击方, ev.amount/db[0], FIXED, owner)（粉伤定额弹回，吃护罩/免粉/减伤——
//   反弹在攻击方吸血结算之后落地，帝皇之御口径）。boss 无效 → watcher 手动门。
// 注册：SOUL_PROGRAM(602) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4750_reflect_third.cpp（号段 4750-4759）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 602;
constexpr int kWiredKeyBase = 990819000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 只认两攻击伤害时点（k_thorn 11 同款就地复刻——"受到的伤害"限攻击系）。
bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// arm：TAKE_DAMAGE 反弹 watcher。
EffectResult k319_602_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
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
            // ⚠️ ev.actor 可能是 -1（无行动方事件）——范围守卫（k_thorn 11 同款，
            //   fuzz seed ~1601 段 UBSan 实证）。
            if (!c || !g_core || !g_core->deal_pink_damage) {
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
            int denom = 3;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 0 && db[0] > 0) {
                    denom = db[0];
                }
            }
            const ElfPet& foe = c->getPet(ev.actor);
            if (foe.hp <= 0) {
                return;   // 反弹出手方存活校验（k_thorn 同款）
            }
            const int rebound = ev.amount / denom;
            if (rebound > 0) {
                (void)g_core->deal_pink_damage(c, ev.actor, rebound,
                                               DamageKind::FIXED, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（602 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k319_602_arm,
                        /*once=*/false, /*early=*/true)});
