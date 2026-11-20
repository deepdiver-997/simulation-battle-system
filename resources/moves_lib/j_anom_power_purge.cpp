// J 组 J104 · 异常状态威力族 —— 513×14（2026-10-03，worktree ../agents/j104）
//
// 家族语义（effect_info 官方模板即权威）：
//   513（arg_count=0）"处在烧伤、中毒、冻伤状态时威力翻倍，命中后解除这些异常状态"
//
// 实现口径：
//   - 变威力 = SKILL_EFFECT 查自身 {烧伤2, 中毒1, 冻伤5} 任一（has_active_anomaly
//     CoreApi 槽——非 inline 方法插件不可直调）→ skill_power_view = view×2
//     （102 翻倍通道，batch_b:559 同款）。
//   - 解除 = 同 tick 内对三码 reduce_active_anomaly_rounds(ctx, self, 99) 压到
//     当回合前（CoreApi 215 槽；"只压剩余回合数"——end<round 即刻失活、由回合
//     扣减自然收尾，星盘族冻结口径不受影响）。miss 不到 SKILL_EFFECT 桶 →
//     "命中后"语义天然成立。
//   - 零参 bind_participants → [owner, target]。
//
// 载体切片（unity 库 json_each 精确匹配 14；全零参自洽，脚本核算 2026-10-03）：
//   513 零参：纯链 4 条（千翼幻光闪 14695 特150 / 支离破碎 15035 / 破颅之锐 15950 /
//   八荒龙息焰 16829——场景载体取 千翼幻光闪）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 513 无硬编码 ②effect_meta
// 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：102（状态门威力翻倍 view×2，batch_b:559）、reduce_active_anomaly_rounds
//   （CoreApi 215 槽，2207 族）。
// 场景回归：test/scenario/scenario_1970_anom_power_purge.cpp（1970 号段，一文件一宏）。

#include "common.h"

namespace {

// ── 513 —— 处在烧伤/中毒/冻伤时威力翻倍，命中后解除这些异常 ──
// args_num=0：bind_participants → [owner, target]。
EffectResult effect_513_anom_power_purge(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 513, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 513, args,
                     "513 布局=[owner,target]（零参 bind_participants）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     513, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int fire = static_cast<int>(AbnormalStatusId::Burn);
    const int poison = static_cast<int>(AbnormalStatusId::Poison);
    const int frostbite = static_cast<int>(AbnormalStatusId::Frostbite);
    const bool boosted = g_core->has_active_anomaly(ctx, self, fire)
                      || g_core->has_active_anomaly(ctx, self, poison)
                      || g_core->has_active_anomaly(ctx, self, frostbite);
    if (boosted) {
        const int view = ctx->ws.skill_power_view[self];
        if (view > 0) {
            ctx->ws.skill_power_view[self] = view * 2;   // 威力翻倍（102 通道）
        }
        // 命中后解除三异常：剩余回合压尽（即刻失活，回合扣减自然收尾）。
        if (g_core->reduce_active_anomaly_rounds) {
            (void)g_core->reduce_active_anomaly_rounds(ctx, self, 99);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(513, effect_513_anom_power_purge)
