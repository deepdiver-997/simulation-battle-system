// J 组 J193 · 击败自身异常族 —— 1 宏/9 载体（2026-10-22，worktree ../agents/j193）
//
// 家族语义（effect_info 模板即权威）：
//   1933 [2] 击败对手则{0}%令自身{1}
//
// 实现口径（j_break_drain_defeat.cpp 934 击败 watcher 直系 + 现代异常通道）：
//   - 用技时注册 EVENT_DEATH watcher（once=true，窗口=本回合 duration_rounds=1）：
//     只认"本次击败"——DEATH 事件带击杀方归因（battle_primitives.cpp:2141 actor=
//     killer、target=被击败方），严格过滤 ev.actor==owner && ev.target==opp，
//     不会把之后别的技能击杀错记到这次使用上。
//   - 命中 → apply_anomaly_checked(自身, {1}, 随机时长, owner, {0}%, Skill)
//     （191/476/4829 同通道；概率申报受亮节闸门管辖）。
//   - 审计槽 plugin_storage[k1933AuditBase+owner] = 本次异常码（场景断言"watcher 真的
//     走到"，无随机）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1933 [概率,异常]：37665 星光·泰山压顶 [100,7]（链 [2220,1395,1933]，物 300）、
//        36628 霜·凌天寂帝枪 [100,32]、37039 晟锋断宵诀 [100,2]、37483 生死盘灵
//        [100,14]、38377 幕下睡魇 [100,30]…（9/9 自洽，全部 100%）
//
// 动工前置三查（2026-10-22）：①内核 grep 1933 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5150_defeat_self_anomaly.cpp（号段 5150-5159）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1933AuditBase = 990193300;   // +owner：本次触发的异常码（审计槽）

// ── 1933 —— 击败对手则{0}%令自身{1} ──
// int_args = [owner, target, 概率, 异常码]（args_num=2）。
EffectResult effect_1933_defeat_self_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1933, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1933, args,
                     "1933 布局=[owner,target,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1933, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1933, args, "需要 概率∈[0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1933, args,
                     "1933 {1} 异常码超出 0-43");
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/true,
        [owner, opp, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.target != opp) {
                return;   // 只认本次用技的击杀（DEATH 带 killer 归因）
            }
            c->plugin_storage[k1933AuditBase + owner] = anomaly;
            if (!g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            (void)g_core->apply_anomaly_checked(c, owner, anomaly, random_anomaly_duration(),
                                                owner, chance, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1933, effect_1933_defeat_self_anomaly)
