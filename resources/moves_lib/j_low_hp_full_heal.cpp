// J 组 J228 · 低体全恢复族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   530 [1] 使用时若自身体力低于1/{0}则恢复全部体力值
//
// 实现口径（直系先例 batch_a_skills.cpp effect_835：heal_amount 到 max_hp）：
//   - 门 = self.hp < max_hp/{0}（**严格小于**；恰好等于阈值不触发——"低于"字面）。
//     阈值整数除法与载体口径一致（max_hp_of 读 numericalBase[HP]）。
//   - 恢复量 = max_hp - self.hp（只补差额，避免 heal_amount 的封顶掩盖）；
//     走 heal_amount 真原语 → 自愈封回血（HEAL_BLOCK）生效，不直写 hp。
//   - 载体均为攻击/属性技，链上无兄弟（15467/17387/34125 纯链），自身 HP 只由本效果改写。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   530 [分母]：15467 绝影剑 [3]、17387 天火渡世劫 [4]、17488 玄空极鸣闪 [4]、
//     17789 利刃奇袭 [4]、17966 暗夜诛魔阵 [4]、22737 十二试炼 [3]、
//     24420 铁血龙盔 [5]、34125 荆棘斑斓 [2]（2/3/4/5 四档）
//
// 动工前置三查（2026-10-22）：①内核 grep 530 无硬编码 ②effect_info 无同文模板
//   ③moves_lib 并集（含 kbatch worktree）无 530 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5590_low_hp_full_heal.cpp（号段 5590-5599）。

#include "common.h"

namespace {

// ── 530 —— 使用时若自身体力低于1/{0}则恢复全部体力值 ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_530_low_hp_full_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 530, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 530, args,
                     "530 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 530, args, "owner 越界");
    EFFECT_ARG_GUARD(denom > 0, 530, args, "需要 分母 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, owner);
    const int hp = ctx->getPet(owner).hp;
    if (hp >= max_hp / denom) {
        return EffectResult::kOk;   // 门：严格低于 1/denom 才恢复
    }
    if (hp <= 0) {
        return EffectResult::kOk;   // 已倒地（防御性；正常流程不会走到）
    }
    (void)g_core->heal_amount(ctx, owner, max_hp - hp);   // 补到满，吃封回血
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(530, effect_530_low_hp_full_heal)
