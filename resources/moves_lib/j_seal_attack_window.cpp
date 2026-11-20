// J183 封攻击技窗族 —— effect 1923（1 宏 / 10 技）
//                                                                          （2026-10-07，worktree ../agents/j183）
//
// 效果模板（effect_info 1923，args_num=1）："{0}回合内令对手使用的攻击技能无效"。
//
// 载体切片（args_num 逐位锚定，2026-10-07）：
//   28283 长相思 [191,1396,1923,478] args[4, 4,3,2,2, 1, 1] → 1923=[1]
//     （191=[4]反弹异常窗、1396=[4,3,2,2]低血吸取窗、478=[1]封属性技窗）
//   濛濛雾海 28656 [191,1923,1118,1154] args[4, 1, 3,100,5,1, 3,200,0] → 1923=[1]
//     （1118=[3,100,5,1]攻/属条件窗、1154=[3,200,0]附加窗）
//   上古岩碑 29205 [191,1923,1458] args[4, 1, 3,50,100,29] → 1923=[1]
//     （1458=[3,50,100,29] 属性技无效/异常窗——**未接**，静默 no-op）
//
// 实现：SKILL_EFFECT 时点 → seal_skill(attribute=false, attack=true, count=1 假值、
//   duration_rounds=db[0]（纯回合型，695/781 形），非 hit_invalid——「无效」= 走技能无效
//   补偿口径（chaos_sorensen 1011 属性版同通道、936/695 先例；1217/936 与 781 的
//   命中失效分歧按 1217/936 取"无效"）。penetrable=true。
//
// 三查记录（2026-10-07）：①内核 grep 1923 无硬编码；②effect_info 模板即权威；
//   ③registry.moves 无 1923、moves_lib 无 1923——真缺口。
// 先例：chaos_sorensen 1011 ③（seal_skill 回合型/次数型全参调用形）、
//   skills.cpp 695/781 注（纯回合型凑 count=1 假值）、1217/936（"无效"语义裁定）。
// 场景回归：test/scenario/scenario_3425_seal_attack_window.cpp（T3425-T3426）。

#include "common.h"

namespace {

EffectResult effect_1923_seal_attack_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1923, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1923, args, "需要 [owner,target,rounds]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1923, args,
                     "owner/target 越界");
    if (!g_core || !g_core->seal_skill || rounds <= 0) {
        return EffectResult::kOk;
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/1923,
                       /*attribute=*/false, /*attack=*/true, /*count=*/1,
                       /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE,
                       /*hit_invalid=*/false, /*chance_pct=*/100,
                       /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1923, effect_1923_seal_attack_window);
