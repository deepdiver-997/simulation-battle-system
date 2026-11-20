// K174 攻击命中回血族 —— effect 818（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 818，2026-10-07）：
//   818：3189 水中灵 [200,0]「攻击技能命中后立即为自身恢复200点体力」/
//       4705 塔琳-MKII [400,1]「攻击技能命中后自身恢复400点体力（boss无效）」
//       → db[0]=回血量、db[1]=boss 位（PvP 引擎无 boss 面，不消费，记档）。
// 实现：攻击方 DETECT 票（K169 393 先手攻附同窗直系）——early arm（ENTER_EXIT_STAGE
// + plugin_storage 旗标防双触发）接线 DamagePhase::DETECT（管线注释明载
// DETECT=「受击检测/触发（反伤、附伤）」，回血挂攻击落地后时点）；门 =
// attackerId==owner + isRed + final>0（「命中后」：miss/免伤零伤不恢复，4399
// 行文锚定，与 893「使用后」族相反）+ onstage 现场查（桶 TEAM 绑定）；
// 动作 = heal_amount(db[0])（吃封回血/上限封顶原语内查）。
// ⚠️ 三查记录（2026-10-07）：①内核无 818 硬编码；②side_effect/effect_info 818
//    系技能空间同号（双注册空间常态，1004 先例），魂印以 effect_icon tips 为权威；
//    ③并集 registry 无 818 注册。4399✓3189/4705。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990702000;   // +宏*10+owner：管线已接线旗标（防 early 双触发）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 818：攻击技能命中后恢复自身体力（水中灵/塔琳双行）────────────────

EffectResult soulmark_818_arm(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 818, owner)) {
        return EffectResult::kOk;
    }
    const int heal = [&] {
        if (!ctx) {
            return 0;
        }
        const int slot = ctx->find_pet_with_soulmark(owner, 818);
        if (slot < 0 || slot > 5) {
            return 0;
        }
        const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        return db.empty() ? 0 : db[0];
    }();
    if (heal <= 0) {
        return EffectResult::kOk;   // 无回血量行不接线
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, heal](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 「命中后」：miss/零伤不恢复
            }
            if (!onstage_has_soul_mark(c, owner, 818)) {
                return;   // 换下场不恢复（DETECT 桶 TEAM 绑定，现场查在场）
            }
            (void)g_core->heal_amount(c, owner, heal);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
// 818 = early arm（ENTER_EXIT_STAGE，接 DETECT 攻击方票，旗标防 early 双触发）。
SOUL_PROGRAM(818, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_818_arm,
                    /*once=*/false, /*early=*/true)});
