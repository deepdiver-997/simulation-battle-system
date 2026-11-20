// K356 — 2374 雷弧4754，唯一effect_icon行1958，args=[1,0]无可锚数值。
// 回合首对手有能力提升→本回合自身必暴且攻击伤害+50%，boss无效。
// 三查：内核、全feat soul/moves无注册；effect_info2374为异文技能模板。
// K353加算增伤槽 + 227 must_crit先例；文本明确ROUND_START，ws每轮清零。
#include "common.h"
namespace {
constexpr int kMarkId = 2374;
EffectResult foe_boost_crit_damage(BattleContext* ctx,const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx,a.owner,kMarkId)) return EffectResult::kOk;
    for (int slot=0;slot<BattleContext::kAbilityLevelSlotCount;++slot) {
        if (ctx->ability_levels[1-a.owner][slot] > 0) {
            ctx->ws.must_crit[a.owner]=true;
            ctx->ws.damage_add_pct[a.owner]+=50.0f;
            break;  // 任一提升即触发一次，不按种类/等级叠加
        }
    }
    return EffectResult::kOk;
}
}  // namespace
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START,&foe_boost_crit_damage,
                        false,false,SoulScope::STAGE,/*boss_invalid=*/true)});
