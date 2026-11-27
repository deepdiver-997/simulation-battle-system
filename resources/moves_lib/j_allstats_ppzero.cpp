#include <utils/sbs_rand.h>
// J 组 J71 · 全属性/PP归零族 —— 2 宏/32 载体（2026-10-03，worktree ../agents/j71）
//
// 家族语义（effect_info 模板即权威）：
//   1145 [1] 令双方全属性+{0}
//   912  [1] 若对手处于能力下降则使对手随机{0}个技能的PP归零
//
// 实现口径：
//   - 1145 = all_stats_up 双侧（common.h 同款六槽含命中，g_core->stat_change 真原语
//     本体/视图一并同步；966 兜底/J1 1234 反向先例）。int_args=[owner,target,up]。
//   - 912 = 负槽门（J63 431 同判据：对手六槽任一 <0）→ 随机 {0} 个技能 PP 归零
//     （1114 同款：候选=pp>0 槽、sbs_rand 不重复抽取、直写 pp=0）。门关 → 无操作。
//
// 载体切片（unity 库 json_each 精确匹配 16+16 计账；两族无纯链——场景走"兄弟全实现
// 且良性"链）：
//   1145 [up]：37293 [1145,843]|[1,2,2]（843=自身先制+2，良性；物150 攻击技）
//   912  [count]：纯链 28377 噤灵食心 [3]（属性技，PP10）/35348 黄昏践踏 [1]
//
// 动工前置三查（2026-10-03）：①内核 grep 1145/912 无硬编码 ②custom_effect_overrides
// 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1580_allstats_ppzero.cpp（号段 1580-1589）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include <cstdlib>
#include <vector>

namespace {

// 对手负槽判据（J63 431 同款六槽任一 <0）。
bool j71_foe_dropped(const BattleContext* ctx, int target) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[target][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1145 —— 令双方全属性+{0} ──
// int_args = [owner, target, up]（args_num=1）。
EffectResult effect_1145_both_all_stats_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1145, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1145, args,
                     "1145 布局=[owner,target,提升]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int up = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1145, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(up > 0, 1145, args, "需要 提升 > 0");
    all_stats_up(ctx, owner, up);
    all_stats_up(ctx, target, up);
    return EffectResult::kOk;
}

// ── 912 —— 若对手处于能力下降则使对手随机{0}个技能的PP归零 ──
// int_args = [owner, target, count]（args_num=1）。
EffectResult effect_912_drop_gate_zero_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 912, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 912, args,
                     "912 布局=[owner,target,项数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     912, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count > 0, 912, args, "需要 项数 > 0");
    if (!j71_foe_dropped(ctx, target)) {
        return EffectResult::kOk;   // 门：对手处于能力下降状态
    }
    ElfPet& opp_pet = ctx->getPet(target);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(opp_pet.skills.size()); ++i) {
        if (opp_pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    for (int k = 0; k < count && !candidates.empty(); ++k) {
        const int pick = choose_skill_range(*ctx, 912, 0, owner, target, 0, static_cast<int>(candidates.size()) - 1);
        opp_pet.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
        candidates.erase(candidates.begin() + pick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1145, effect_1145_both_all_stats_up)
SKILL_EFFECT(912, effect_912_drop_gate_zero_pp)
