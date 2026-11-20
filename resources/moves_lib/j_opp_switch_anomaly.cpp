// J 组 J212 · 对手主动切换异常窗族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   2040 [3] {0}回合内对手主动切换精灵出战时{1}%{2}
//
// 实现口径（batch_c 1510 对手中切窗直系，检测面换 EVENT_SWAP）：
//   - SKILL_EFFECT 时点按 {0} 回合注册 EVENT_SWAP watcher（注册面窗界 duration_rounds=
//     {0}，过期自动跳过；不手写窗账本——套路目录 §三 T6 口径）。
//   - **EVENT_SWAP 只在主动中切 emit**（battleFsm handle_OperationChooseSkillMedicament：
//     CHOOSE_PET 实际换槽时 emit；死亡换宠走 perform_switch 但明确不 emit SWAP）——
//     所以不需要 1510 的 pet_death_notified 判据，事件面天然就是"主动切换"。
//   - drain 时 perform_switch 已完成，getPet(target) = **登场的新宠**，时点即"出战时"。
//   - 概率走 apply_anomaly_checked（申报制，受亮节闸门管辖）；{1}=0 在位即消费（红线7：
//     不写成 `>0` 守卫把显式 0 关闸档变成参数非法）。
//   - 异常时长走 random_anomaly_duration()（当前恒 2，与既有施加族一致）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 10/10 自洽）：
//   2040 [回合, 概率, 异常码]：28373 薄雾流光 [3,100,5]、28388 维咒源阵 [3,100,30]、
//     28718 深渊落款 [3,100,29]、29335 斗争宣言 [2,100,2]、37039 晟锋断宵诀 [2,100,2]、
//     37540 无极星灵阵 [3,100,19]、38485 帝皇降魔歼灭斩 [3,100,31]…（全载体概率 100）
//
// 动工前置三查（2026-10-22）：①内核 grep 2040 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5390_opp_switch_anomaly.cpp（号段 5390-5399）。

#include "common.h"

namespace {

// ── 2040 —— {0}回合内对手主动切换精灵出战时{1}%{2} ──
// int_args = [owner, target, 回合, 概率, 异常码]（args_num=3）。
EffectResult effect_2040_opp_switch_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2040, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2040, args,
                     "2040 布局=[owner,target,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2040, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 2040, args, "需要 回合 > 0");
    // 红线7：概率在位即消费（0 = 显式关闸档，仍注册窗、掷点必败——不得当非法参数）。
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 2040, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2040, args, "需要 异常码 ∈ [0,43]");
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false,
        [owner, target, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != target || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            // SWAP 已在 perform_switch 之后 emit：此时在场者就是登场的新宠。
            if (c->getPet(target).hp <= 0) {
                return;
            }
            (void)g_core->apply_anomaly_checked(c, target, anomaly, random_anomaly_duration(),
                                                owner, chance, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2040, effect_2040_opp_switch_anomaly)
