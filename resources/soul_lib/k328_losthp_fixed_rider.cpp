// K328 已损失体力比例固伤族 —— effect 960（1 宏 / 1 行）
//（伊基塔 3357 args[30,1]；号段 4840-4849，worktree ../agents/k328，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 960，2026-10-21）：
//   「自身使用攻击技能附加对手已损失体力30%的固定伤害（BOSS无效）」
//   → db[0]=30 = 已损比例%（字面自证）；db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b960\b 无魂印硬编码；②effect_info
//    960 无模板行；③soul_lib 全 worktree/registry 无 960 注册——真缺口。
// 实现（板行处方：K10 693 已损口径 + 攻击附加固伤）：
//   EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ lost = 对手 max - 现血
//   （施加瞬间，K10 693 口径）→ dmg = lost × db[0] / 100（>0 才打）→
//   deal_pink_damage(对手, dmg, FIXED, owner)（粉伤通道）。boss 无效 → watcher
//   手动门。无概率槽 → 100% 必发。
// 注册：SOUL_PROGRAM(960) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4840_losthp_fixed_rider.cpp（号段 4840-4849）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 960;
constexpr int kWiredKeyBase = 990828000;   // +owner：watcher 已挂旗标（防再登场双挂）

// arm：EVENT_SKILL_USED（own+攻击技）→ 附加对手已损 db[0]% 固伤。
EffectResult k328_960_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"使用攻击技能"字面）
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = 1 - owner;
            ElfPet& foe = c->getPet(opp);
            if (foe.hp <= 0) {
                return;   // 存活门
            }
            int pct = 30;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 0 && db[0] > 0) {
                    pct = db[0];
                }
            }
            const int foe_max = resolve_pet_max_hp(foe);
            const int lost = foe_max - foe.hp;
            if (lost <= 0) {
                return;   // 满血无已损 → 不附加
            }
            const int dmg = lost * pct / 100;
            if (dmg <= 0) {
                return;
            }
            (void)g_core->deal_pink_damage(c, opp, dmg, DamageKind::FIXED, owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（960 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k328_960_arm,
                        /*once=*/false, /*early=*/true)});
