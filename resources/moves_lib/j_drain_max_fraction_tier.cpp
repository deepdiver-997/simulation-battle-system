// J 组 J227 · 吸取最大体力分档族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1263 [2] 吸取对手最大体力的1/{0}，自身体力低于对手时变为1/{1}
//
// 实现口径（直系先例 j_drain_if_not_full.cpp 1371：drain_hp 除数口径）：
//   - 档位门 = **当前体力绝对值**比较 self.hp < opp.hp（严格小于；相等走常规档）。
//     官方文案"自身体力低于对手"是当前体力值比较，不是比例——与 1694/1718 同型 HP 门。
//   - 吸取 = drain_hp(owner, opp, denom)：内核读**对手** numericalBase[HP] 算
//     max_hp/denom，对手吃 FIXED 固定伤害、自身 heal_impl 回等量（封回血生效）。
//     denom 选择在调用前一次定档，不会二次结算。
//   - 两个分母都必须 > 0（是除数不是 pct 关闸档，红线 7 的"显式 0"语义不适用）。
//   - 载体链兄弟（25770 的 962/843/58/471、31575 的 1260）均不产生体力变化；
//     31416 等攻击载体自带红伤，场景用 atk=1 + 自身回血精确量作主断言。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   1263 [常规分母,低体分母]：25770 邪能构装 [3,2]、26398 邪羽风暴 [3,2]、
//     26624 漆黑之羽 [3,2]、31416 迅影一闪 [4,3]、31520 幽鬼刃轮 [4,3]、
//     31575 幻梦风灵破 [3,2]、31838 禅定印 [4,3]、38449 冰火破灭弹 [4,3]（3/2、4/3 两档）
//
// 动工前置三查（2026-10-22）：①内核 grep 1263 无硬编码（命中均为注释/他宏）②
//   effect_info 无同文模板 ③moves_lib 并集（含 kbatch worktree）无 1263 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5580_drain_max_fraction_tier.cpp（号段 5580-5589）。

#include "common.h"

namespace {

// ── 1263 —— 吸取对手最大体力 1/{0}；自身当前体力低于对手时改为 1/{1} ──
// int_args = [owner, target, 常规分母, 低体分母]（args_num=2）。
EffectResult effect_1263_drain_max_fraction_tier(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1263, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1263, args,
                     "1263 布局=[owner,target,常规分母,低体分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom_normal = args.int_args[2];
    const int denom_low = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1263, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom_normal > 0 && denom_low > 0, 1263, args,
                     "需要 两个分母 > 0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    // 档位门：当前体力绝对值严格小于（相等 → 常规档）。
    const int self_hp = ctx->getPet(owner).hp;
    const int opp_hp = ctx->getPet(opp).hp;
    const int denom = (self_hp < opp_hp) ? denom_low : denom_normal;
    (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, denom);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1263, effect_1263_drain_max_fraction_tier)
