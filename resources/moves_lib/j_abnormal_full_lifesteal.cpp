// J 板 J261 · 异常门全额吸血族 —— 575×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 575，args_num=0）：
//   "对手处于任意异常状态时造成的伤害将全额恢复体力"（无行参）。
//
// 载体（side_effect 链核算，2026-10-22）：
//   15675 虚空轮回 [575]（纯链，特攻 130，必中）
//   16471 幽冥玄冰击 [575]（纯链，物攻 150）
//   33078 噬魂毒牙 [575]（纯链，物攻 130）
//   34030 碧蓝之径 [575]（纯链，特攻 130）
//   19035 索魂万刃 [469,575] / 19901 霸魂狂刃 [1025,575]
//   31398 电威雷刃 [959,575] / 34773 暗龙寂灭袭 [1138,575,1640]
//
// 实现口径：
//   - 触发时点 = SKILL_EFFECT（**伤害发生前**判门）：gate =
//     ctx->has_any_active_abnormal_status(对手)（BattleContext 内联，2009/1248 同判据；
//     遍历全部官方异常槽，任一未到期即真）。
//   - 门过 → 注册**本回合** EVENT_TAKE_DAMAGE TEAM watcher（duration_rounds=1，
//     ctx->roundCount 起算；once=false——多段/多次红伤各自全额回血，与 89 吸血窗同形）。
//   - 回调只认 ev.actor==owner（己方造成）、ev.target==对手、damage_kind==NORMAL、amount>0
//     （均为实际落血量：护盾吸收后的 amount），然后 heal_amount(owner, ev.amount)
//     ——全额、denom=1；吃封回血/恢复修正/上限，走原语不直写 hp。
//   - 门闭（对手无任何异常）→ 不注册 watcher（审计写 0）。
//   - 审计槽 996575100 + owner：1=门过 / 0=门闭。
//
// 记档：
//   - 门在 SKILL_EFFECT 判定（施放瞬间），**不是**在伤害回调里现场查——睡眠类异常
//     "命中即醒"（K2），回调时现场查会漏；场景 T5920.a 用睡眠门锁死该时点。
//   - watcher 覆盖"本回合"内己方全部 NORMAL 伤害；跨回合由到期窗口自动失效，
//     无需手写账本（T6 注册面窗界后的新代码口径）。
//
// 动工前置三查（2026-10-22）：①内核 grep 575 无行为硬编码 ②effect_info 模板唯一
//   args_num=0、custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集
//   （含 kbatch/main-local 检出）无 SKILL_EFFECT(575) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5920_abnormal_full_lifesteal.cpp（号段 5920-5929）。

#include "common.h"

namespace {

constexpr int k575AuditBase = 996575100;   // +owner：1=门过 / 0=门闭

// ── 575 —— 对手处于任意异常状态时，造成的伤害全额恢复体力 ──
// int_args = [owner, target]（args_num=0）。
EffectResult effect_575_abnormal_full_lifesteal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 575, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 575, args,
                     "575 布局=[owner,target]（args_num=0）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     575, args, "需要 owner/target ∈ {0,1}");

    const bool gate = ctx->has_any_active_abnormal_status(target);
    ctx->plugin_storage[k575AuditBase + owner] = gate ? 1 : 0;
    if (!gate) {
        return EffectResult::kOk;   // 门闭：对手无任何活跃异常
    }
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }

    // 本回合窗口：从施放回合起算 1 回合；once=false 让本回合每次红伤都全额回血。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/false,
        [owner, target](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.target != target) {
                return;   // 只认己方打到对手的红伤
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "造成的伤害" = 攻击红伤；粉/真/属性伤不触发
            }
            if (!g_core || !g_core->heal_amount) {
                return;
            }
            (void)g_core->heal_amount(c, owner, ev.amount);   // 全额（denom=1）
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(575, effect_575_abnormal_full_lifesteal);
