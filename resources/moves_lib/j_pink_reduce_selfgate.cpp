// J64 · 粉减/自状门族 —— 1121×24 / 1169×17（2026-10-03，worktree ../agents/j64）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1121（args_num=2）"下{0}次受到的固定伤害和百分比伤害减少{1}%"：
//   - 粉伤管线 REDUCE_EXTRA 百分比免减（1890 直系 J22 同通道：shared_ptr 计次 +
//     resolvedPink.target==self 判据——只减自身受到的固伤/百分伤，红伤/真伤不归本条）；
//   - 与 1890 的差别：模板**无回合窗**（"下{0}次"纯计次）→ 不套 watch_attr_use，
//     SKILL_EFFECT 一次注册（ON_STAGE epoch：切换失效、跨回合存活，"次"用完自然闲置）；
//   - 实例：刀剑如梦 25425 [1121,1154,48]|[1,50,3,200,0,5]（属性技载体）。
// 1169（args_num=1）"若自身处于异常状态则攻击伤害额外+{0}%且回合末解除自身所有异常"：
//   - 门 = has_any_active_abnormal_status(self)（SKILL_EFFECT 时点判）；
//   - "额外+{0}%" = **非通用增伤**（"额外"判据字面）→ AMP_EXTRA 乘法通道
//     （1103/693 同款 effect_set_damage_amp_extra，抢首空槽，落本次攻击 snapshot）；
//   - "回合末解除自身所有异常" = BATTLE_ROUND_END 一次性 tick → dispel_active_anomalies
//     （2145/2497 解除原语；register_round_effect rounds=1 当回合末即执行）。
//   - 实例：真龙焚天舞 30706 [797,1169]|[2,100]（797=消对手回合类，空场良性，已注册）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1121/1169 无硬编码（pink_reduce_pct 默认项
//   是另一通道，不冲突）②effect_meta 无分类牵连 ③moves_lib 全 worktree 并集无注册——
//   真缺口。切片：side_effect 表 arg_count 1121=2/1169=1 与 effect_info.args_num 一致；
//   41 载体链切片自洽（1121 [次数,百分比] 动工逐条核 / 1169 单参）。
// 先例：1890（j_attr_cond_window.cpp REDUCE_EXTRA 计次 lambda）、1103（j_cond_amp5.cpp
//   AMP_EXTRA 写入）、2497（dispel_active_anomalies）、J14 1024（BATTLE_ROUND_END
//   register_round_effect）、J12（合成节点 id 规避同键覆盖）。
// 场景回归：test/scenario/scenario_1500_pink_reduce_selfgate.cpp（1500 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <memory>

namespace {

// 合成节点 id（J12 kNode 同款规避同键覆盖：1169 的驱散 tick 与门/写槽同回合共存）。
constexpr int kNode1169Dispel = 990116901;

// ── 1169 的回合末驱散 tick（载荷 [owner]）──
EffectResult j64_dispel_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->dispel_active_anomalies || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    (void)g_core->dispel_active_anomalies(c, a.int_args[0]);
    return EffectResult::kOk;
}

}  // namespace

// ── 1121 —— 下{0}次受到的固定伤害和百分比伤害减少{1}% ──
// int_args = [owner, target, counts, pct]（args_num=2）。无回合窗：注册一次、计次耗尽闲置。
EffectResult effect_1121_pink_reduce_counts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1121, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1121, args,
                     "1121 布局=[owner,target,counts,pct]");
    const int self = args.int_args[0];
    const int counts = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1121, args, "owner 越界");
    EFFECT_ARG_GUARD(counts > 0 && pct > 0 && pct <= 100, 1121, args,
                     "需要 次数>0 且 0<减伤≤100");
    auto remaining = std::make_shared<int>(counts);
    ctx->register_pink_damage_effect(
        PinkDamagePhase::REDUCE_EXTRA, self,
        [self, pct, remaining](BattleContext* cc, int bucket_owner) {
            if (!cc || bucket_owner != self || *remaining <= 0) {
                return;
            }
            PinkDamageResolved& pk = cc->resolvedPink;
            if (pk.target != self || pk.final <= 0) {
                return;   // 只减自身受到的固伤/百分伤
            }
            pk.final -= pk.final * pct / 100;
            --*remaining;
        });
    return EffectResult::kOk;
}

// ── 1169 —— 若自身处于异常状态则攻击伤害额外+{0}%且回合末解除自身所有异常 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_1169_self_anomaly_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1169, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1169, args,
                     "1169 布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1169, args, "owner 越界");
    EFFECT_ARG_GUARD(pct > 0, 1169, args, "需要 pct>0");
    if (!ctx->has_any_active_abnormal_status(self)) {
        return EffectResult::kOk;   // 门：自身处于异常状态
    }
    // "额外+{0}%" → AMP_EXTRA 乘法通道，落本次攻击 snapshot（1103 同款）。
    EffectArgs tick_args(std::vector<int>{self, 1 - self, pct});
    (void)effect_set_damage_amp_extra(ctx, tick_args);
    // 回合末解除自身所有异常（当回合末即执行，rounds=1）。
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, kNode1169Dispel, /*rounds=*/1,
                          {self}, &j64_dispel_tick);
    return EffectResult::kOk;
}

SKILL_EFFECT(1121, effect_1121_pink_reduce_counts)
SKILL_EFFECT(1169, effect_1169_self_anomaly_amp)
