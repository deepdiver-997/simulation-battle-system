// J 板 J257 · 异常门必 MISS 窗族 —— 686×9（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 686，args_num=2）：
//   "若对手{0}则使对手下{1}回合攻击必定MISS"
//   {0}=官方异常 id（0=麻痹/1=中毒/2=烧伤/5=冻伤…，见 abnormal-types.h），
//   {1}=窗口回合数（从**下回合**起算，不含施放当回合）。
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算）：
//   16790 幽藤绞杀破 [1,2]   17034 长生不灭破 [1,1]   17268 极火爆冲杀 [2,2]
//   27675 微微微辣   [2,2]   28510 冰冷目光   [5,3]   32899 琉璃净火   [2,2]
//   33342 凛风寒天爆 [5,1]   38110 净罪璃火   [2,2]
//   （16790 同时挂在 friend_side_effect，同一效果共 9 个座位 / 8 个技能。）
//   27675 官方文案佐证："对手烧伤则下2回合攻击必定MISS" + args [2,2] → {0}=异常码。
//
// 实现口径：
//   - 触发时点 = SKILL_EFFECT（技能使用成功时判门）：gate = 对手当前存在异常 {0}
//     （g_core->has_active_anomaly，J253 638/758 同款查询面；异常码越界按门闭）。
//   - 门过 → register_next_rounds_effect(ROUND_START, {1})；tick 每回合写
//     ws.hit_rate_mod[target]=0.0f（432/950 直系）。ws 每回合 reset(1.0f)，
//     窗口每回合由 tick 重写。
//   - "下{1}回合"从下一回合起（不含施放当回合）——NextRounds 起点 R+1 天然满足，
//     **不做**当回合直写（432 的"{0}回合内"双写口径不适用于本族）。
//   - 命中率层近似：hit_rate_mod 同时压属性技（官方只写攻击技），且必中技能
//     （cred.must_hit）绕过命中判定块天然穿透——与 432/78 同档，记档。
//   - 审计槽 996860100 + owner：1=门过 / 0=门闭（门闭含无异常、异常码非法）。
//
// 动工前置三查（2026-10-22）：①内核 grep 686 无行为硬编码 ②effect_info 模板唯一
//   （432/1125 同族不同文）、custom_effect_overrides/programs 零行
//   ③全库 moves_lib/soul_lib 并集（含各 worktree）无 SKILL_EFFECT(686) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5880_status_gate_miss.cpp（号段 5880-5889）。

#include "common.h"

namespace {

constexpr int k686AuditBase = 996860100;   // +owner：1=门过 / 0=门闭

// ── 686 tick：窗内每回合 ROUND_START 压对手命中率修正为 0 ──
// payload = [owner, target]。
EffectResult j257_686_miss_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[target] = 0.0f;   // 432/950 同款直压
    return EffectResult::kOk;
}

// ── 686 —— 若对手{0}则使对手下{1}回合攻击必定MISS ──
// int_args = [owner, target, 异常码, 回合数]（args_num=2）。
EffectResult effect_686_status_gate_miss(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 686, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 686, args,
                     "686 布局=[owner,target,异常码,回合数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     686, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 686, args, "需要 回合数 > 0");

    const bool gate = is_valid_abnormal_status_id(anomaly) && g_core
                      && g_core->has_active_anomaly
                      && g_core->has_active_anomaly(ctx, target, anomaly);
    ctx->plugin_storage[k686AuditBase + owner] = gate ? 1 : 0;
    if (!gate) {
        return EffectResult::kOk;   // 门闭：无异常 / 异常码非法
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 686, rounds,
                                {owner, target}, &j257_686_miss_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(686, effect_686_status_gate_miss);
