// K341 上轮受伤附加+回合末回血族 —— effect 711（1 宏 / 1 行）
//（神佑之翼·炽天使 3014 args[50,1,200]；号段 4970-4979，worktree ../agents/k341，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 711，2026-10-21）：
//   「每回合使用攻击技能时附加自身上回合受到伤害数值的50%固定伤害（boss无效）;战斗
//   阶段结束时恢复200HP」
//   → db[0]=50 = 附加%（字面自证）；db[2]=200 = 回合末回血（字面自证）；
//   db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b711\b 无魂印硬编码；②effect_info
//    711 无模板行；③soul_lib 全 worktree/registry 无 711 注册——真缺口。
// 实现（板行处方：K330 窗形换账本 + K315 无条件回血）：
//   ① EVENT_TAKE_DAMAGE watcher（own、两攻击伤害时点、actor 守卫）→ 当轮受伤账本
//   累加 ev.amount。
//   ② BATTLE_ROUND_END 桶：last = 当轮账本（清零重计）+ heal_amount(db[2])（无条件，
//   恢复原语钳 max）。
//   ③ EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ fixed_damage(对手,
//   last×db[0]/100)（last=上轮快照；0 → 不附加）。boss 无效 → 手动门三处。
// 注册：SOUL_PROGRAM(711) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4970_lasthit_rider_heal.cpp（号段 4970-4979）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 711;
constexpr int kCurKeyBase = 990841000;     // +owner：当轮受伤累计
constexpr int kLastKeyBase = 990842000;    // +owner：上轮受伤快照
constexpr int kWiredKeyBase = 990843000;   // +owner：watcher 已挂旗标

// 只认两攻击伤害时点（k_thorn 同款就地复刻）。
bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// 行参：db[0]=附加%（兜底 50）、db[2]=回合末回血（兜底 200）。
void k341_read_row(BattleContext* ctx, int owner, int& pct, int& heal) {
    pct = 50;
    heal = 200;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        pct = db[0];
    }
    if (db.size() > 2 && db[2] > 0) {
        heal = db[2];
    }
}

// ROUND_END：last 快照 + 无条件回血。
EffectResult k341_711_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int cur_key = kCurKeyBase + owner;
    ctx->plugin_storage[kLastKeyBase + owner] =
        any_int_or(ctx->plugin_storage[cur_key], 0);
    ctx->plugin_storage.erase(cur_key);   // 当轮重计
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    int pct = 50;
    int heal = 200;
    k341_read_row(ctx, owner, pct, heal);
    if (heal > 0) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

// arm：受击账本 + 攻击附加 rider。
EffectResult k341_711_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->fixed_damage) {
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
            if (!c) {
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
            const int cur_key = kCurKeyBase + owner;
            c->plugin_storage[cur_key] =
                any_int_or(c->plugin_storage[cur_key], 0) + ev.amount;
        },
        WatcherScope::TEAM);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才附加
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int pct = 50;
            int heal = 200;
            k341_read_row(c, owner, pct, heal);
            const int last = any_int_or(c->plugin_storage[kLastKeyBase + owner], 0);
            const int rider = last * pct / 100;
            if (rider > 0) {
                (void)g_core->fixed_damage(c, 1 - owner, rider);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（711 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k341_711_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k341_711_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
