// J44 · 双方同态族（本单产码部分）—— 547×24（2026-10-02，worktree ../agents/j44）
//
// ⚠️ 本单 2 宏中 642 语义未定（"{1}%做出{2}"动作面无模板/4399 直证）——记档留
//   晨班 4399 深查后再产；本分支只产 **547**。
//
// 547（arg_count=2）"使敌我双方同时{0}{1}回合"（{0}=异常码、{1}=指定回合数）：
//   - **双侧同态**：apply_anomaly_checked(self, {0}, dur={1}) + (opp 同款)——双方
//     各自过免疫/抗性检查链（checked 通道），指定时长非 random_anomaly_duration
//     （573 指定时长先例）。解码依据：载体内码 {0,2,5,27} 均为异常码（火焰领地
//     [2,5]=双烧 5 回合、电磁过载 [0,3]=双麻 3 回合、坏死余芳 [27,2]）。
//   - 实例：火焰领地 23038 [547]|[2,5]（全库 24 载体，属性技为主）。
//
// 动工前置三查（2026-10-02）：①内核 grep 547 无行为硬编码 ②effect_meta 无分类
//   牵连 ③moves_lib（本分支）无既有注册——真缺口。载体切片核验 24 载体布局零
//   异常（[异常码,回合数]）。
// 先例：573（apply_anomaly 指定时长）、2005/464（apply_anomaly_checked 申报制）、
//   1414（"敌我双方同时"措辞同族，4399 绝念之先驱互证）。
// 场景回归：test/scenario/scenario_1290_547.cpp（1290 号段，单宏单块）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 547 —— 使敌我双方同时{0}{1}回合（{0}=异常码，{1}=指定回合数）──
EffectResult effect_547_both_sides_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 547, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 547, args,
                     "布局=[owner,target,anomaly,rounds]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 547, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 547, args, "异常码越界");
    EFFECT_ARG_GUARD(rounds > 0, 547, args, "需要 rounds>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 双侧各自过检查链（免疫/抗性独立判定）；指定时长如实申报。
    (void)g_core->apply_anomaly_checked(ctx, self, anomaly, rounds, self, 100,
                                        ChanceSource::Skill);
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, rounds, self, 100,
                                        ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(547, effect_547_both_sides_anomaly)
