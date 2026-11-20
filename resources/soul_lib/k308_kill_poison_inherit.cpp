// K308 击败传染异常族 —— effect 309（1 宏 / 1 行）
//（幻草灵狮 2618 args[1,0]；号段 4640-4649，worktree ../agents/k308，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 309，2026-10-21）：
//   「若击败中毒的对手，则下一个出场的对手也进入中毒状态」
//   → db[0]=1 = 中毒码（Poison=1 官方码字面自证）；db[1]=0 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b309\b 无魂印硬编码；②effect_info
//    309 无模板行；③soul_lib 全 worktree/registry 无 309 注册——真缺口。
// 实现：
//   ① BATTLE_AFTER_DEFEATED 桶（双侧每杀必经，K6 定版归因时点）：find_defeated_foe_slot
//   归因门（本时点先于换宠——被杀者异常仍按方在册，has_active_anomaly 直查）→
//   被击败者带 db[0] 异常 → 挂传染待决旗（plugin_storage）。
//   ② EVENT_ENTER_STAGE watcher（battleFsm:626 载荷 actor=登场方；只认对手侧）：
//   旗在 → apply_anomaly_checked(对手侧, db[0] 码, duration, owner, pct=100, SoulMark)
//   → 清旗（只传下一个）。印主已倒地/不在场则旗作废不传（onstage 门，STAGE 域口径）。
// 注册：SOUL_PROGRAM(309) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4640_kill_poison_inherit.cpp（号段 4640-4649）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 309;
constexpr int kPendingKeyBase = 990808000;   // +owner：传染待决旗
constexpr int kWiredKeyBase = 990809000;     // +owner：watcher 已挂旗标

// 行参：db[0]=异常码（兜底 Poison）。
int k308_read_code(BattleContext* ctx, int owner) {
    int code = static_cast<int>(AbnormalStatusId::Poison);
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            code = db[0];
        }
    }
    return code;
}

// ① AFTER_DEFEATED：击败中毒对手 → 挂传染待决旗。
EffectResult k308_309_after_defeated(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (find_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;   // 本时点无 owner 方击杀（K6 归因门）
    }
    const int code = k308_read_code(ctx, owner);
    if (!g_core->has_active_anomaly(ctx, 1 - owner, code)) {
        return EffectResult::kOk;   // 被击败者非中毒
    }
    ctx->plugin_storage[kPendingKeyBase + owner] = 1;
    return EffectResult::kOk;
}

// arm：ENTER_STAGE watcher（对手侧入场）→ 传染。
EffectResult k308_309_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
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
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != 1 - owner) {
                return;   // 只看对手侧入场（battleFsm:626 actor=登场方）
            }
            if (c->plugin_storage.count(kPendingKeyBase + owner) == 0) {
                return;   // 无待决旗
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                c->plugin_storage.erase(kPendingKeyBase + owner);
                return;   // 印主不在场（STAGE 域）：旗作废
            }
            const int code = k308_read_code(c, owner);
            if (code > 0 && is_valid_abnormal_status_id(code)) {
                (void)g_core->apply_anomaly_checked(c, 1 - owner, code,
                                                    random_anomaly_duration(), owner,
                                                    /*chance_pct=*/100,
                                                    ChanceSource::SoulMark);
            }
            c->plugin_storage.erase(kPendingKeyBase + owner);   // 只传下一个
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（309 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k308_309_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_AFTER_DEFEATED, &k308_309_after_defeated,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
