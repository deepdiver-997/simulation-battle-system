// J180 消强成功升级族 —— effect 430（1 宏 / 3 技）
//                                                                          （2026-10-07，worktree ../agents/j180）
//
// 载体与切片（side_effect/side_effect_arg，2026-10-07）：
//   37798 竭力大切割 [430,427,1395] args[0,2, 3,-1] → 430 取 [0,2]=自身攻+2
//     （427 取 [3,-1]=3 回合双防-1 窗、1395 零参先手必暴）
//   38279 破风头槌 [430,1472,709] args[0,2, 30] → 430 取 [0,2]=自身攻+2
//     （1472 取 [30]、709 零参）
//   38427 半梦 [430,155] args[2,2, 1] → 430 取 [2,2]=自身防+2（槽2=防；155 取
//     [1]=睡眠1回合）⚠️ 立单行误写"特防"——槽序 0攻1特攻2防3特防，2=防，以本记档对账
//   切片互锚：430 args_num=2、1472/155 args_num=1 与实参列数自洽（R1 切片口径）。
//
// 实现：SKILL_EFFECT 时点——clear_stat_boosts(opp)<=0（无提升/被免消除强化 STAT_CLEAR
//   挡下）→ 短路不给（1011"消除成功则"同款门）；成功 → stat_change(own, db[0], db[1])
//   真原语（186 同款：本体+视图一并同步，速度档参与先手权比较）。
//   槽序 = 能力等级槽 0攻 1特攻 2防 3特防 4速 5命中。
//
// 链核查（2026-10-07）：三载体链上其余效果 427/155/709/1395/1472 registry 全 ✓——
//   **链全接、无哑效果**，场景按"链是活的"钉断言无并行激活陷阱（常识 §五）。
// 三查记录：①内核 grep 430 无硬编码；②effect_info 430 模板即权威
//   （"消除对手能力提升状态，若消除状态成功，则自身{0}等级+{1}"）；③registry.moves
//   无 430、moves_lib 无 430——真缺口。
// 先例：chaos_sorensen 1011（clear_stat_boosts 成功门）、f_order_family 186
//   （stat_change 真原语 + EFFECT_ARG_GUARD 形）。
// 场景回归：test/scenario/scenario_3345_dispel_boost_stat_up.cpp（T3345-T3347）。

#include "common.h"

namespace {

EffectResult effect_430_dispel_boost_stat_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 430, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 430, args, "需要 [owner,target,slot,levels]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slot = args.int_args[2];
    const int levels = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 430, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 430, args,
                     "等级槽越界（0攻 1特攻 2防 3特防 4速 5命中）");
    if (!g_core || !g_core->clear_stat_boosts || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 消除失败（无提升/被免消除挡下）→ 不给升级
    }
    (void)g_core->stat_change(ctx, owner, slot, levels);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(430, effect_430_dispel_boost_stat_up);
