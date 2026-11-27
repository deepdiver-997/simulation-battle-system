// K175 攻击后随机降属族 —— effect 893（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 893，2026-10-07）：
//   893：3280 德罗西尼 [2,2,1]「使用攻击技能后对手随机2项能力-2（BOSS无效）」/
//       4703 波波鱼 [3,1,1]「自身使用攻击技能后令对手随机3项能力等级-1（boss无效）」
//       → db[0]=项数、db[1]=幅度、db[2]=boss 位（PvP 引擎无 boss 面，不消费，记档）。
// 实现：EVENT_SKILL_USED 窗（K2 358 同窗：own + used_attribute 属性技排除）→
// 随机去重选槽（1314「随机 3 项-1」同款 rand%6 循环）→ stat_drop(对手, 槽, db[1])
// （附加禁令/免弱/钳 -6 原语内查；AT_FLOOR 槽照常计入项数）。
// ⚠️ 行文口径：「使用后」非「命中后」（4399✓两行）——miss 也触发（E13 锚点
// 先于可用性判定），与 818「命中后」族相反；取证记档进场景 D。
// ⚠️ 三查记录（2026-10-07）：①内核无 893 硬编码；②side_effect/effect_info 893
//    系技能空间同号（双注册空间常态，1004 先例）；③并集 registry 无 893 注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990703000;   // +宏*10+owner：watcher 已挂旗标（防 early 双触发）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 893：使用攻击技能后随机 N 项降属（德罗西尼/波波鱼）──────────────

EffectResult soulmark_893_arm(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 893, owner)) {
        return EffectResult::kOk;
    }
    const int drop_count = [&] {
        if (!ctx) {
            return 0;
        }
        const int slot = ctx->find_pet_with_soulmark(owner, 893);
        if (slot < 0 || slot > 5) {
            return 0;
        }
        const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        return db.empty() ? 0 : db[0];
    }();
    const int drop_amount = [&] {
        if (!ctx) {
            return 0;
        }
        const int slot = ctx->find_pet_with_soulmark(owner, 893);
        if (slot < 0 || slot > 5) {
            return 0;
        }
        const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        return db.size() > 1 ? db[1] : 0;
    }();
    if (drop_count <= 0 || drop_amount <= 0 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, drop_count, drop_amount](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 893)) {
                return;
            }
            const int opp = 1 - owner;
            int picked[6] = {};
            int done = 0;
            int guard = 0;
            while (done < drop_count && guard < 100) {
                ++guard;
                const int slot = choose_soul_range(*wctx, 893, 0, owner, opp, 0, 5);
                if (picked[slot]) {
                    continue;
                }
                picked[slot] = 1;
                ++done;
                (void)g_core->stat_drop(wctx, opp, slot, drop_amount, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
// 893 = early arm（ENTER_EXIT_STAGE，挂 TEAM watcher，plugin_storage 旗标防 early 双触发）。
SOUL_PROGRAM(893, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_893_arm,
                    /*once=*/false, /*early=*/true)});
