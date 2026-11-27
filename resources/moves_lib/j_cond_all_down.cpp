// J 组 J21 · 条件全属性降族 —— 2 宏/54 载体（2026-10-02，worktree ../agents/j21）
//
// 家族语义（effect_info 模板即权威）：
//   857 [4] {0}%使对手全属性-{1}，若先出手则{2}%使对手全属性-{3}
//   577 [2] 自身处于能力提升状态时，使对手{0}能力{1}（切片=[槽位码,负增量]）
//
// 实现口径：
//   - 857 = acting_side_is_first 分支（common.h，980/52 同判据）：先手走 {2}%-{3} 档、
//     后手走 {0}%-{1} 档；掷点 roll_percent（850 口径，弱化无亮节闸门管辖面）；
//     "全属性" = 六槽循环 stat_drop（938/1009 同款弱化原语：查免弱/附加禁令，
//     actor=属主如实声明；原语内钳 -6）。
//   - 577 = count_boost_kinds 同判据门（batch_a/batch_c 助手同名复刻：任一正槽即
//     "处于能力提升状态"，六槽含命中）→ stat_drop 单槽 {0}（{1} 负增量取绝对值）。
//     槽位码实载 3/4/5（特防/速度/命中）。
//
// 载体切片（unity 库 json_each 精确匹配 32+22=54；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   857  全部 [50,1,100,1]：后手 50% 全属性-1 / 先手 100% 全属性-1
//        （32471 蝎毒魔针、26972 月汐之氲 属性技、36594 钳坤一掷…）
//   577  [槽位码,负增量]：15677 沙瀑之舞 [5,-1]、34005 残影轻拂 [5,-3]、
//        19873 余音渺渺 [4,-2]、38546 和平之歌 [3,-2]
//
// 动工前置三查（2026-10-02）：①内核 grep 2 id 无硬编码 ②custom_effect_programs/
// overrides 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1060_cond_all_down.cpp（号段 1060-1069）。

#include "common.h"

#include <cstdlib>

namespace {

// "自身处于能力提升状态"计数（batch_a/batch_c count_boost_kinds 同判据：含命中 6 槽）。
inline int j21_count_boost_kinds(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 857 —— {0}%使对手全属性-{1}，若先出手则{2}%使对手全属性-{3} ──
// int_args = [owner, target, chance, amount, chance_first, amount_first]（args_num=4）。
EffectResult effect_857_conditional_all_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 857, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 857, args,
                     "857 布局=[owner,target,概率,级数,先手概率,先手级数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     857, args, "需要 owner/target ∈ {0,1}");
    const bool first = acting_side_is_first(ctx);
    const int chance = first ? args.int_args[4] : args.int_args[2];
    const int amount = first ? args.int_args[5] : args.int_args[3];
    EFFECT_ARG_GUARD(chance > 0 && amount > 0, 857, args, "需要 概率/级数 > 0");
    if (!roll_percent(ctx->rng(), chance)) {
        return EffectResult::kOk;   // 掷点失败（850 口径：>=100 短路必中）
    }
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, amount, owner);   // 全属性六槽，弱化原语
    }
    return EffectResult::kOk;
}

// ── 577 —— 自身处于能力提升状态时，使对手{0}能力{1} ──
// int_args = [owner, target, slot_code, delta]（args_num=2）。
EffectResult effect_577_boost_gate_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 577, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 577, args,
                     "577 布局=[owner,target,槽位码,负增量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slot = args.int_args[2];
    const int magnitude = -args.int_args[3];   // 载体恒负增量
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     577, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 577, args,
                     "需要 槽位码 ∈ [0,5]");
    EFFECT_ARG_GUARD(magnitude > 0, 577, args, "需要 增量 ≠ 0（载体恒负）");
    if (j21_count_boost_kinds(ctx, owner) <= 0) {
        return EffectResult::kOk;   // 门槛：自身处于能力提升状态
    }
    if (g_core && g_core->stat_drop) {
        (void)g_core->stat_drop(ctx, target, slot, magnitude, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(857, effect_857_conditional_all_down)
SKILL_EFFECT(577, effect_577_boost_gate_drop)
