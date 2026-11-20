// J 组 J31 · 消回反转/攻击值附伤/下只先制族 —— 3 宏/66 载体（2026-10-02，worktree ../agents/j31）
//
// 家族语义（effect_info 模板 + 4399 双源）：
//   1878 [1] 消除对手回合类效果，若对手不处于回合类效果则吸取对手最大体力的1/{0}
//   1460 [3] 附加{0}{1}值{2}%的百分比伤害，先出手时附加效果翻倍
//   934  [2] 击败对手则对手下只出场精灵{0}回合内先制-{1}
//
// 实现口径：
//   - 1878 = break_round_effects(target) **==0 取反门**（797/959 消回门的镜像："对手不处于
//     回合类效果"= 没清掉东西）→ drain_hp(owner, target, {0})（597 吸取口径：对手掉
//     最大体力 1/{0} 固定伤、自身回等量）。
//   - 1460 = {0}=归属（0=自身/1=对手，4399 实证：极速雷影[0,4]"自己速度值"、
//     死亡之镰[1,4]"对手速度值"）；{1}=**官方码序**属性码（0攻 1防 2特攻 3特防 4速，
//     593 {1} 码同序；祥兽聚福击[0,1]"自身防御值"实证）→ 引擎数值槽映射
//     {0:物攻, 1:防御, 2:特攻, 3:特防, 4:速度}，读 **视图值**（getTempAbilityValue，
//     2501/676 同口径）。通道 = 模板"百分比伤害" → PERCENT_VALUE（1231/1875 计算值
//     粉伤同通道）；⚠️ 记档：4399 的 25% 族（穿影背刺等）多写"固定伤害"——与模板
//     分歧，按模板权威取百分比通道（两者皆粉伤、吃护罩一致）。
//     落点 = EVENT_HIT watcher（4399"攻击命中后"= 命中才发；EVENT_SKILL_USED 是
//     "使用即触发"不合）filter ev.actor==owner → 值 ×{2}% ×（acting_side_is_first ? 2:1）
//     ——先手翻倍在命中时点判（DETECT/命中桶 FIRST_* 状态域）。
//   - 934 = AFTER_ACTION 击败门（J12 桥：我方行动结束后对手在场宠 hp≤0 = 击败）置
//     armed 旗标（plugin_storage）+ EVENT_ENTER_STAGE 常驻 watcher（K3 330 死亡继承
//     事件面同源）：armed 且入场方=对手侧 → 对该侧授 NextRounds {0} 回合的
//     MOVE_RIGHT tick（ws.preemptive_level[side] -= {1}，994 先制累加同写点）→
//     解除 armed。跨回合常驻 watcher（duration=0）。
//
// 载体切片（unity 库 json_each 精确匹配 20+23+23=66；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   1878 [分母]：36263 嗡嗡 [3]、36655 鲲·千里击涛 [4]
//   1460 [归属,属性码,pct]：33158 穿影背刺 [0,0,25]、33235 极速雷影 [0,4,30]、
//        37524 死亡之镰 [1,4,30]、34638 祥兽聚福击 [0,1,40]
//   934  [rounds,先制-]：19594 天斩一命决 [2,1]、36456 牧夫锐矛刺 [1,2]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 无既有注册——真缺口（EVENT_HIT/EVENT_ENTER_STAGE/beat 门均为既有设施）。
// 场景回归：test/scenario/scenario_1160_break_drain_defeat.cpp（号段 1160-1169）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// 934 的 MOVE_RIGHT tick —— payload={side, amount}；先制窗每回合重写（994 写点）。
EffectResult j31_934_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int side = args.int_args[0];
    const int amount = args.int_args[1];
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[side] += amount;   // 负值 = 先制-{1}
    return EffectResult::kOk;
}

// ── 1878 —— 消除对手回合类效果，若对手不处于回合类效果则吸取对手最大体力的1/{0} ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_1878_break_fail_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1878, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1878, args,
                     "1878 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1878, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1878, args, "需要 分母 > 0");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    // 消回门取反："不处于回合类效果" = NO_EFFECTS（IMMUNE=有但免断，不算不处于）。
    if (g_core->break_round_effects(ctx, target) != BreakResult::NO_EFFECTS) {
        return EffectResult::kOk;
    }
    if (g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/target, denom);
    }
    return EffectResult::kOk;
}

