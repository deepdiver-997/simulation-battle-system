// J182 提升态概率异常族 —— effect 1227（1 宏 / 10 技）
//                                                                          （2026-10-07，worktree ../agents/j182）
//
// 效果模板（effect_info 1227，args_num=2）：
//   "自身处于能力提升状态则有{0}%概率使对手{1}，先出手时概率翻倍"。
//
// 载体切片（args_num 逐位锚定，2026-10-07）：
//   33881 淬锻焚灭焰 [1381,1227,669] args[3, 40,8] → 1227=[40,8]=40%睡眠
//     （1381=[3]消回合成功反弹窗、669 零参击败先制）
//   34523 无念无忧散 [1227,837,1587] args[40,8, 150,100,350, 1] → 1227=[40,8]=40%睡眠
//     （837=[150,100,350] 递增吸取、1587=[1] 击败先制——未接静默）
//   34627 无间堕落斩 [1220,1227,910] args[2, 40,23, 2] → 1227=[40,23]=40%诅咒
//     （1220=[2] 消强必爆窗、910=[2] 击败封属窗）
//
// 实现：SKILL_EFFECT 时点——count_boost 判据（本体 ability_levels 六槽任一正等级，
//   962/1472 同款，负等级不算）→ 无提升短路；先手 pct×2 封 100（1472"翻倍"口径的
//   概率版：单掷、翻倍后封顶）单次掷点 → apply_anomaly_checked(db[1],
//   random_anomaly_duration(), owner, pct, ChanceSource::Skill)（F7 范式：概率申报给
//   原语，内核单掷、亮节管辖——禁本地 roll 双掷）。
//
// 三查记录（2026-10-07）：①内核 grep 1227 无硬编码；②effect_info 模板即权威；
//   ③registry.moves 无 1227、moves_lib 无 1227——真缺口。
// 先例：f_order_family 1472（boost 判据+先手翻倍）、anomaly_channel 109（F7 概率申报）、
//   K164 116（异常码走 db）。
// 场景回归：test/scenario/scenario_3415_boost_cond_anomaly.cpp（T3415-T3417）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// "自身处于能力提升状态"计数（962/1472 同判据：本体正等级六槽含命中，负等级不算）。
int count_boost_kinds_j182(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

EffectResult effect_1227_boost_cond_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1227, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 1227, args, "需要 [owner,target,pct,anomaly]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1227, args,
                     "owner/target 越界");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (count_boost_kinds_j182(ctx, owner) <= 0) {
        return EffectResult::kOk;   // 无提升状态：不掷不施加
    }
    int effective = pct;
    if (acting_side_is_first(ctx)) {
        effective = std::min(100, pct * 2);   // 先手：概率翻倍（封 100 单掷）
    }
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                        owner, effective, ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1227, effect_1227_boost_cond_anomaly);
