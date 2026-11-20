// J 组 J176 · 连击递增概率族 —— 181×12（2026-10-04，worktree ../agents/j176）
//
// 家族语义（effect_info 官方模板即权威）：
//   181 [4] {0}%概率令对手{1}，连续攻击每次提高{2}%概率，最多提高{3}%
//
// 实现口径：
//   - ⚠️ 勘误（本单实测）：看板行原路线 combo_hit_hook per-hit 递增——12 载体
//     moves.atk_num **全 0（单击技）**，逐段钩子无段可挂。"连续攻击每次提高"实为
//     **连用递增**（696/795/2396 家族口径）：plugin_storage 每用 +1 →
//     chance = min({0} + {2}×(uses-1), {3}) → apply_anomaly_checked 申报制。
//     命中门 = skill_exec_result==HIT（miss 不计用）。
//
// 载体切片（unity 库 json_each 精确匹配 12/12；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-04）：181 [概率,码,步长,上限]：
//   {15,0,5,20}/{15,7,5,40}/{20,6,10,100}/{20,6,5,50}/… 纯链×4（炽焰奔雷爪 12994
//   [15,0,5,20]——码 0 避场景；极限连爪破 13509 [15,7,5,40] 码 7 疲惫可用）。
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 181
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：795/2396/696（连用计数递增封顶，e_power_crit_ramp）、118（威力域随机）、
//   apply_anomaly_checked 申报制（J3 形）、命中门（J23 36 口径）。
// 场景回归：test/scenario/scenario_2780_combo_escalating.cpp（2780 号段）。

#include "common.h"

namespace {

constexpr int k181UsesBase = 990181000;   // +owner：连用计数（795/2396 口径）

// ── 181 —— {0}%概率令对手{1}，连续攻击每次提高{2}%，最多提高{3}% ──
// int_args = [owner, target, chance, anomaly, step, cap]（args_num=4）。
EffectResult effect_181_combo_escalating(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 181, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 181, args,
                     "181 布局=[owner,target,概率,异常码,步长,上限]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int step = args.int_args[4];
    const int cap = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     181, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && step > 0 && cap > 0, 181, args,
                     "需要 概率/步长/上限 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 181, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中"门（miss 不计用）
    }
    const int uses_key = k181UsesBase + owner;
    const auto it = ctx->plugin_storage.find(uses_key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[uses_key] = uses;
    const int chance_t = std::min(chance + step * (uses - 1), cap);
    // 审计槽（F9 782 口径，990181100+owner）：落本次申报概率×100——场景对概率梯度
    // 做无随机精确断言（原 J148 审计槽实现退役后由本文件承接）。
    ctx->plugin_storage[static_cast<unsigned long long>(990181100 + owner)] = chance_t * 100;
    (void)g_core->apply_anomaly_checked(ctx, args.int_args[1], anomaly,
                                        random_anomaly_duration(), owner, chance_t,
                                        ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(181, effect_181_combo_escalating)
