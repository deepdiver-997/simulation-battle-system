// core_api() — 填充 Core→插件 的函数指针集合（真实原语实现地址）。
#include <plugin/core_api.h>

#include <primitives/battle_primitives.h>
#include <effects/effect_meta.h>
#include <numerical-calculation/calculation.h>

namespace {
// 插件侧窗口家族查询：取不到 meta（未收录）→ 默认 InRounds。
EffectWindowKind effect_window_kind_impl(int effect_id) {
    const EffectMeta* meta = EffectMetaCatalog::instance().find(effect_id);
    return meta ? meta->window : EffectWindowKind::InRounds;
}
}  // namespace

const CoreApi& core_api() {
    static const CoreApi api = {
        /*version=*/"1.0",
        /*apply_anomaly=*/&apply_anomaly,
        /*break_round_effects=*/&break_round_effects,
        /*clear_stat_boosts=*/&clear_stat_boosts,
        /*transfer_stat_boosts=*/&transfer_stat_boosts,
        /*heal_amount=*/&heal_amount,
        /*grant_guaranteed_first=*/&grant_guaranteed_first,
        /*stat_change=*/&stat_change,
        /*stat_drop=*/&stat_drop,
        /*seal_skill=*/&seal_skill,
        /*stat_reversal=*/&stat_reversal,
        /*stat_boost_reversal=*/&stat_boost_reversal,
        /*fixed_damage=*/&fixed_damage,
        /*effect_window_kind=*/&effect_window_kind_impl,
        /*restraint_multiplier=*/&Calculation::calculateRestraintMultiples,
        /*deal_pink_damage=*/&deal_pink_damage,
        /*pp_reduce=*/&pp_reduce,
        /*clear_stat_drops=*/&clear_stat_drops,
        /*deal_true_damage=*/&deal_true_damage,
    };
    return api;
}