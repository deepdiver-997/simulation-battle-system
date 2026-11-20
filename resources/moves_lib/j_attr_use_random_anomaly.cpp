// J 组 J195 · 属性技异常窗族 —— 1 宏/10 载体（2026-10-22，worktree ../agents/j195）
//
// 家族语义（effect_info 模板即权威）：
//   1394 [2] {0}回合内对手使用属性技能则随机附加{1}种异常状态
//
// 实现口径（j_attr_cond_window.cpp 163/1348 直系）：
//   - EVENT_SKILL_USED watcher（skills.cpp 发射点"使用即触发"，miss/被封属也算使用；
//     used_attribute 标记属性技），窗口 = watcher duration={0}（TEAM，跨回合稳定）。
//   - 过滤 ev.target==owner && ev.used_attribute（攻击方使用属性技时 target=本效果持有方）。
//   - 附加 = attach_random_anomalies(对手, anomaly_pool_all(), {1}, 100, owner, Skill)
//     （1367/1348 同原语：解析池 → 子句级概率 → 不重复挑选 → 逐只现代通道施加）。
//   - 审计槽 plugin_storage[k1394AuditBase+owner] = 本次实际落地种数（场景可断言
//     "窗真的触发"，无随机）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1394 [回合,种数]：28230 追猎 [3,1]（纯链，属）、36688 漩涡撕扯 [3,2]（纯链）、
//        27706 尘心裂纹 [3,1]、26907 不灭龙甲 [2,2]…（10/10 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1394 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5170_attr_use_random_anomaly.cpp（5170-5179）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1394AuditBase = 990139400;   // +owner：本次实际落地种数（审计槽）

// ── 1394 —— {0}回合内对手使用属性技能则随机附加{1}种异常状态 ──
// int_args = [owner, target, 回合, 种数]（args_num=2）。
EffectResult effect_1394_attr_use_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1394, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1394, args,
                     "1394 布局=[owner,target,回合,种数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int count = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1394, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && count > 0, 1394, args, "需要 回合/种数 > 0");
    if (!g_core || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false,
        [owner, opp, count](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || !ev.used_attribute
                || !g_core || !g_core->attach_random_anomalies) {
                return;   // 只认对手使用属性技（469/163 同判据）
            }
            const RandomAnomalyResult r = g_core->attach_random_anomalies(
                c, opp, anomaly_pool_all(), count, /*probability_pct=*/100, owner,
                ChanceSource::Skill);
            c->plugin_storage[k1394AuditBase + owner] = r.landed;
        },
        WatcherScope::TEAM);   // 跨回合窗口必须 TEAM（469/163 同款选型）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1394, effect_1394_attr_use_random_anomaly)
