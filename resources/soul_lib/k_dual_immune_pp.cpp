// K355 — 2381 嗒啦啦4760，唯一effect_icon行1965，args=[1,0]。
// 全异常/能力下降免疫；回合开始所有技能恢复2PP（boss无效）。
// 行参无与2/概率/回合数对应的锚点，按tips定值，不猜args、不读a.row。
// 三查：内核/全feat soul+move无注册；effect_info2381为异文技能模板。
// K275永久ON_STAGE票 + K299 STAT_DROP + K227逐技能PP钳上限；现代魂免。
#include "common.h"
#include <algorithm>
namespace {
constexpr int kMarkId = 2381;
EffectResult grant(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) return EffectResult::kOk;
    ctx->grant_immunity(a.owner, ImmunityType::ANOMALY, coverage_all(),
                        0, 0, 238101, true, EffectScope::ON_STAGE,
                        0, kMarkId, ImmunityTier::Modern);
    ctx->grant_immunity(a.owner, ImmunityType::STAT_DROP, coverage_all(),
                        0, 0, 238102, true, EffectScope::ON_STAGE,
                        0, kMarkId, ImmunityTier::Modern);
    return EffectResult::kOk;
}
EffectResult recover_pp(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) return EffectResult::kOk;
    for (auto& skill : ctx->getPet(a.owner).skills) {
        skill.pp = std::min(skill.maxPP, skill.pp + 2);
    }
    return EffectResult::kOk;
}
}  // namespace
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &grant,
                        false, true, SoulScope::STAGE, /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &recover_pp,
                        false, false, SoulScope::STAGE, /*boss_invalid=*/true)});
