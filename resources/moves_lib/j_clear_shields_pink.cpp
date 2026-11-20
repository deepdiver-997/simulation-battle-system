// J 组 J214 · 清盾罩附粉伤族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1828 [1] 消除对手所有护盾、护罩效果，并附加等同于消除护盾、护罩值总和{0}%的百分比伤害
//
// 实现口径（batch_c 1989 两清直系 + 1072 粉伤落点）：
//   - 两清：对手 shield_bank_（护盾，吃红伤）+ hood_bank_（护罩，吃粉伤）分别 clear()；
//     "护盾、护罩类效果"是两清（1989 判据；1215 注释同款）。
//   - 消除数额 = 清理前两侧 total() 之和；**先记数再清**（clear 后读不到）。
//   - 清完 emit EVENT_SHIELD_BROKEN（1989 同款；"护盾被击破/消失时…"监听器照常收到，
//     amount=被消除的总和）。
//   - 附伤 = deal_pink_damage(target, 消除总和×{0}/100, PERCENT_VALUE, owner)——已算好
//     具体值、走百分比档抗性/护罩（1072/870 同款；插件造粉伤唯一入口）。
//   - 无盾可清（总和 0）→ 整条无事（不 emit、不附伤）；{0}=0 是在位关闸档：清照做、
//     附伤值 0 自然不落地（红线7：不以 `pct>0` 拒绝参数）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 10/10 自洽）：
//   1828 [百分比]：28632 浅唱诗 [150]（纯链）、36017 圣剑·至序永生 [100]、36374 冥魂咒印
//     [100]、36495 破产之日 [100]、36628 霜·凌天寂帝枪 [60]、36708 天威圣戟 [50]、
//     37374 炼·无妄登极 [100]、38041 欲咒昧灵录 [80]…（60/80/100/150 四档）
//
// 动工前置三查（2026-10-22）：①内核 grep 1828 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5410_clear_shields_pink.cpp（号段 5410-5419）。

#include "common.h"

namespace {

// ── 1828 —— 消除对手所有护盾、护罩效果，并附等同消除总和{0}%的百分比伤害 ──
// int_args = [owner, target, 百分比]（args_num=1）。
EffectResult effect_1828_clear_shields_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1828, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1828, args,
                     "1828 布局=[owner,target,百分比]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1828, args, "需要 owner/target ∈ {0,1}");
    // 红线7：百分比在位即消费（0 = 显式关闸档，仍执行两清）。
    EFFECT_ARG_GUARD(pct >= 0, 1828, args, "需要 百分比 >= 0");
    ElfPet& foe = ctx->getPet(target);
    // 先记数再清（clear 之后总量读不到）。
    const int removed = foe.shield_bank_.total() + foe.hood_bank_.total();
    if (removed <= 0) {
        return EffectResult::kOk;   // 无盾可清 → 整条无事
    }
    foe.shield_bank_.clear();
    foe.hood_bank_.clear();
    ctx->event_center_.emit(
        BattleEvent{EventType::EVENT_SHIELD_BROKEN, owner, target, removed});
    const int value = removed * pct / 100;
    if (value > 0 && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, value, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1828, effect_1828_clear_shields_pink)
