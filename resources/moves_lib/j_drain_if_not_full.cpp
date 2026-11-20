// J 组 J216 · 未满体力吸取族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1371 [1] 若自身未满体力则吸取对手最大体力的1/{0}
//
// 实现口径（1672 未满体力吸取直系）：
//   - 门 = self.hp < max_hp_of(self)（"未满体力"严格小于；满血不触发）。
//   - 吸取 = drain_hp(owner, opp, {0})：内核读**对手** numericalBase[HP] 算 max_hp/{0}，
//     对手吃 FIXED 固定伤害、自身 heal_impl 回等量（1062/1672 同通道；封回血生效）。
//   - {0} 是除数，必须 >0（不是 pct 关闸档，红线7 的"显式 0"语义不适用于除数）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 9/9 自洽）：
//   1371 [分母]：32445 天火驱邪 [4]、33011 龙·战天下 [4]、33619 狂·雪覆洪荒 [3]、
//     35241 炼·耀星闪烁 [3]、35688 秘影毒瘴 [3]、35807 发条之鸣 [3]、
//     36482 灵烁无双箭 [4]、37316 扶桑 [3]、38147 玄羽悼唁 [5]（3/4/5 三档）
//
// 动工前置三查（2026-10-22）：①内核 grep 1371 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5430_drain_if_not_full.cpp（号段 5430-5439）。

#include "common.h"

namespace {

// ── 1371 —— 若自身未满体力则吸取对手最大体力的1/{0} ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1371_drain_if_not_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1371, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1371, args,
                     "1371 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1371, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1371, args, "需要 分母 > 0");
    if (ctx->getPet(owner).hp >= max_hp_of(ctx, owner)) {
        return EffectResult::kOk;   // 满体力 → 门闭
    }
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, denom);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1371, effect_1371_drain_if_not_full)
