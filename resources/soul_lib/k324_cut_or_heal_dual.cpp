// K324 攻技削现血+属性回血族 —— effect 653（1 宏 / 1 行）
//（佐伊尔 2923 args[1,3,1,2,200]；号段 4800-4809，worktree ../agents/k324，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 653，2026-10-21）：
//   「每次使用攻击技能减少对手1/3当前体力（boss无效）;每次使用属性技能恢复200HP」
//   → db[1]=3 = 削血分母（"1/3"字面自证）；db[4]=200 = 属性技回血（字面自证）；
//   db[0]/db[2]/db[3] 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b653\b 无魂印硬编码；②effect_info
//    653 技能空间模板不同文；③soul_lib 全 worktree/registry 无 653 注册——真缺口。
// 实现（双 rider 按 used_attribute 分派，K314/K276 同窗拼装）：
//   EVENT_SKILL_USED（own）：
//   · 攻击技（!used_attribute，miss 也算 E13）→ dmg = 对手现血 / db[1]（至少 1）→
//     deal_pink_damage(FIXED)（粉伤通道；存活门）。pct=100（"每次"字面）。
//   · 属性技 → heal_amount(owner, db[4])（恢复原语，封回血钳 max）。
//   boss 无效 → watcher 手动门。
// 注册：SOUL_PROGRAM(653) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4800_cut_or_heal_dual.cpp（号段 4800-4809）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 653;
constexpr int kWiredKeyBase = 990824000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=削血分母（兜底 3）、db[4]=属性技回血（兜底 200）。
void k324_read_row(BattleContext* ctx, int owner, int& denom, int& heal) {
    denom = 3;
    heal = 200;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1 && db[1] > 0) {
        denom = db[1];
    }
    if (db.size() > 4 && db[4] > 0) {
        heal = db[4];
    }
}

// arm：EVENT_SKILL_USED（own）→ 攻击技削现血 / 属性技回血。
EffectResult k324_653_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
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
            if (!c || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
                return;
            }
            if (ev.actor != owner) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int denom = 3;
            int heal = 200;
            k324_read_row(c, owner, denom, heal);
            if (!ev.used_attribute) {
                // 攻击技：削对手现血 1/db[1]（按施加瞬间现血计，存活门）。
                const int opp = 1 - owner;
                ElfPet& foe = c->getPet(opp);
                if (foe.hp <= 0 || denom <= 0) {
                    return;
                }
                int dmg = foe.hp / denom;
                if (dmg <= 0) {
                    dmg = 1;   // 现血 < 分母时保底 1（口径记档）
                }
                (void)g_core->deal_pink_damage(c, opp, dmg, DamageKind::FIXED, owner);
            } else if (heal > 0) {
                (void)g_core->heal_amount(c, owner, heal);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（653 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k324_653_arm,
                        /*once=*/false, /*early=*/true)});
