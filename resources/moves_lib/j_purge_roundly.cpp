// J 组 J19 · 消强/吸反族三 —— 2 宏/50 载体（2026-10-02，worktree ../agents/j19）
//
// 家族语义（effect_info 模板 + 4399 逐字核对）：
//   759 [3] 消除对手能力上升状态，消除成功则{0}回合内对手每回合{1}{2}
//       —— {1}=槽位码（**官方语义序**，4399 四载体钉死）：1=防御、3=双防、4=速度、
//          5=命中（2=特防无载体，按序补全）；{2}=级数（负值，取绝对值下放）。
//   978 [0] 吸取并反转对手能力提升状态
//
// 实现口径：
//   - 759 = J2 消强门（clear_stat_boosts(target)>0，STAT_CLEAR 免消除挡下返回 0 不触发）
//     + 每回合窗 tick（950/532 双写纪律的结构，但本效果是**累加型**弱化、非幂等写：
//     ⚠️ 立即写只在**窗口含当回合**时做——round_effect_start_round 对后手顺延折算
//     （InRounds 起点=R+1），若后手也立即写会多落一次（rounds+1 次）；先手立即写覆盖
//     当回合 + ROUND_START tick 覆盖其余回合 = 恰 {0} 次）。每回合走 stat_drop 真原语
//     （对手施加的弱化：查免弱/附加禁令，1009 六槽循环同款；actor=属主）。
//   - 978 = 1061 成功路径（burn_trio 先例）：快照对手正等级 → stat_boost_reversal 反转
//     → REVERSED 则自身逐槽 stat_change 获得等量提升（"吸取"）。
//     ⚠️ 记档：978 模板无失败子句（1061 失败→消除；978 失败 → 无事发生）。
//     雷鸣审判 4399 原文"吸收…同时反转…"与该实现吻合（自身得等量+对手变下降）。
//
// 载体切片（unity 库 json_each 精确匹配 27+23=50；side_effect.arg_count 切分自洽；
// 37944 极光千刃链尾余 1 参=兜余位（1037 同款），19603 阵列含两次 759=官方数据原样）：
//   759  [rounds,槽位码,级数]：35106 时分重合 [3,4,-2]（速-2×3回合，测试载体）、
//        34495 毛球急冻冰 [2,1,-2]（防-2）、19603 恶梦之魇 [2,3,-1]（双防-1）、
//        17299 王焚天君焱 [2,5,-2]（命中-2）
//   978  全部 0 参：17977 雷鸣审判、19565 天启凝心斩、36465 合十…
//
// 动工前置三查（2026-10-02）：①内核 grep 2 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1040_purge_roundly.cpp（号段 1040-1049）。

#include "common.h"

namespace {

// {1} 槽位码 → 引擎能力槽（官方语义序映射；477 六槽/PLD kStatMap 同族口径）。
// 4399 实证：1=防御、3=双防（防御+特防两槽）、4=速度、5=命中；2=特防按序补全。
void j19_roundly_drop(BattleContext* ctx, int target, int slot_code, int magnitude,
                      int actor) {
    if (!ctx || !g_core || !g_core->stat_drop || magnitude <= 0) {
        return;
    }
    int slots[2] = {-1, -1};
    switch (slot_code) {
        case 1: slots[0] = 2; break;            // 防御
        case 2: slots[0] = 3; break;            // 特防
        case 3: slots[0] = 2; slots[1] = 3; break;   // 双防
        case 4: slots[0] = 4; break;            // 速度
        case 5: slots[0] = 5; break;            // 命中
        default: return;                         // 未知槽位码（记档：全库无实例）
    }
    for (const int slot : slots) {
        if (slot >= 0) {
            (void)g_core->stat_drop(ctx, target, slot, magnitude, actor);
        }
    }
}

// 759 的 ROUND_START tick：payload = {owner, target, slot_code, magnitude}。
EffectResult j19_759_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    j19_roundly_drop(ctx, args.int_args[1], args.int_args[2], args.int_args[3],
                     args.int_args[0]);
    return EffectResult::kOk;
}

// ── 759 —— 消除对手能力上升状态，消除成功则{0}回合内对手每回合{1}{2} ──
// int_args = [owner, target, rounds, slot_code, magnitude]（args_num=3）。
EffectResult effect_759_purge_then_roundly_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 759, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 759, args,
                     "759 布局=[owner,target,回合,槽位码,级数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int slot_code = args.int_args[3];
    const int magnitude = -args.int_args[4];   // 载体恒负值，取绝对值下放
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     759, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 759, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(magnitude > 0, 759, args, "需要 级数 ≠ 0（载体恒负）");
    if (!g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    // J2 消强门：清掉个数 >0 才触发（1217/766 先例；STAT_CLEAR 免消除挡下=0）。
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;
    }
    // 每回合窗：InRounds {0} 回合。累加型弱化非幂等——立即写只在窗口含当回合时做
    //（后手由 round_effect_start_round 顺延折算，立即写会多落一次，见文件头）。
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    if (start == ctx->roundCount) {
        j19_roundly_drop(ctx, target, slot_code, magnitude, owner);
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 759, rounds,
                          {owner, target, slot_code, magnitude}, &j19_759_round_tick);
    return EffectResult::kOk;
}

// ── 978 —— 吸取并反转对手能力提升状态 ──
// int_args = [owner, target]（args_num=0，装载器注入）。
EffectResult effect_978_drain_and_reverse(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 978, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 978, args,
                     "978 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     978, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    // 快照对手正等级（1061 同款：stat_change 写入前先抄，反转后逐槽补给自身）。
    int boosted[BattleContext::kAbilityLevelSlotCount]{};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        boosted[i] = std::max(0, ctx->ability_levels[target][i]);
    }
    if (g_core->stat_boost_reversal(ctx, target) != StatReversalResult::REVERSED
        || !g_core->stat_change) {
        return EffectResult::kOk;   // 反转失败（免弱挡下/无提升可反）→ 无后缀子句，记档
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (boosted[i] > 0) {
            g_core->stat_change(ctx, owner, i, boosted[i]);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(759, effect_759_purge_then_roundly_drop)
SKILL_EFFECT(978, effect_978_drain_and_reverse)
