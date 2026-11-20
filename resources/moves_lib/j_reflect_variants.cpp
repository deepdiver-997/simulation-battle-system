// J 组 J80 · 弹控变体族 —— 1932×16 / 1687×16（2026-10-03，worktree ../agents/j80）
//
// 家族语义（effect_info 官方模板即权威）：
//   1932（arg_count=2）"{0}回合内免疫并反弹除{1}外所有受到的异常状态"
//   1687（arg_count=1）"{0}回合内免疫并反弹所有受到的控制类异常状态"
//
// 实现口径（191 直系，anomaly_channel 同款双授予）：
//   - 免疫半 = grant_immunity(ANOMALY, anomaly_mask=按位, rounds, ON_STAGE, tier=Modern)
//     ——mask 位语义（rule_center.h:553 消费端）：**置位=免疫该 status_id**，mask==0
//     才是全免。1932 排除 {1} → mask = ~(1ULL<<{1})（其余全置位）；1687 只免疫
//     控制类 → mask = 遍历 0..kOfficialAbnormalStatusMaxId 收集
//     is_control_abnormal_status(id) 的位集（表驱动，不硬编码 id 清单）。
//   - 反弹半 = rule_center_.grant_reflect（无 mask 参——消费端 apply_anomaly_impl
//     的 reflect() 只在**免疫挡下后**触发（[3] 免疫命中 → return reflect()），
//     mask 天然同时管住"免疫"与"反弹"两半：{1}/非控制类照常落地不弹。
//   - Modern 层（191 同款）：古早主动毒照穿（"弹控不能免疫主动毒"官方口径）。
//
// 载体切片（unity 库 json_each 精确匹配 16/16=32；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1932 [回合,排除码]：{4,16}×3/{4,5}×2/{4,32}×2/{5,0}/{5,9}/…（排除码全集含 0）
//     （场景载体 水晶之匿 28249 [1783✗,1932,1039✓]|[5,2] 属——1783 未实现哑）
//   1687 [回合]：{4}×14/{3}×2（场景载体 灵威神谕 36555 [1687,878✗]|[3,3] 物——
//     878 未实现哑，近纯链）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1932/1687 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：191（免疫+reflect 双授予 Modern 层）、rule_center.h:553（mask 位语义）、
//   apply_anomaly_impl reflect()（免疫挡下后触发，mask 同管两半）。
// 场景回归：test/scenario/scenario_1700_reflect_variants.cpp（1700 号段，一文件两宏）。

#include "common.h"

namespace {

constexpr int kOfficialAnomalyMax = 43;

// 1932 —— {0}回合内免疫并反弹除{1}外所有受到的异常状态
// int_args = [owner, target, rounds, exclude]（args_num=2）。
EffectResult effect_1932_reflect_except_one(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1932, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1932, args,
                     "1932 布局=[owner,target,回合,排除码]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int exclude = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1932, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1932, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(exclude >= 0 && exclude <= kOfficialAnomalyMax, 1932, args,
                     "需要 排除码 ∈ [0,43]");
    // 掩码：除排除码外全置位（置位=免疫该 id；消费端 rule_center.h:553）。
    const uint64_t mask = ~(1ULL << exclude);
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/mask, /*duration_rounds=*/rounds,
                        /*source_id=*/1932, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/1932, /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1,
                                    /*source_effect_id=*/1932, /*target=*/owner,
                                    /*rounds=*/rounds);
    return EffectResult::kOk;
}

// 1687 —— {0}回合内免疫并反弹所有受到的控制类异常状态
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_1687_reflect_control_only(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1687, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1687, args,
                     "1687 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1687, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1687, args, "需要 回合 > 0");
    // 控制类位集表驱动收集（is_control_abnormal_status 是唯一权威，不硬编码清单）。
    uint64_t mask = 0;
    for (int id = 0; id <= kOfficialAnomalyMax; ++id) {
        if (is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            mask |= 1ULL << id;
        }
    }
    EFFECT_ARG_GUARD(mask != 0, 1687, args, "控制类位集为空——表异常");
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/mask, /*duration_rounds=*/rounds,
                        /*source_id=*/1687, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/1687, /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1,
                                    /*source_effect_id=*/1687, /*target=*/owner,
                                    /*rounds=*/rounds);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1932, effect_1932_reflect_except_one)
SKILL_EFFECT(1687, effect_1687_reflect_control_only)
