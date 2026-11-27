// K186 已损体力动态概率麻痹+追击族 —— effect 1880（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 1880，2026-10-07）：
//   1880：4295 吕蒙 两行 [10,1]/[20,1]（进化同构互锚）
//     「战斗阶段结束时有概率令对手麻痹，概率等同于自身当前已损失体力百分比（boss无效）；
//      自身攻击命中后附加对手已损失体力值{db[0]}%的固定伤害（boss有效）」
//     → db[0]=追击百分比（10/20）、db[1]=boss 旗（两子句 boss 位互异但同值 1，
//       PvP 引擎无 boss 面，不消费，分记记档）。
// 实现（双节点，声明进 SOUL_PROGRAM）：
//   ① BATTLE_OLD_ROUND_END_1 节点（1314「战斗阶段结束」同款 State）：
//      pct = 已损×100/max → roll_percent(pct) → apply_anomaly_checked(对手, 麻痹0)
//      （码 0 麻痹 checked 通道 K176 已验正常；满血 0% 短路不耗 rand）。
//   ② 攻击方 DETECT 票（293 相位同步直系/K169 同窗）：attackerId==owner + isRed +
//      final>0 + onstage 现场门 → deal_pink_damage(对手, 对手已损×db[0]/100, FIXED)
//      （「对手已损」取伤害落地前快照——DETECT 相位在 hp 结算前）。
// ⚠️ 三查记录（2026-10-07）：①内核无 1880 硬编码；②soul_lib 并集无注册；
// ③两行 args 双参自洽、4399✓4295（双子句 boss 位分记）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990705000;   // +宏*10+owner：管线已接线旗标（防 early 双触发）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

int row_db0(BattleContext* ctx, int owner, int mark_id) {
    const int slot = ctx->find_pet_with_soulmark(owner, mark_id);
    if (slot < 0 || slot > 5) {
        return 0;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    return db.empty() ? 0 : db[0];
}

// ── ① 战斗阶段结束：roll 自身已损% → 麻痹对手 ──────────────────────────────

EffectResult k186_1880_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1880)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    const int lost_pct = (max_hp - self.hp) * 100 / max_hp;
    if (!roll_percent(ctx->rng(), lost_pct)) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, ctx->opponent(owner),
                                        static_cast<int>(AbnormalStatusId::Paralysis),
                                        random_anomaly_duration(), owner, lost_pct,
                                        ChanceSource::SoulMark);
    return EffectResult::kOk;
}

// ── ② 攻击命中后：附加对手已损体力 db[0]% 固定伤害 ─────────────────────────

EffectResult k186_1880_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 1880, owner)) {
        return EffectResult::kOk;
    }
    const int pursue_pct = row_db0(ctx, owner, 1880);
    if (pursue_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, pursue_pct](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤附加（命中后）
            }
            if (!onstage_has_soul_mark(c, owner, 1880)) {
                return;
            }
            const ElfPet& foe = c->getPet(c->opponent(owner));
            const int foe_max = resolve_pet_max_hp(foe);
            const int foe_lost = foe_max - foe.hp;
            if (foe_lost <= 0) {
                return;
            }
            const int pursue = foe_lost * pursue_pct / 100;
            if (pursue > 0) {
                g_core->deal_pink_damage(c, c->opponent(owner), pursue, DamageKind::FIXED,
                                         owner);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双节点）────────
SOUL_PROGRAM(1880, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k186_1880_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k186_1880_round_end)});
