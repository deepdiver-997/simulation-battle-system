// J 组 J306 · 天敌翻倍固伤族 —— 1309×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1309（args_num=1）"附加{0}点固定伤害，遇到天敌时附加效果翻倍"
//
// 实现口径（同形先例直系）：
//   - 附加固定伤害 = fixed_damage(对手, amount)（**粉伤**路径：吃护罩/固伤抗性/粉伤
//     免疫相；497 递增固伤同款原语）。载体 {0} ∈ {150, 200}。
//   - "遇到天敌时附加效果翻倍" = is_predator(自己)（common.h 助手：**对手固有属性克制
//     自身固有属性**、克制倍率 > 1；不看技能属性、不看系别视图——官方实测口径）→
//     amount × 2。次序同 497「最高{2}点→封顶后天敌翻倍」：先按 {0} 取额，再整体翻倍。
//   - 审计槽：991309100 + owner*2 = 本次实际附加的固伤额（场景据此做**精确**断言，
//     不受红伤 85%~100% 浮动干扰）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽、无剩余参数）：
//   1309 [固伤]：33350 深渊绝息 [150]（纯链 cat2 pow140，**场景载体**）、35508 随风吹拂
//        [150]（纯链 cat2 pow120）、35898 逆光生长 [150]（纯链 cat1 pow130）、
//        35309 绛天龙行 [200]（纯链 cat1 pow140）、31633 暗煞龙吟 [150]
//        （链 [889,1309]，prio=3）、34417 烈焰赤牙碎 [200]（链 [472,1309]）、
//        36069 寸毫染霜雪 [200]（链 [980,1309]）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1309 无行为硬编码 ②effect_info 仅本条模板
//   （无同文其它 id）③moves_lib/soul_lib 并集（jbatch/kbatch/hub 各检出）无
//   SKILL_EFFECT(1309)——真缺口。
//
// 场景回归：test/scenario/scenario_6390_predator_double_fixed.cpp（号段 6390-6399）。

#include "common.h"

namespace {

constexpr int k1309FixedAuditBase = 991309100;   // +owner*2：本次实际附加的固伤额

}  // namespace

// ── 1309 —— 附加{0}点固定伤害，遇到天敌时附加效果翻倍 ──
// int_args = [owner, target, 固伤]（args_num=1，loader 注入 owner/target 前缀）。
EffectResult effect_1309_predator_double_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1309, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1309, args,
                     "1309 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1309,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 1309, args, "需要 固伤 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    // 天敌 = 对手固有属性克制自身固有属性（common.h 助手；497/956 同款判据）。
    const int dmg = is_predator(ctx, owner) ? amount * 2 : amount;
    ctx->plugin_storage[k1309FixedAuditBase + owner * 2] = dmg;
    (void)g_core->fixed_damage(ctx, opp, dmg);
    return EffectResult::kOk;
}

SKILL_EFFECT(1309, effect_1309_predator_double_fixed)
