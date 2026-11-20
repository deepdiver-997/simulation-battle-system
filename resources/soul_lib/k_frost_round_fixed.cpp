// K352 — 1723 K·棱镜4133，effect_icon唯一行1360，原序args=[200,0]。
// 战斗阶段结束附加db[0]点固伤；对手处于冻伤5时翻倍，无boss无效子句。
// 三查：内核无同号；effect_info1723为不同文技能模板；全feat与soul/moves无注册。
// K312 ROUND_END粉伤原语 + K193异常查询；行参一律db_at，不读a.row。
#include "common.h"
#include <numerical-calculation/calculation.h>

namespace {
constexpr int kMarkId = 1723;
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);

int db_at(const std::vector<int>& db, size_t index, int fallback) {
    return index < db.size() ? db[index] : fallback;
}

EffectResult frost_round_fixed(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->has_active_anomaly ||
        a.owner < 0 || a.owner > 1 || !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int enemy = 1 - a.owner;
    const auto& self = ctx->getPet(a.owner);
    if (self.hp <= 0 || ctx->getPet(enemy).hp <= 0) return EffectResult::kOk;
    const int base = db_at(self.soulMark.args.owned_int_args, 0, 200);
    if (base <= 0) return EffectResult::kOk;  // 显式0不被fallback覆盖
    // checked_pct_mul的第二参是增幅百分数：+100%才是翻倍，不是传200。
    const int damage = g_core->has_active_anomaly(ctx, enemy, kFrostbite)
        ? checked_pct_mul(base, 100, "soul1723_frost_double") : base;
    (void)g_core->deal_pink_damage(ctx, enemy, damage, DamageKind::FIXED, a.owner);
    return EffectResult::kOk;
}
}  // namespace

SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &frost_round_fixed,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
