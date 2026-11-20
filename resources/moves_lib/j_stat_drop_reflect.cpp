// J 组 J13 · 下降反馈族 —— 2 宏/52 载体（2026-10-02，worktree ../agents/j13）
//
// 家族语义（effect_info 模板即权威）：全部"将自身的能力下降状态（双倍）反馈给对手"。
//   813  args_num=0 "将自身的能力下降状态双倍反馈给对手"
//   1030 args_num=1 "将自身能力下降状态反馈给对手，反馈成功则对手{0}"（{0}=异常码，申报制）
//
// 实现口径（J13 工单行 + 1009/J4 六槽循环先例）：
//   - 门槛 = 自身存在负槽（ability_levels[owner][i] < 0，六槽含命中——2026-09-18 口径）；
//     自身无下降 → 无反馈，1030 的"{0}"自然不申报。
//   - 反馈 = 逐槽 stat_drop 深度镜像：813 每槽压 2×|深度|（"双倍"），1030 压 1×|深度|。
//     弱化原语查免弱/附加禁令（actor=属主如实声明），钳 -6 由原语负责（部分生效仍算 SUCCESS）。
//   - 自身下降**不清除**（官方文本无"清除"子句，只"反馈"）。
//   - 1030"反馈成功" = ≥1 槽 stat_drop 落地 SUCCESS（对手免弱全 IMMUNE / 全在 -6 AT_FLOOR
//     → 反馈未落地 → 后缀不申报）。与 1978"未触发=未落地"（免疫/抗性都算未触发）同哲学。
//   - {0} 异常码申报制：apply_anomaly_checked 现代通道 100%（2558 同款；异常码全集 ≤43，
//     载体实见 0/2/5/6/8/10/15/20/22/23/29）。
//
// 载体切片（unity 库 json_each 精确匹配 27+25；side_effect.arg_count 切分全载体自洽、
// 零剩余，脚本核算 2026-10-02）：
//   813  全部 0 参：30046 烈焰之羽 [813]|[]、30186 裂地龙爪、30818 无尽之剑、37546 间奏清朗…
//   1030 全部 1 参：25068 碎梦吾身 [48,1030,164]|[5,6,3,100,6]→[6]害怕（4399 文本对上）、
//        30177 光陨渐影决 [1030]|[0]麻痹、26677 幻木丛生 [8]睡眠、27402 心灵相通 [20]失明、
//        37348 哀·沉静挽歌 [2103,1030,1356,1685]|[100,8,…]→[29]失神…
//
// 动工前置三查（2026-10-02）：①内核 grep include/src 零硬编码 ②custom_effect_programs/
// overrides 无声明（纯 SKILL_EFFECT 即时型）③moves_lib 无既有注册——真缺口。
//
// 场景回归：test/scenario/scenario_980_stat_drop_reflect.cpp（号段 980-989）。

#include "common.h"

namespace {

// ── 813 —— 将自身的能力下降状态双倍反馈给对手 ──
// int_args = [owner, target]（args_num=0 → 装载器注入 owner/target，1974 同款）。
EffectResult effect_813_reflect_drops_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 813, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 813, args,
                     "813 布局=[owner,target]（0 参效果，装载器注入 owner/target）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 813,
                     args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int depth = ctx->ability_levels[owner][i];
        if (depth < 0) {
            // 双倍深度镜像；弱化原语查免弱（IMMUNE 整槽不动），-6 钳制在原语内。
            (void)g_core->stat_drop(ctx, opp, i, -2 * depth, owner);
        }
    }
    return EffectResult::kOk;
}

// ── 1030 —— 将自身能力下降状态反馈给对手，反馈成功则对手{0} ──
// int_args = [owner, target, 异常码]（args_num=1）。
EffectResult effect_1030_reflect_drops_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1030, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1030, args,
                     "1030 布局=[owner,target,异常码]（int_count>=3）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1030,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1030, args,
                     "需要 异常码 ∈ [0,43]");
    // 反馈（单倍深度镜像）：任一槽落地 = 反馈成功。
    bool landed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int depth = ctx->ability_levels[owner][i];
        if (depth >= 0 || !g_core || !g_core->stat_drop) {
            continue;
        }
        if (g_core->stat_drop(ctx, opp, i, -depth, owner) == StatDropResult::SUCCESS) {
            landed = true;
        }
    }
    if (!landed) {
        return EffectResult::kOk;   // "反馈成功"门槛（免弱/全在 -6 → 未落地不申报）
    }
    if (g_core && g_core->apply_anomaly_checked) {
        // 申报制异常（{0}=载体异常码，100% 现代通道，2558 同款）。
        (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                            owner, /*chance=*/100, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(813, effect_813_reflect_drops_double)
SKILL_EFFECT(1030, effect_1030_reflect_drops_anomaly)
