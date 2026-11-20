// K330 受击反弹+窗口附加固伤族 —— effect 674（1 宏 / 1 行）
//（安格瑞 2950 args[30,1,2,150]；号段 4860-4869，worktree ../agents/k330，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 674，2026-10-21）：
//   「受到攻击时,将受到伤害的30%反馈给对手(boss无效)，并且下两回合附加150点固定伤害」
//   → db[0]=30 = 反弹%（字面自证）；db[2]=2 = 窗长回合（"下两回合"自证）；
//   db[3]=150 = 附加固伤（字面自证）；db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b674\b 无魂印硬编码；②effect_info
//    674 技能空间模板不同文；③soul_lib 全 worktree/registry 无 674 注册——真缺口。
// 实现（双 rider：K319 反弹同款 + K296 账本/K7 窗形拼装）：
//   ① EVENT_TAKE_DAMAGE watcher（own、两攻击伤害时点、actor 范围守卫）→
//   deal_pink_damage(攻击方, amount×db[0]/100, FIXED, owner) + 挂窗账本
//   （plugin_storage = roundCount + db[2]，覆盖"下两回合"）。
//   ② EVENT_SKILL_USED watcher（own + 攻击技）→ 账本在窗内（roundCount ≤ 到期轮）→
//   fixed_damage(对手, db[3])。boss 无效 → 双 watcher 手动门。
// 注册：SOUL_PROGRAM(674) 单节点（arm 双 watcher），总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4860_reflect_window_fixed.cpp（号段 4860-4869）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 674;
constexpr int kWinKeyBase = 990830000;     // +owner：窗到期轮（roundCount≤值 有效）
constexpr int kWiredKeyBase = 990831000;   // +owner：watcher 已挂旗标

// 只认两攻击伤害时点（k_thorn 同款就地复刻）。
bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// arm：双 watcher 接线。
EffectResult k330_674_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->fixed_damage) {
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
    // ① 受击 → 反弹 30% + 挂窗。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            // ⚠️ ev.actor 可能 -1（无行动方事件）——范围守卫（k_thorn 同款）。
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
            int reflect_pct = 30;
            int window_rounds = 2;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 0 && db[0] > 0) {
                    reflect_pct = db[0];
                }
                if (db.size() > 2 && db[2] > 0) {
                    window_rounds = db[2];
                }
            }
            const int rebound = ev.amount * reflect_pct / 100;
            if (rebound > 0) {
                const ElfPet& foe = c->getPet(ev.actor);
                if (foe.hp > 0) {
                    (void)g_core->deal_pink_damage(c, ev.actor, rebound,
                                                   DamageKind::FIXED, owner);
                }
            }
            c->plugin_storage[kWinKeyBase + owner] =
                c->roundCount + window_rounds;   // 受击轮 + db[2] = 窗到期轮
        },
        WatcherScope::TEAM);
    // ② 窗内攻击 → 附加 db[3] 固伤。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才附加（"附加固定伤害"族惯例）
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const auto win = c->plugin_storage.find(kWinKeyBase + owner);
            if (win == c->plugin_storage.end() ||
                any_int_or(win->second) < c->roundCount) {
                return;   // 无窗/已过期
            }
            int fixed = 150;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 3 && db[3] > 0) {
                    fixed = db[3];
                }
            }
            (void)g_core->fixed_damage(c, 1 - owner, fixed);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（674 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k330_674_arm,
                        /*once=*/false, /*early=*/true)});
