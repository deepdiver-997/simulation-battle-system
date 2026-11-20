// J 板 J259 · 未满体力下回合减伤族 —— 1584×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 1584，args_num=1）：
//   "出手时若自身未满体力则下回合自身受到的攻击伤害减少{0}%"
//   {0}=减伤百分比；"未满体力"= 出手时当前体力 < 当前最大体力。
//
// 载体切片（side_effect.arg_count=1 顺序切分，8/8 自洽；纯链 35168）：
//   34223 镇魂之哀嚎 [50]（链上 1234/1273 在前）  34314 冠领千钧 [50]
//   34443 狂刃强袭 [50]  34489 毛绒冰球 [50]  34924 喜贺元宵 [50]
//   35000 九幽断川镰 [50]  35168 冥界之钟 [50]（纯链）  37992 全力助燃 [50]
//   ——全部载体 {0}=50。
//
// 实现口径（J217 1548 的镜像兄弟：同为"下回合减伤窗"，门不同）：
//   - 门 = SKILL_EFFECT 时点判 `getPet(owner).hp < numericalBase[HP]`（出手时未满体力）。
//     与 1548 的 AFTER_ACTION "未击败"门不同：本宏在技能效果时点即可定门，不需要
//     当回合伤害落地后的探针。
//   - 门过 → register_next_rounds_effect(ROUND_START, rounds=1)（NextRounds 窗界：
//     R+1 起算、不含施放当回合），tick 向 ws.damage_reduce_add[owner] 首空槽写 +{0}
//     （加算减伤百分比：正值=减伤，calculation.h applyDamageReduction 钳 ±100，
//     4 槽跨槽求和）。红伤专用通道——粉伤走独立管线，天然不受影响。
//   - 门闭（满血）不注册；审计槽 996584100+owner 记门结果（1=门过/0=门闭）。
//   - {0}=0 在位即消费（红线7）：pct>=0，显式 0 仍注册窗、写 0 等价空槽。
//
// 动工前置三查（2026-10-22）：①内核 grep 1584 无行为硬编码 ②effect_info 模板唯一
//   args_num=1、custom_effect_programs/overrides 零行 ③全 worktree moves_lib/soul_lib
//   无 SKILL_EFFECT(1584) 注册——真缺口。
// 场景回归：test/scenario/scenario_5900_wounded_next_round_reduce.cpp（号段 5900-5909）。

#include "common.h"

namespace {

constexpr int k1584AuditBase = 996584100;   // +owner：1=未满体力门过 / 0=满血门闭

int j259_max_hp(BattleContext* ctx, int side) {
    return ctx->getPet(side).numericalBase[NumericalPropertyIndex::HP];
}

// ── 1584 第二跳：R+1 ROUND_START 写加算减伤槽（首空槽；正值=减伤）──
EffectResult j259_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[1];
    if (owner < 0 || owner > 1 || pct < 0) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 1584 —— 出手时若自身未满体力则下回合受到的攻击伤害减少{0}% ──
// int_args = [owner, target, 减伤%]（args_num=1）。
EffectResult effect_1584_wounded_next_round_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1584, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1584, args,
                     "1584 布局=[owner,target,减伤%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1584, args, "需要 owner/target ∈ {0,1}");
    // 红线7：百分比在位即消费（0 = 显式关闸档，仍注册窗；写 0 等价空槽）。
    EFFECT_ARG_GUARD(pct >= 0, 1584, args, "需要 减伤% >= 0");

    const bool wounded = ctx->getPet(owner).hp < j259_max_hp(ctx, owner);
    ctx->plugin_storage[k1584AuditBase + owner] = wounded ? 1 : 0;
    if (!wounded) {
        return EffectResult::kOk;   // 满血门闭：不武装下回合窗
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1584, /*rounds=*/1,
                                {owner, pct}, &j259_reduce_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1584, effect_1584_wounded_next_round_reduce)
