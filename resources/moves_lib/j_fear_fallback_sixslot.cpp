// J 板 J262 · 害怕兜底六槽弱化族 —— 504×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 504，args_num=7）：
//   "{0}%令对手害怕，若没有触发害怕效果，则对手{1}"
//   {0}=申报概率；{1}=六槽增量表（官方语义序 攻,防,特攻,特防,速,命中；负值=降低）。
//   未触发 = 异常施加未落地（掷歪/免疫/抗性/弹控），兜底对对手逐槽施加增量。
//
// 载体切片（side_effect.arg_count=7 顺序切分，8/8 自洽）：
//   15832 黄金傲天斩 [30,-1,-1,-1,0,0,0]   17329 星风血煞击 [50,0,-1,0,0,0,-1]
//   22456 凄凉之地   [50,-1,0,-1,0,0,0]   23136 脉冲音波   [50,0,0,0,0,0,-2]
//   23822 神灵洗礼   [80,0,0,0,0,-2,-2]   34615 恐怖猎杀   [50,0,0,0,0,-1,-1]
//   37047 威慑哨音   [40,-1,0,-1,0,-2,-2]
//   34474 风雨暗影冲 [461,504]|[3,40,0,0,0,0,-1,-1] → 504=[40,0,0,0,0,-1,-1]
//   —— {1} 是**官方语义序**（攻,防,特攻,特防,速,命中）→ 引擎等级槽
//   （0攻 1特攻 2防 3特防 4速 5命中）映射 {0,2,1,3,4,5}。4399 交叉验证：
//   凄凉之地"攻击、特攻-1" = [-1,0,-1,...]（idx0/idx2）、神灵洗礼"速度、命中-2"
//   = [...,-2,-2]（idx4/idx5）、星风血煞击"防御-1，命中-1" = [0,-1,0,0,0,-1]
//   （idx1=防）——三例互证官方语义序，直读引擎序会把防/特攻互换。
//
// 实现口径：
//   - 主干 = apply_anomaly_checked **申报制**（916/J3 口径）：{0} 走概率闸门，
//     害怕(6) 走现代施加；"未触发" = !landed（含 ROLL_FAILED 与各被挡档，
//     1003/1837/1482 同判据）。
//   - 兜底 = 六槽增量：负值走 stat_drop（对手弱化，吃免弱/抗性——官方就是弱化），
//     正值走 stat_change（独立必成）；0 = 该槽不在本次表内（跳过）。
//   - 审计槽 996504100+owner：1=主句落地 / 0=未落地（场景无随机断言用）。
//
// 动工前置三查（2026-10-22）：①内核 grep 504 无行为硬编码 ②effect_info 模板唯一
//   args_num=7、custom_effect_programs/overrides 零行 ③全 worktree moves_lib/soul_lib
//   无 SKILL_EFFECT(504) 注册——真缺口。运行时 parser 不匹配本模板（"令对手害怕"
//   后无占位符）。
// 场景回归：test/scenario/scenario_5930_fear_fallback_sixslot.cpp（号段 5930-5939）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int k504LandedAuditBase = 996504100;   // +owner：1=主句落地 / 0=未落地
// 官方语义序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
constexpr int kSemanticToSlot[6] = {0, 2, 1, 3, 4, 5};

bool j262_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 504 —— {0}%令对手害怕，若没有触发害怕效果，则对手{1}（六槽）──
// int_args = [owner, target, 概率, 攻, 防, 特攻, 特防, 速, 命中]（args_num=7）。
EffectResult effect_504_fear_or_six_slot_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 504, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 504, args,
                     "504 布局=[owner,target,概率,六槽增量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     504, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0, 504, args, "需要 概率 >= 0（在位即消费）");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }

    // 主干：申报制害怕。chance=0 = 显式关闸档 → 必定未触发 → 兜底照常。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, static_cast<int>(AbnormalStatusId::Fear), random_anomaly_duration(),
        owner, chance, ChanceSource::Skill);
    const bool landed = j262_landed(r);
    ctx->plugin_storage[k504LandedAuditBase + owner] = landed ? 1 : 0;
    if (landed) {
        return EffectResult::kOk;
    }

    // 兜底：六槽增量（官方语义序 → 引擎槽）。0 = 本槽不在表内。
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[3 + i];
        if (delta < 0) {
            if (g_core->stat_drop) {
                (void)g_core->stat_drop(ctx, target, kSemanticToSlot[i], -delta, owner);
            }
        } else if (delta > 0) {
            if (g_core->stat_change) {
                (void)g_core->stat_change(ctx, target, kSemanticToSlot[i], delta);
            }
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(504, effect_504_fear_or_six_slot_drop)
