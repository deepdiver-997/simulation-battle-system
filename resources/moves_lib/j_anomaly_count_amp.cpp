// J 组 J267 · 异常计数增伤族 —— 1 宏/7 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   2073 [1] 对手每处于一种异常状态造成的伤害提升{0}%
//
// 实现口径（先例：j_cond_amp5.cpp 1103 的 AMP_EXTRA 乘法通道；
// j_abnormal_full_lifesteal.cpp 575 的异常查询面）：
//   - 计数 = 对手当前生效异常**种数** n（g_core->has_active_anomaly 扫 0..43；
//     "处于一种异常状态"按种类计，不含剩余回合数）。
//   - 增伤 = effect_set_damage_amp_extra(owner, target, n × {0})：写
//     ws.damage_add_extra_mul 首空槽（1103 同通道），本回合攻击结算消费；
//     n=0 时 bonus=0，原语不写槽（不占槽位）。
//   - 审计槽 991207300 + owner = 本次 bonus 百分比（0=对手无异常）。
//   - {0} 在位即消费；<=0 视为坏行参（EFFECT_ARG_GUARD）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   2073 [pct]：36827 御浪千击波 [60]、36846 拂晓 [50]、37612 烈焰·高能射线 [50]、
//     37703 凝血寒浪 [50]（纯链）、38228 赤绘 [50]、38368 强电冲击 [50]、
//     38390 自·万物定理 [40]（7/7 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 2073 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(2073) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5980_anomaly_count_amp.cpp（号段 5980-5989）。

#include "common.h"

namespace {

constexpr int kAuditBase = 991207300;   // +owner：本次 bonus 百分比

// ── 2073 —— 对手每处于一种异常状态造成的伤害提升{0}% ──
// int_args = [owner, target, 每异常 pct]（args_num=1）。
EffectResult effect_2073_anomaly_count_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2073, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2073, args,
                     "2073 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2073, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 2073, args, "需要 pct > 0");

    int kinds = 0;
    if (g_core && g_core->has_active_anomaly) {
        for (int id = 0; id <= 43; ++id) {
            if (g_core->has_active_anomaly(ctx, target, id)) {
                ++kinds;
            }
        }
    }
    const int bonus = kinds * pct;
    ctx->plugin_storage[kAuditBase + owner] = bonus;
    if (bonus != 0) {
        EffectArgs amp_args(std::vector<int>{owner, target, bonus});
        (void)effect_set_damage_amp_extra(ctx, amp_args);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2073, effect_2073_anomaly_count_amp)
