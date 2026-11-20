// K177 攻击技消回合类族 —— effect 399（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 399，2026-10-07）：
//   399：2831 尤米茜 [1,50]「每次使用攻击技能有50%概率消除对手回合类效果」/
//       3155 骸烬箭灵 [1,100]「所有攻击技能消除对手回合类效果」（100% 档行文省略概率）
//       → db[0]=域旗恒 1（不消费，记档）、db[1]=百分比概率。
// 实现：EVENT_SKILL_USED 窗（K2 358 同窗：own + used_attribute 属性技排除）→
// roll_percent(db[1])（0/100 短路不耗 rand）→ break_round_effects(对手)
// （三态原语，1038 同款；本宏无后续效果，返回值不分支）。
// ⚠️ 判据记档：工单通道写 executed_skill_is_attack——该助手读 ws.skill_type_view
//    物化视图，而 E13 锚点在 execute 起点发射（物化之前）读不到本技能；本窗攻击技
//    门用事件载荷 used_attribute（声明类型，K2 358/K164 109/K175 893 三先例同款）。
//    executed_* 助手适用于行动后/回合末锚点（速查 §四点五）。
// ⚠️ 「使用」口径：E13 使用即触发（miss/被盔也算使用，2026-10-03 裁定）——场景 D 取证。
// ⚠️ 三查记录（2026-10-07）：①内核无 399 硬编码；②soul_lib 并集无注册；③dump 实证
//    两行同构互锚（100% 行概率省略由行文补齐）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990704000;   // +宏*10+owner：watcher 已挂旗标（防 early 双触发）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 399：使用攻击技能概率消除对手回合类效果（尤米茜/骸烬箭灵）────────

EffectResult soulmark_399_arm(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 399, owner)) {
        return EffectResult::kOk;
    }
    const int pct = [&] {
        if (!ctx) {
            return 0;
        }
        const int slot = ctx->find_pet_with_soulmark(owner, 399);
        if (slot < 0 || slot > 5) {
            return 0;
        }
        const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        return db.size() > 1 ? db[1] : 0;
    }();
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 399)) {
                return;
            }
            if (!roll_percent(pct)) {
                return;
            }
            if (g_core && g_core->break_round_effects) {
                (void)g_core->break_round_effects(wctx, wctx->opponent(owner));
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
// 399 = early arm（ENTER_EXIT_STAGE，挂 TEAM watcher，plugin_storage 旗标防 early 双触发）。
SOUL_PROGRAM(399, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_399_arm,
                    /*once=*/false, /*early=*/true)});
