// K329 当前生命比例固伤族 —— effect 783（1 宏 / 1 行）
//（幽冥邪龙 3135 args[30,1]；号段 4850-4859，worktree ../agents/k329，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 783，2026-10-21）：
//   「攻击附加对手当前生命值30%的固定伤害（boss无效）」
//   → db[0]=30 = 现血比例%（字面自证）；db[1]=1 废位不消费（K1 纪律）。
//   ⚠️ 与 960 同参 [30,1] 不同义：960 按**已损**（max-现血）、本行按**当前**——
//   两行互锚辨析（板行提示），实现按各自 tips 落地。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b783\b 无魂印硬编码；②effect_info
//    783 无模板行；③soul_lib 全 worktree/registry 无 783 注册——真缺口。
// 实现（板行处方：K9 293 攻击附加固伤，按对手当前 hp×db[0]%）：
//   EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ dmg = 对手当前血 × db[0] / 100
//   （施加瞬间现血；>0 才打）→ deal_pink_damage(对手, dmg, FIXED, owner)（粉伤通道）。
//   boss 无效 → watcher 手动门。无概率槽 → 100% 必发。
// 注册：SOUL_PROGRAM(783) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4850_curhp_fixed_rider.cpp（号段 4850-4859）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 783;
constexpr int kWiredKeyBase = 990829000;   // +owner：watcher 已挂旗标（防再登场双挂）

// arm：EVENT_SKILL_USED（own+攻击技）→ 附加对手当前血 db[0]% 固伤。
EffectResult k329_783_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
                return;   // 攻击技才触发（"攻击附加"字面）
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
            const int dmg = foe.hp * pct / 100;   // 按施加瞬间当前血
            if (dmg <= 0) {
                return;
            }
            (void)g_core->deal_pink_damage(c, opp, dmg, DamageKind::FIXED, owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（783 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k329_783_arm,
                        /*once=*/false, /*early=*/true)});
