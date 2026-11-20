// J 组 J218 · 概率异常低体翻倍族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1210 [2] {0}%使对手{1}，自身体力低于对手时概率翻倍
//
// 实现口径：
//   - SKILL_EFFECT 时点同步读双方**当前**体力：self.hp < opp.hp（严格）→ p×2 封 100。
//     与 J206 460（提升门加算）/ J209 974（概率申报）同申报制通道。
//   - 概率走 apply_anomaly_checked（申报 {0}%，受亮节闸门管辖）；{0}=0 在位即消费
//     （红线7：0 是显式关闸档，不设 >0 门）。
//   - 异常时长 random_anomaly_duration()（既有施加族一致）。
//   - 审计槽：990121000+self*2 = 本次申报概率（翻倍后的实际值）；
//             990121001+self*2 = ApplyAnomalyResult 整数（场景判"是否真的掷中"）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   1210 [概率,异常码]：37041 冥土掩埋 纯链 [25,9]、29367 不稳定存在 [50,6]、
//     30950 火焰初明 [40,0]、32018 虎蜂刺 [35,6]、34437 云光散花击 [40,20]、
//     35268 千凿巨石破 [30,9]、38243 穿冰破寒刺 [40,15]；32183 余烬复燃 双挂
//     [40,2]/[40,22]（同宏两段各自消费）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1210 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(1210)——真缺口。
// 场景回归：test/scenario/scenario_5470_prob_anomaly_low_hp.cpp（号段 5470-5479）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1210PctBase = 990121000;     // +self*2：本次申报概率
constexpr int k1210ResultBase = 990121001;  // +self*2：ApplyAnomalyResult 整数

// ── 1210 —— {0}%使对手{1}，自身体力低于对手时概率翻倍 ──
// int_args = [owner, target, 概率, 异常码]（args_num=2）。
EffectResult effect_1210_chance_anomaly_low_hp_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1210, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1210, args,
                     "1210 布局=[owner,target,概率,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1210, args, "需要 owner/target ∈ {0,1}");
    // 红线7：概率在位即消费（0 = 显式关闸档，仍申请、掷点必败——不得当非法参数）。
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 1210, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1210, args, "需要合法异常 id");

    int declared = pct;
    if (ctx->getPet(self).hp < ctx->getPet(opp).hp) {
        declared = (declared > 50) ? 100 : declared * 2;   // 概率翻倍，封顶 100
    }
    ctx->plugin_storage[k1210PctBase + self * 2] = declared;
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, declared, ChanceSource::Skill);
    ctx->plugin_storage[k1210ResultBase + self * 2] = static_cast<int>(r);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1210, effect_1210_chance_anomaly_low_hp_double)
