// J 组 J22 · 属性技响应/攻击技窗族 —— 2 宏/46 载体（2026-10-02，worktree ../agents/j22）
//
// 家族语义（effect_info 模板即权威）：
//   718 [2] {0}回合内对手使用属性技能，则自身下{1}回合必定暴击
//   592 [3] 下{0}回合每回合使用攻击技能{1}%令对手{2}（{2}=异常码，实载含 0）
//
// 实现口径：
//   - 718 = EVENT_SKILL_USED watcher（469/E13 同通道：使用即触发，miss/被封属都算；
//     filter ev.target==self && ev.used_attribute——"对手使用属性技"字面）+ 731 必暴
//     授予形（ws.must_crit=true 的 ROUND_START tick）。触发时点在对手行动中 →
//     register_next_rounds_effect NextRounds({1})：起点 R+1，窗口 [R+1, R+{1}] 恰为
//     "下{1}回合"；重复触发 = 同键替换刷新（994 常驻节点口径）。
//   - 592 = 597 每回合窗族（FIRST/SECOND_AFTER_ACTION 双侧注册，桶只跑行动方——
//     52 教训，每回合恰一次）+ "下{0}回合" NextRounds 起点折算。tick 判
//     used_skill_this_round && selected_skill_is_attack（"攻击技能"=物理/特殊）→
//     J3 申报制 apply_anomaly_checked（{1}% 掷骰在原语内、亮节闸门管辖；
//     chance>=100 短路必发——载体 25747 [3,100,5] 测试即确定性）。
//
// 载体切片（unity 库 json_each 精确匹配 25+21=46；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   718  [rounds,crit_rounds]：38081 罪恶类列 [2,1]（+1088 已实现，atk=1 时休眠）、
//        23870 天法归元 [3,2]、37934 苦舟 [3,1]
//   592  [rounds,chance,异常码]：25747 毒灵妖魄 [3,100,5]（chance=100 测试载体）、
//        16149 圣甲雷霆闪 [2,30,0]（{2}=0 麻痹）、23455 妖月之祭 [3,40,15]
//
// 动工前置三查（2026-10-02）：①内核 grep 2 id 无硬编码 ②custom_effect_programs/
// overrides 无声明 ③moves_lib 无既有注册——真缺口（EVENT_SKILL_USED 与 ws.must_crit
// 为既有内核设施：E13/731 先例）。
// 场景回归：test/scenario/scenario_1070_attr_response_window.cpp（号段 1070-1079）。

#include "common.h"

namespace {

// ── 718 的必暴 tick —— payload={owner}；ws 每回合 reset → 窗口每回合重写（731 同款）。
EffectResult j22_718_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// ── 718 —— {0}回合内对手使用属性技能，则自身下{1}回合必定暴击 ──
// int_args = [owner, target, rounds, crit_rounds]（args_num=2）。
EffectResult effect_718_attr_use_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 718, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 718, args,
                     "718 布局=[owner,target,回合,必暴回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int crit_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     718, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && crit_rounds > 0, 718, args, "需要 回合/必暴回合 > 0");
    // 监听对手技能使用（TEAM scope 跨回合，469 同参；"{0}回合内"含当回合）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, crit_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || !ev.used_attribute) {
                return;   // 条件：对手使用的是属性技（使用即触发）
            }
            const int user = ev.actor;
            if (user < 0 || user > 1 || user == owner) {
                return;
            }
            // "下{1}回合必暴"：NextRounds 起点 R+1（触发发生在对手行动中）。
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, owner, 718, crit_rounds,
                                        {owner}, &j22_718_must_crit_tick);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 592 的 AFTER_ACTION tick —— payload={owner, target, chance, anomaly}。
EffectResult j22_592_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // "每回合使用攻击技能"：本回合真点了技能且是攻击技（物理/特殊）。
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                            owner, chance, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 592 —— 下{0}回合每回合使用攻击技能{1}%令对手{2} ──
// int_args = [owner, target, rounds, chance, anomaly]（args_num=3）。
EffectResult effect_592_attack_window_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 592, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 592, args,
                     "592 布局=[owner,target,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     592, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0, 592, args, "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 592, args, "需要 异常码 ∈ [0,43]");
    // "下{0}回合"：NextRounds 起点折算；AFTER_ACTION 桶按行动方执行（52 教训），
    // 双侧注册每回合恰跑一次（597 窗族同构）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(ctx, st, owner, 592, rounds,
                                    {owner, target, chance, anomaly}, &j22_592_round_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(718, effect_718_attr_use_must_crit)
SKILL_EFFECT(592, effect_592_attack_window_anomaly)
