// E9 · 天敌/条件增伤族批产（2026-09-29，工单：docs_local/docs/05-任务清单/E组批量效果工单.md E9）
//
// 家族成员（effect_info 模板 × args_num）：
//   428（1 参 [n]）   "遇到天敌时附加{0}点固定伤害"
//       —— 银河星爆 13534 [50]、天霸狂龙吼 14245 [50]、灭世魔龙波 14738 [100]、镜水击 14891
//          [200] 等 49 技。
//   188（0 参）       "若对手处于异常状态，则威力翻倍并消除对手相应的防御能力提升效果"
//       —— 极限冰结 13068、天罗龙影闪 15305、魅影极乐爪 15866、万圣夜王斩 16467 等 44 技。
//   1020（1 参 [n]）  "全属性+{0}，对手不处于能力提升状态时强化效果翻倍"
//       —— 笑傲光阴 25044 [1]、万峰矗立 25242 [1]、暗黑壁垒 25721 [1] 等 39 技。
//
// 动工三查（2026-09-29）：内核 skills.cpp/battle_primitives.cpp/EffectMetaCatalog 无
// 428/188/1020 硬编码；moves_lib 无既有 SKILL_EFFECT —— 真缺口。
//
// 落点：
// ★ 428 = D6 1158 即时档同款：SKILL_EFFECT 时点 `is_predator`（common.h 助手，固有属性
//   克制判，非技能系别）成立 → `deal_pink_damage(FIXED)` 附加固伤。"附加"打在攻击技能上，
//   与 1158 的固伤 rider 同刻同时点（即时档，774 同口径记档）。
// ★ 188 = 威力翻倍半边走 96 破魂枪同款变威力通道：`skill_power_view.value × 2`（引擎在
//   resolve 时已按面板 materialize 打底，SKILL_EFFECT 时点 view.value>0 可直接乘；
//   ATTACK_DAMAGE 按新威力重算覆盖）。条件 = `has_any_active_abnormal_status(对手)`
//   （1048 同判据）。
//   ⚠️ 消除子句跳过（记档待内核线）："消除对手**相应的防御**能力提升"需要 per-stat 消除
//   原语——现装 clear_stat_boosts 是全槽（会把攻击提升一起误清）；直写 ability_levels
//   走 write_ability_level 唯一落点不可达（本体/视图分裂，common.h all_stats_up 教训）。
// ★ 1020 = 1991/1178 同款条件翻倍：`all_stats_up(ctx, owner, n)`，对手任一槽正等级存在
//   则不翻倍。条件读对手 6 槽能力等级现值（SKILL_EFFECT 时点）。
#include "common.h"

#include <algorithm>

namespace {

// 对手是否存在任一能力提升（6 槽正等级；1178 的 count_boost_kinds 判据同源）。
bool opponent_has_any_boost(const BattleContext* ctx, int opp) {
    if (!ctx || opp < 0 || opp > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 428 —— 遇到天敌时附加{0}点固定伤害 ────────────────────────────────────────
EffectResult effect_428_predator_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 428, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 428, args, "需要 [owner,target,n]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int n = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 428, args,
                     "owner/target 越界");
    if (n <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    if (!is_predator(ctx, owner)) {
        return EffectResult::kOk;   // 非天敌：不附加
    }
    (void)g_core->deal_pink_damage(ctx, target, n, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 188 —— 若对手处于异常状态，则威力翻倍（消除子句跳过，见文件头记档）─────────
EffectResult effect_188_anomaly_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 188, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 2, 188, args, "需要 [owner,target]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 188, args,
                     "owner/target 越界");
    if (!ctx->has_any_active_abnormal_status(opp)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;   // 96 破魂枪同款：视图威力 ×2（变威力重算覆盖首次结算）
    }
    return EffectResult::kOk;
}

// ── 1020 —— 全属性+{0}，对手不处于能力提升状态时强化效果翻倍 ──────────────────
EffectResult effect_1020_all_up_cond_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1020, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1020, args, "需要 [owner,target,n]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int n = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1020, args,
                     "owner/target 越界");
    if (n <= 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, opponent_has_any_boost(ctx, opp) ? n : n * 2);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(428, effect_428_predator_fixed);
SKILL_EFFECT(188, effect_188_anomaly_power_double);
SKILL_EFFECT(1020, effect_1020_all_up_cond_double);
