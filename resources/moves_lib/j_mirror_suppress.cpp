#include <utils/sbs_rand.h>
// J76 · 提升镜像压制族 —— 1245×17（2026-10-03，worktree ../agents/j76）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1245（args_num=1）"若自身处于能力提升状态则附加给对手等同于自身能力提升状态的
//   能力下降状态，若未满足或未触发则令对手随机{0}项技能PP值归零"：
//   - 门 = 自身任一正槽（J14 助手同判据）；
//   - 镜像 = 自身正槽逐项 stat_drop(对手, 同槽, 同幅度)（**查免弱**，J48 负值分支
//     同款；"等同于"= 幅度 1:1，AT_CAP 对称——不放大）；
//   - "未满足"（自身无正槽）"或未触发"（镜像全被免弱/已在 -6 挡下，无一次 SUCCESS）
//     → pp_zero_slot 随机 {0} 项 PP 归零（原语逐槽发事件，2026-09-23 批 8 修正线；
//     候选槽 0..4 洗牌取前 {0}，pp==-1 的无限槽由原语 INVALID_PARAM 兜底跳过）。
//   - 实例：魔君相助 25690 [191,47,1245]|[4,4,2]（属性技，191/47 自侧票良性）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1245 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1245=1 与 effect_info.args_num 一致；17 载体切片自洽（[随机项数]）。
// 先例：J48（stat_drop/stat_change 分支）、J14（正槽判据）、J58（pp 原语通道）、
//   battle_primitives 769（pp_zero_slot 族专用原语）。
// 场景回归：test/scenario/scenario_1660_mirror_suppress.cpp（1660 号段）。

#include "common.h"

#include <cstdlib>
#include <utility>

namespace {

// 自身任一正槽（J14 助手同判据；匿名空间不跨 TU，就地重写）。
bool j76_has_any_stat_up(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 1245 —— 自身有提升 → 对手等量下降（逐槽镜像）；未满足/未触发 → 随机 {0} 项 PP 归零 ──
// int_args = [owner, target, zero_count]（args_num=1）。
EffectResult effect_1245_mirror_suppress(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1245, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1245, args,
                     "1245 布局=[owner,target,zero_count]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int zero_count = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1245, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(zero_count > 0, 1245, args, "需要 zero_count>0");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (j76_has_any_stat_up(ctx, owner)) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int lv = ctx->ability_levels[owner][i];
            if (lv <= 0) {
                continue;
            }
            // 逐槽镜像查免弱；只有 SUCCESS 算"触发"（AT_FLOOR=已在 -6 无法再降=未触发）
            if (g_core->stat_drop(ctx, target, i, lv, owner) == StatDropResult::SUCCESS) {
                landed = true;
            }
        }
    }
    if (landed) {
        return EffectResult::kOk;
    }
    // 未满足 / 未触发 → 随机 {0} 项技能 PP 归零
    if (!g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    int slots[5] = {0, 1, 2, 3, 4};
    for (int i = 4; i > 0; --i) {
        std::swap(slots[i], slots[choose_skill_range(*ctx, 1245, 0, owner, target, 0, i)]);
    }
    int picked = 0;
    for (int i = 0; i < 5 && picked < zero_count; ++i) {
        if (g_core->pp_zero_slot(ctx, target, slots[i], owner) == PpReduceResult::SUCCESS) {
            ++picked;
        }
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1245, effect_1245_mirror_suppress)
