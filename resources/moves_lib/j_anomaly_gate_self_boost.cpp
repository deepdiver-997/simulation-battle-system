// J 组 J240 · 异常门自身强化族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   182（args_num=4）"若对手处于{0}状态，{2}%自身{1}等级+{3}"
//   —— {0}=官方异常码；{1}=官方能力码（0攻1防2特攻3特防4速，5=命中）；{2}=概率；{3}=增量。
//
// 实现口径：
//   - 门 = 对手指定异常槽 end_round 未到期。`has_active_abnormal_status` 是内核非
//     inline 符号、插件不可直调（core_api.h 注释）→ 用公开的 `abnormal_status_end_round`
//     数组自判（effect_batch8.cpp 同式）。
//   - 概率：{2}=100 直接放行；{2}<=0 显式关闸（在位即消费，不回落默认值——红线 7）；
//     其余走 roll_percent 申报前本地掷点（本宏官方模板无"申报"字样，与 182 族同）。
//   - 落点 = `stat_change(自身, 槽, +{3})` 真原语（本体/视图同步；自身增益不吃免弱）。
//     {1} 是**官方码序**（0攻1防2特攻3特防4速）→ 引擎槽 {0,2,1,3,4,5}（473/585 同款）。
//   - 时点 = SKILL_EFFECT：19682 的特攻 +2 会在同一刀的 ATTACK_DAMAGE 前生效
//     （stat_change 同步 ws.view_levels，伤害公式读视图）——官方"使用技能时强化，
//     本次伤害即吃强化"的直读口径。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   12999 末日暗影 [182,15]|[6,0,100,1,15]（害怕→攻+1）
//   13005 烈日千阳 [182,22]|[7,4,100,1,15,2]（疲惫→速+1）
//   **13070 混沌冰暴 [182]|[5,4,100,1]（冻伤→速+1，纯链，场景载体）**
//   13834 玄冰疾刺 [182]|[5,4,100,1]
//   14647 寒光凝结波 [182]|[5,4,100,1]
//   15704 圣光烈阳破 [57,182]|[5,8,2,0,100,1]（火系→攻+1；57 占 2 参）
//   17298 王·帝君审判 [687,182]|[2,50,2,0,100,2]（烧伤→攻+2；687 占 2 参）
//   **19682 风雪打击 [182]|[5,2,100,2]（冻伤→特攻+2，纯链，场景载体）**
//
// 动工前置三查（2026-10-22）：①内核 grep 182 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5710_anomaly_gate_self_boost.cpp（号段 5710-5719）。

#include "common.h"

namespace {

// "处于{status}异常"= 该异常槽 end_round 未到期（effect_batch8.cpp 同式）。
bool has_anomaly_active(BattleContext* ctx, int side, int status_id) {
    return ctx && side >= 0 && side <= 1 && status_id >= 0
        && status_id < kOfficialAbnormalStatusSlotCount
        && ctx->roundCount < ctx->abnormal_status_end_round[side][status_id];
}

// 官方能力码（0攻1防2特攻3特防4速5命中）→ 引擎槽（0攻1特攻2防3特防4速5命中）。
int official_stat_to_engine_slot(int official) {
    switch (official) {
        case 0: return 0;
        case 1: return 2;
        case 2: return 1;
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        default: return -1;
    }
}

// ── 182 —— 若对手处于{0}状态，{2}%自身{1}等级+{3} ──
EffectResult effect_182_anomaly_gate_self_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 182, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 182, args,
                     "182 布局=[owner,target,status,stat,chance,levels]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int status = args.int_args[2];
    const int official_stat = args.int_args[3];
    const int chance = args.int_args[4];
    const int levels = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 182,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(status), 182, args, "182 {0} 异常码越界");
    EFFECT_ARG_GUARD(levels > 0, 182, args, "182 {3} 增量需 > 0");
    const int slot = official_stat_to_engine_slot(official_stat);
    EFFECT_ARG_GUARD(slot >= 0, 182, args, "182 {1} 能力码超出官方码序（0-5）");

    if (!has_anomaly_active(ctx, opp, status)) {
        return EffectResult::kOk;   // 对手不处于指定异常
    }
    // 行参概率在位即消费：显式 0 关闸，100 直接放行，其余本地掷点。
    if (chance <= 0) {
        return EffectResult::kOk;
    }
    if (!roll_percent(chance)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(ctx, owner, slot, levels);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(182, effect_182_anomaly_gate_self_boost)
