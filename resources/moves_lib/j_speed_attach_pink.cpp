// J257 · 速度面板附加伤族 —— 115×6（2026-10-24，worktree ../agents/j257）
//
// 家族语义（effect_info 模板 + 4399 载体文本交叉定标）：
//
// 115（args_num=2）"{0}%概率附加速度的1/{1}伤害"：
//   - {1} 是**自身速度面板值的分母**：飓风利袭 12079 [15,3]="15%几率附加速度的1/3伤害"、
//     疾风闪 15221 [100,8]="附加自身速度值1/8的固定伤害"、巨翼疾风破 15861 [100,4]
//     ="附加1/4自身速度值的伤害"、哗啦冰雹/迅羽击 [100,3]="100%几率附加速度的1/3伤害"
//     ——四文本全吻合（{0}=触发概率、{1}=速度/1/{1} 分母）。
//   - "附加…固定伤害" = 粉伤（DamageKind::FIXED，吃护罩不吃护盾——速查伤害类型表），
//     打对手，量 = floor(速度面板 / {1})。
//   - 触发条件 = 本次攻击**命中**（默认 HIT 分支注册，miss/被封不注册——内核分支语义
//     白拿，无需自判）；"附加"时机 = 攻击伤害落地后（SKILL_EFFECT 桶）。
//
// 实现口径：
//   - SKILL_EFFECT 本体（默认 HIT 分支）：
//     速度面板读 ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPEED)
//     （视图槽含能力等级，J294 1105 攻面板同口径——提速/降速效果会联动，记档为
//     "面板视图"口径）；pink = speed / {1}（整除）；roll_percent({0})（common.h 助手）
//     → g_core->deal_pink_damage(ctx, opp, pink, DamageKind::FIXED, owner)。
//   - 概率审计槽（F9 782 / J146 口径）：申报概率与计算粉伤在掷骰前落槽，场景对
//     概率/粉伤做无随机精确断言；100% 载体（15221/15861/哗啦冰雹/迅羽击）另配
//     行为断言（粉伤支配 hp 差带）。
//
// plugin_storage 审计槽（grep 全库无撞）：
//   990115000 + owner = 本次申报概率（{0}）
//   990115100 + owner = 本次计算粉伤（speed/{1} 整除；掷骰前落槽，条件恒真即值）
//
// 动工前置三查（2026-10-24，worktree ../agents/j257）：
//   ①内核 grep 115 无行为硬编码（990115 数字撞见是旧场景 source_id，非键空间）
//   ②effect_info 115 args_num=2 与 side_effect.arg_count=2 一致、custom 表零行
//   ③moves_lib/soul_lib 全库无 SKILL_EFFECT(115) 注册、parser 不接（非状态主句/
//     非五类条件模板）——真缺口。
//   载体切片（side_effect.arg_count 优先，脚本核算 2026-10-24）：
//     纯链×4：飓风利袭 12079 [15,3] / 灵能飓风破 14708 [15,3] / 疾风闪 15221 [100,8] /
//             巨翼疾风破 15861 [100,4]
//     混链×2：哗啦冰雹 36983 [115,1256]|[100,3]（1256 已实现）
//             迅羽击 37330 [115,968]|[100,3]（968 已实现）
//   混链兄弟全 implemented → 全载体链激活，场景断言只钉 115 半边。
//   真实精灵覆盖 14 只。
// 先例：j_attack_panel_pink.cpp J294 1105（面板取数 getTempAbilityValue + deal_pink_damage
//   + 三审计槽）、common.h roll_percent、chaos_sorensen（DamageKind::FIXED 粉伤）。
// 场景回归：test/scenario/scenario_7370_speed_attach_pink.cpp（号段 7370-7379）。

#include "common.h"

#include <entities/numerical-properties.h>

namespace {

constexpr int k115ProbBase = 990115000;  // +owner：本次申报概率
constexpr int k115PinkBase = 990115100;  // +owner：本次计算粉伤（掷骰前）

// ── 115 —— {0}%概率附加自身速度值 1/{1} 的固定伤害 ──
// int_args = [owner, target, {0}=概率, {1}=分母]（args_num=2；默认 HIT 分支）。
EffectResult effect_115_speed_attach_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 115, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 115, args,
                     "115 布局=[owner,target,概率,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int prob = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 115,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(prob > 0 && prob <= 100, 115, args, "需要 概率 ∈ (0,100]");
    EFFECT_ARG_GUARD(denom > 0, 115, args, "需要 分母 > 0");

    // 速度面板（视图槽，含能力等级——J294 1105 攻面板同口径）
    const int speed = ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPEED);
    const int pink = speed / denom;
    ctx->plugin_storage[k115ProbBase + owner] = prob;
    ctx->plugin_storage[k115PinkBase + owner] = pink;

    if (pink > 0 && roll_percent(prob)) {
        if (!g_core || !g_core->deal_pink_damage) {
            return EffectResult::kOk;
        }
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(115, effect_115_speed_attach_pink)
