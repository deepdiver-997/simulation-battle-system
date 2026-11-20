// K354 — 1496 护卫者P2 3906，唯一effect_icon行1173，原序args=[10,0]。
// 对手每有一种能力提升，自身攻击承伤减少db[0]%；boss有效。
// 三查：内核无同号；effect_info1496为异文技能模板；全feat与soul/moves无注册。
// K344首空减伤槽 + K353正等级计种，按套路目录§二在ROUND_START快照，每轮重写。
#include "common.h"
#include <numerical-calculation/calculation.h>
namespace {
constexpr int kMarkId = 1496;
int db_at(const std::vector<int>& db, size_t index, int fallback) {
    return index < db.size() ? db[index] : fallback;
}
EffectResult foe_boost_kind_reduce(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) return EffectResult::kOk;
    const int enemy = 1 - a.owner;
    const int pct = db_at(ctx->getPet(a.owner).soulMark.args.owned_int_args, 0, 10);
    int kinds = 0;
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        if (ctx->ability_levels[enemy][slot] > 0) ++kinds;
    }
    if (kinds == 0 || pct <= 0) return EffectResult::kOk;  // 显式0保持关闸
    // pct×kinds用已有checked乘区检查中间积，不另造整数乘区。
    const int reduction = checked_pct_mul(pct, (kinds - 1) * 100, "soul1496_kind_pct");
    for (int slot = 0; slot < 4; ++slot) {
        if (ctx->ws.damage_reduce_add[a.owner][slot] == 0) {
            ctx->ws.damage_reduce_add[a.owner][slot] = reduction;
            break;
        }
    }
    return EffectResult::kOk;
}
}  // namespace
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &foe_boost_kind_reduce,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
