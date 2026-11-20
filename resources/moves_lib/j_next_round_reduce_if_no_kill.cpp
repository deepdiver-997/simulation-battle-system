// J 组 J217 · 未击败下回合减伤族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1548 [1] 当回合未击败对手则使自身下回合受到的伤害降低{0}%
//
// 实现口径（batch_b 1171 未击败→NextRounds 直系，减伤改百分比槽）：
//   - AFTER_ACTION 探针（当回合伤害落地后可知"未击败"；1218/984/1171 同归位）：
//     探针只注册在本方名下、且只在本方行动时执行（bucket 按 acting side 跑）→
//     对侧 AFTER_ACTION 天然不触发。探针自身 duration=1（当回合有效，回合末清理）。
//   - 未击败判据 = 对手在场宠 hp > 0（死亡结算在本状态之后，AFTER_ACTION 读到 0
//     即本回合已击败）。
//   - 减伤窗 = register_next_rounds_effect(ROUND_START, rounds=1)：R+1 回合开始写
//     ws.damage_reduce_add 首空槽 **+{0}**（加算减伤百分比：正值=减伤，
//     calculation.h applyDamageReduction 的算术对称钳 ±100；4 槽跨槽求和）。
//   - {0}=0 在位即消费（写 0 等于空槽不生效，不拒绝参数）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 9/9 自洽）：
//   1548 [减伤%]：33983 轮回证道破 [50]、34092 哀·蝠翼风暴 [50]、34128 甜俏枝蔓展 [50]、
//     34349 哀·蝠群蔽日 [50]、34605 皓腕凝霜舞 [50]、34678 摄心幻灵目 [50]、
//     35539 战威圣骑踏 [50]、36485 夜灵闪光 [50]（纯链）、36487 异火焚林 [50]
//     ——全载体 50%。
//
// 动工前置三查（2026-10-22）：①内核 grep 1548 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5440_next_round_reduce_if_no_kill.cpp（号段 5440-5449）。

#include "common.h"

namespace {

// ── 1548 第二跳：R+1 ROUND_START 写加算减伤槽（首空槽；正值=减伤）──
EffectResult j217_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[1];
    if (owner < 0 || owner > 1 || pct < 0) {
        return EffectResult::kOk;
    }
    int(&slots)[2][4] = ctx->ws.damage_reduce_add;
    for (int i = 0; i < 4; ++i) {
        if (slots[owner][i] == 0) {
            slots[owner][i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 1548 —— 当回合未击败对手则使自身下回合受到的伤害降低{0}% ──
// int_args = [owner, target, 减伤%]（args_num=1）。
EffectResult effect_1548_reduce_if_no_kill(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1548, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1548, args,
                     "1548 布局=[owner,target,减伤%]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1548, args, "需要 owner/target ∈ {0,1}");
    // 红线7：百分比在位即消费（0 = 显式关闸档，仍注册窗；写 0 等价空槽）。
    EFFECT_ARG_GUARD(pct >= 0, 1548, args, "需要 减伤% >= 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1548, 0, owner, -1, EffectArgs(std::vector<int>{owner, pct}),
                     [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                         if (!c || !a.int_args || a.int_count < 2) {
                             return EffectResult::kOk;
                         }
                         const int self = a.int_args[0];
                         const int val = a.int_args[1];
                         if (c->getPet(1 - self).hp <= 0) {
                             return EffectResult::kOk;   // 本回合已击败对手 → 门闭
                         }
                         register_next_rounds_effect(
                             c, State::BATTLE_ROUND_START, self, 1548, /*rounds=*/1,
                             {self, val}, &j217_reduce_tick);
                         return EffectResult::kOk;
                     });
        // duration=1：探针只在当回合有效（回合末自然清理），下一回合不重复给窗。
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1548;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1548, effect_1548_reduce_if_no_kill)
