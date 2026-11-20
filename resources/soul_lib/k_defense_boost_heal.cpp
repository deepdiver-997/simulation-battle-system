// K350 — 1877 切尔诺4292，effect_icon唯一行1513，原序args=[2,0]。
// 战斗阶段结束时，防御/特防任一提升→恢复最大体力1/db[0]，boss有效。
// 三查：内核无同号，effect_info1877为不同文技能模板；soul/moves及feat分支无注册。
// K206条件回血 + K281原序行参；全属性槽序中防御=2、特防=3，不能误用特攻=1。
// 无自记状态、无直接写HP；db[1]=0未与文案互锚，不猜语义。
#include "common.h"

namespace {
constexpr int kMarkId = 1877;

int db_at(const std::vector<int>& db, size_t index, int fallback) {
    return index < db.size() ? db[index] : fallback;
}

EffectResult defense_boost_heal(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto& self = ctx->getPet(a.owner);
    if (self.hp <= 0 || (ctx->ability_levels[a.owner][2] <= 0 &&
                        ctx->ability_levels[a.owner][3] <= 0)) {
        return EffectResult::kOk;
    }
    const int denominator = db_at(self.soulMark.args.owned_int_args, 0, 2);
    // 参数在位即消费：显式0/负数无合法分母，关闸；仅缺位用文案默认2。
    if (denominator > 0) {
        (void)g_core->heal_amount(ctx, a.owner, resolve_pet_max_hp(self) / denominator);
    }
    return EffectResult::kOk;
}
}  // namespace

SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &defense_boost_heal,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