// ── 1460 —— 附加{0}{1}值{2}%的百分比伤害，先出手时附加效果翻倍 ──
// int_args = [owner, target, 归属, 属性码, pct]（args_num=3）。
EffectResult effect_1460_stat_value_percent_hit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1460, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1460, args,
                     "1460 布局=[owner,target,归属,属性码,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int whose = args.int_args[2];
    const int stat_code = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1460, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(whose == 0 || whose == 1, 1460, args, "需要 归属 ∈ {0=自身,1=对手}");
    EFFECT_ARG_GUARD(stat_code >= 0 && stat_code <= 4, 1460, args, "需要 属性码 ∈ [0,4]");
    EFFECT_ARG_GUARD(pct > 0, 1460, args, "需要 pct > 0");
    // 官方码序 → 引擎数值槽（0攻 1防 2特攻 3特防 4速）。
    int slot = -1;
    switch (stat_code) {
        case 0: slot = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK); break;
        case 1: slot = static_cast<int>(NumericalPropertyIndex::DEFENSE); break;
        case 2: slot = static_cast<int>(NumericalPropertyIndex::SPECIAL_ATTACK); break;
        case 3: slot = static_cast<int>(NumericalPropertyIndex::SPECIAL_DEFENSE); break;
        case 4: slot = static_cast<int>(NumericalPropertyIndex::SPEED); break;
        default: return EffectResult::kOk;
    }
    // "攻击命中后附加"——单次直附：能进 SKILL_EFFECT 桶 = 本次攻击已命中（731 记档：
    // miss/打盔不进 SKILL_EFFECT）。⚠️ 不用 EVENT_HIT watcher：它在 ON_SKILL_HIT 桶后
    // 已 drain（1036 记档），本次攻击收不到、持久注册又会跨回合误附（记档）。
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int stat_owner = (whose == 0) ? owner : target;
    const int value = ctx->ws.getTempAbilityValue(stat_owner,
                                                  static_cast<NumericalPropertyIndex>(slot));
    const bool first = acting_side_is_first(ctx);
    const int amount = value * pct / 100 * (first ? 2 : 1);
    if (amount > 0) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 934 —— 击败对手则对手下只出场精灵{0}回合内先制-{1} ──
// int_args = [owner, target, rounds, amount]（args_num=2）。
// 实现：EVENT_DEATH（对手侧宠死亡，实测必发）→ 当场预登记 NextRounds {0} 回合的
// MOVE_RIGHT tick（ws.preemptive_level[side] -= {1}，994 写点）。
// ⚠️ 窗绑定 **side** 而非具体宠：死亡补位不发 ENTER_STAGE/SWAP（J31 实测记档，
// perform_switch 的"切到同一只"早返回覆盖补位路径）→ "下只出场精灵"以 side 承载；
// 击杀回合的 MOVE_RIGHT 已过 → NextRounds 起点=R+1 = 下只首个完整回合。
// ⚠️ 归因记档：ev.target==opp 的任意死亡都武装（含反伤/流祈类非攻击击杀）——
// 官方"击败"的严格归因（最后一刀来源=自身）需 DEATH 事件带 actor，现无，从宽。
EffectResult effect_934_defeat_next_priority_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 934, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 934, args,
                     "934 布局=[owner,target,回合,先制降幅]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = -args.int_args[3];   // 载体恒正，下放负值
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     934, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && amount < 0, 934, args, "需要 回合>0 且 降幅>0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, rounds, amount](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp) {
                return;
            }
            // 击败当下预登记：NextRounds 起点=R+1（下只首个完整回合）。
            // 节点挂**本方**（owner）：对手侧节点会被死亡换宠的 on_stage 作废清掉
            // （invalidate_on_stage_effects，J31 实测坑）；payload 仍写对手槽位。
            register_next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, owner, 934, rounds,
                                        {opp, amount}, &j31_934_priority_tick);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1878, effect_1878_break_fail_drain)
SKILL_EFFECT(1460, effect_1460_stat_value_percent_hit)
SKILL_EFFECT(934, effect_934_defeat_next_priority_down)
