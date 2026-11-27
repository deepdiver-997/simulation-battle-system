// K314 攻技削现血族 —— effect 371（1 宏 / 1 行）
//（苍穹圣虎 2784 args[1,50,33,1]；号段 4700-4709，worktree ../agents/k314，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 371，2026-10-21）：
//   「每次使用攻击技能都有50%概率减少对手1/3当前体力(boss无效)」
//   → db[1]=50 = 概率%（字面自证）；db[2]=33 = 现血百分比（"1/3"≈33% 字面互证）；
//   db[0]/db[3] 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b371\b 无魂印硬编码；②effect_info
//    371 技能空间模板不同文；③soul_lib 全 worktree/registry 无 371 注册——真缺口。
// 实现（板行处方：K164 roll + K302 粉伤）：
//   EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ roll_percent(db[1]) →
//   dmg = 对手当前血 × db[2] / 100（按施加瞬间现血计）→ deal_pink_damage(对手, dmg,
//   FIXED, owner)（粉伤吃护罩/免粉/减伤；dmg<=0 跳过）。boss 无效 → watcher 手动门。
// 注册：SOUL_PROGRAM(371) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4700_attack_hp_cut.cpp（号段 4700-4709）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 371;
constexpr int kWiredKeyBase = 990814000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=概率%（兜底 50；0=显式关闸）、db[2]=现血百分比（兜底 33）。
void k314_read_row(BattleContext* ctx, int owner, int& pct, int& percent) {
    pct = 50;
    percent = 33;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1) {
        pct = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        percent = db[2];
    }
}

// arm：EVENT_SKILL_USED（own+攻击技）→ roll → 削对手现血 db[2]%。
EffectResult k314_371_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
            int pct = 50;
            int percent = 33;
            k314_read_row(c, owner, pct, percent);
            if (pct <= 0 || !roll_soul_percent(*c, kMarkId, 0, owner, opp, pct)) {
                return;   // 关闸 / 未中
            }
            int dmg = foe.hp * percent / 100;
            if (dmg <= 0) {
                return;
            }
            (void)g_core->deal_pink_damage(c, opp, dmg, DamageKind::FIXED, owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（371 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k314_371_arm,
                        /*once=*/false, /*early=*/true)});
