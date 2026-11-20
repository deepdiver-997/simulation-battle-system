// K270 致死残血+对手低体门族 —— effect 365（1 宏 / 1 行）
//（真龙·无上 2769；号段 4260-4269，worktree ../agents/k270，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 365，2026-10-21）：
//   row 2769 args[4,1]：「受到致死攻击时若对手体力不足1/4残留1点体力（boss无效）」
//   → db[0]=4 = 分母（"1/4"字面自证）；db[1]=1 = 残留体力点数（字面自证）——两参
//   自洽均消费（796 同款判据）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b365\b 无硬编码；②effect_info 365
//    无模板行；③soul_lib 全 worktree/registry 无 365 注册——真缺口。
// 实现（板行处方：死亡拦截器信号模型，帝皇之御 2026-09-30 迁移定稿同款）：
//   EVENT_HP_EXHAUSTED watcher（own，TEAM）——伤害通道 0 血检测面天然只应"致死攻击"
//   （秒杀/自爆走自有信号面不发本事件，帝皇之御口径）→ 对手 hp×db[0] < 对手max
//   （"不足1/4"严格小于，×分母防整除截断）→ heal_amount(owner, db[1]) 拉起；封回血
//   覆盖当前时点则拉不起来、死亡照常成立（盖亚同款）。免死逐次应答（信号各答各的，
//   时限非次数——本行无时限窗，常驻）。对手已倒（双同归）→ 门闭不拉（口径选择记档）。
//   boss 无效子句 → watcher 手动门 is_boss_challenge（diwang 反弹半边同款）。
// 注册：SOUL_PROGRAM(365) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4260_foe_lowhp_deathgate.cpp（号段 4260-4269）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 365;
constexpr int kWiredKeyBase = 990770000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[0]=分母（兜底 4）、db[1]=残留点数（兜底 1）——tips 字面自证。
void k270_read_row(BattleContext* ctx, int owner, int& denom, int& keep) {
    denom = 4;
    keep = 1;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        denom = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        keep = db[1];
    }
}

// arm：EVENT_HP_EXHAUSTED（own）→ 对手低体门 → heal 拉起 db[1] 点。
EffectResult k270_365_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
        EventType::EVENT_HP_EXHAUSTED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.target != owner) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门，diwang 同款）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const ElfPet& foe = c->getPet(1 - owner);
            if (foe.hp <= 0) {
                return;   // 对手已倒（双同归）：门闭不拉（记档）
            }
            int denom = 4;
            int keep = 1;
            k270_read_row(c, owner, denom, keep);
            if (foe.hp * denom >= resolve_pet_max_hp(foe)) {
                return;   // 对手体力未"不足 1/4"（严格小于）
            }
            (void)g_core->heal_amount(c, owner, keep);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（365 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k270_365_arm,
                        /*once=*/false, /*early=*/true)});
