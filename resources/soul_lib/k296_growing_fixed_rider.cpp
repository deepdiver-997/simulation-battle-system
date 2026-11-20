// K296 成长固伤 rider 族 —— effect 629（1 宏 / 1 行）
//（德尔波拉 2886 args[50,300]；号段 4520-4529，worktree ../agents/k296，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 629，2026-10-21）：
//   「附加50点固定伤害,每回合增加50点,最多300点」
//   → db[0]=50 = 基数（=步长，两处 50 同源字面自证）；db[1]=300 = 上限（字面自证）
//   ——双参自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b629\b 无魂印硬编码；②effect_info
//    629 技能空间模板不同文（"消除状态下回合先制"，无关）；③soul_lib 全
//    worktree/registry 无 629 注册——真缺口。
// 实现（板行处方：EVENT_SKILL_USED 攻击技 rider + 三段账）：
//   ⚠️ 增长语义按官方字面"**每回合**增加"落地：ROUND_START 桶账本 += db[0]（首回合
//   即 50：arm 时账本置 0，回合首先增长后攻击）封顶 db[1]；EVENT_SKILL_USED（own +
//   攻击技，miss 也算 E13，属性技不触发——"附加"族 K164 109 同窗）→
//   fixed_damage(对手, 账本当前值)（粉伤通道）。若按"每次攻击递增"实现则多攻/跳过
//   回合语义与官方"每回合"不合——口径选择记档。
// 注册：SOUL_PROGRAM(629) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4520_growing_fixed_rider.cpp（号段 4520-4529）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 629;
constexpr int kLedgerKeyBase = 990796000;   // +owner：当前固伤值账本
constexpr int kWiredKeyBase = 990797000;    // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[0]=基数/步长（兜底 50）、db[1]=上限（兜底 300）。
void k296_read_row(BattleContext* ctx, int owner, int& step, int& cap) {
    step = 50;
    cap = 300;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        step = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        cap = db[1];
    }
}

// ROUND_START：账本增长（arm 置 0 后首回合 = db[0]）。
EffectResult k296_629_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    int step = 50;
    int cap = 300;
    k296_read_row(ctx, a.owner, step, cap);
    const int key = kLedgerKeyBase + a.owner;
    const int cur = any_int_or(ctx->plugin_storage[key], 0);
    ctx->plugin_storage[key] = (cur + step > cap) ? cap : cur + step;
    return EffectResult::kOk;
}

// arm：账本置 0 + 接攻击 rider watcher。
EffectResult k296_629_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kLedgerKeyBase + owner] = 0;
    const int key = kWiredKeyBase + owner;
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
                return;   // 攻击技才触发（属性技不算）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int value = any_int_or(c->plugin_storage[kLedgerKeyBase + owner], 0);
            if (value <= 0) {
                return;
            }
            (void)g_core->fixed_damage(c, 1 - owner, value);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（629 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k296_629_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k296_629_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
