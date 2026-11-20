// K353 — 1493 巡逻者T1 3904，effect_icon唯一行1171，原序args=[10,0]。
// 自身每有一种能力提升，攻击伤害提升db[0]%；boss有效。数正等级槽，不数等级和。
// 三查：内核无同号；effect_info1493为异文技能模板；全feat及soul/moves无注册。
// 技能999文字含“额外”，走独立乘区；本魂印无“额外”，照K283常驻加算通道。
// 套路目录§二：ROUND_START快照→ws每回合重写，回合内等级改变下轮重新计种。
#include "common.h"

namespace {
constexpr int kMarkId = 1493;
int db_at(const std::vector<int>& db, size_t index, int fallback) {
    return index < db.size() ? db[index] : fallback;
}
EffectResult boost_kind_damage(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) return EffectResult::kOk;
    const int pct = std::max(0, db_at(ctx->getPet(a.owner).soulMark.args.owned_int_args, 0, 10));
    int kinds = 0;
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        if (ctx->ability_levels[a.owner][slot] > 0) ++kinds;
    }
    // 这里累加百分数至浮点申报槽；伤害乘区由内核checked_pct_mul执行。
    ctx->ws.damage_add_pct[a.owner] += static_cast<float>(kinds) * static_cast<float>(pct);
    return EffectResult::kOk;
}
}  // namespace
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &boost_kind_damage,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
