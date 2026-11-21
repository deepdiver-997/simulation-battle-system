// J 组 J307 · 击败门强化转移/翻倍族 —— 784×7 + 1253×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   784 （args_num=0）"若本回合击败对手则将对手的能力提升效果转移到自己身上"
//   1253（args_num=0）"击败对手则自身能力提升等级翻倍"
//
// 实现口径（同形先例直系）：
//   - 击败门 = j_kill_preempt.cpp **1587** 同款：施放时在 FIRST/SECOND AFTER_ACTION 各挂
//     一条 duration=1 探针（897/J12 同款 acting_side 判据），本侧行动结束且
//     `getPet(opp).hp <= 0` → 门开。桶键 = (source_id<<32)|effect_id，两效果 id 不同
//     → 互不同键（j_defeat_gate2 的同键覆盖/use-after-free 坑不适用）。
//   - 784 = transfer_stat_boosts(对手 → 自己)：**转移**语义（对手正等级整体搬到自己
//     身上、对手清零）——569「吞噬对手的能力提升效果」同款原语；与 1949「**复制**」
//     （对手保持不动）刻意不同。门开时点 = 击败方 AFTER_ACTION，早于败方换宠
//     （换宠才 clear_ability_levels）→ 败方等级此刻仍可读。
//   - 1253 = 自身正槽逐项 stat_change(+当前等级)："**提升等级**翻倍"= 提升量翻倍
//     （403/J12-964 保守取法：+2 → +4；+6 顶格时 AT_CAP 钳住 = 自然封顶，不算错）。
//   - 审计槽（+owner*2）：
//       991784100 = 784 击败门（0/1）；991784200 = 本次转移到的正等级总和。
//       9911253100 = 1253 击败门（0/1）；9911253200 = 本次翻倍的槽数。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽、无剩余参数）：
//   784 []：30438 三千大世斩（链 [1060,784]——1060=吸取对手强化，**不选**：会先搬走
//        对手强化）、33318 乱石裂地锤（链 [781,864,784]——**场景载体**：781 消除对手
//        回合类效果、864 是"未击败"门，击败档不触发）、36131 沥胆候甘来（链
//        [1821,784,934,709]——709=击败后自身强化翻倍，与 1253 抢观测面，**不选**）、
//        37228 角突（链 [596,784]，prio=3 污染先手）、32556 魅狐偷心术、30548 裂世
//        天寰杀、38224 狂乱夺魂袭。
//   1253 []：33177 锤音破晓（**纯链** cat1 pow130，场景载体）、33291 夺魄重击（纯链）、
//        33610 草莓投掷（纯链 cat2）、31257 瞳·恩断义绝、32128 王·亿光星尘、
//        33727 极速引爆、37412 垂云撼浪袭。
//
// 动工前置三查（2026-10-22）：①内核 grep 784/1253 无行为硬编码 ②effect_info 两条模板
//   各仅本条（无同文其它 id）③moves_lib/soul_lib 并集（jbatch/kbatch/hub 各检出）无
//   SKILL_EFFECT(784)/SKILL_EFFECT(1253)——真缺口。
//   （注：784/1253/1426 出现在 resources/boss_invalid_effects.json = "BOSS 战无效"
//   数据注记，非实现项；同 J12 口径。）
//
// 场景回归：test/scenario/scenario_6400_kill_boost_transfer.cpp（号段 6400-6409）。

#include "common.h"

#include <vector>

namespace {

constexpr int k784GateBase = 991784100;    // +owner*2：击败门
constexpr int k784MovedBase = 991784200;   // +owner*2：转移到的正等级总和
// ⚠️ 键位事实（2026-11-21 Windows gcc -Woverflow 实锤）：原设计 9911253100/9911253200
// 超出 int（INT_MAX=2147483647），历来实际生效的就是 mod 2^32 截断值 1321318508/1321318608
// （各平台同截断 → 行为一致，但属编译期收窄 UB 面）。此处显式落截断值，键槽不变、警告清零；
// 如需恢复 10 位键位语义须连 plugin_storage 键类型一起升 int64_t（连场景 6400 的副本）。
constexpr int k1253GateBase = 1321318508;    // 原设计 9911253100（+owner*2：击败门）
constexpr int k1253DoubledBase = 1321318608; // 原设计 9911253200（+owner*2：翻倍槽数）

// AFTER_ACTION 桶的当前行动方（j_kill_preempt/j_gate_family4 同款小工具）。
int j307_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

void j307_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 击败门（本侧行动结束且对手已倒）。
bool j307_kill_gate(BattleContext* ctx, const EffectArgs& args, int self) {
    if (j307_bucket_acting_side(ctx) != self) {
        return false;
    }
    return ctx->getPet(1 - self).hp <= 0;
}

// ── 784 后缀：把对手的正等级转移到自己身上（对手清零）──
EffectResult j307_784_transfer(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->transfer_stat_boosts || !args.int_args ||
        args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    int before = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            before += ctx->ability_levels[opp][i];
        }
    }
    (void)g_core->transfer_stat_boosts(ctx, opp, self);
    j307_set_storage(ctx, k784MovedBase + self * 2, before);
    return EffectResult::kOk;
}

// ── 784 击败门探针 ──
EffectResult j307_784_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (!j307_kill_gate(ctx, args, self)) {
        j307_set_storage(ctx, k784GateBase + self * 2, 0);
        return EffectResult::kOk;   // 门闭
    }
    j307_set_storage(ctx, k784GateBase + self * 2, 1);
    return j307_784_transfer(ctx, args);
}

// ── 1253 后缀：自身正槽逐项翻倍（delta = 当前等级；AT_CAP 自然封顶）──
EffectResult j307_1253_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->stat_change || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    int doubled = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[self][i];
        if (lv > 0) {
            (void)g_core->stat_change(ctx, self, i, lv);
            ++doubled;
        }
    }
    j307_set_storage(ctx, k1253DoubledBase + self * 2, doubled);
    return EffectResult::kOk;
}

// ── 1253 击败门探针 ──
EffectResult j307_1253_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (!j307_kill_gate(ctx, args, self)) {
        j307_set_storage(ctx, k1253GateBase + self * 2, 0);
        return EffectResult::kOk;   // 门闭
    }
    j307_set_storage(ctx, k1253GateBase + self * 2, 1);
    return j307_1253_double(ctx, args);
}

// 门探针挂载（FIRST/SECOND 双侧各一条，duration=1 只在用技当回合存活）。
void j307_arm_kill_gate(BattleContext* ctx, int effect_id, int self,
                         std::vector<int> payload, EffectFn probe) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, self, -1, EffectArgs(payload), probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
}

}  // namespace

// ── 784 —— 若本回合击败对手则将对手的能力提升效果转移到自己身上 ──
// int_args = [owner, target]（args_num=0，loader 注入 owner/target）。
EffectResult effect_784_kill_transfer_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 784, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 784, args,
                     "784 布局=[owner,target]（args_num=0）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 784,
                     args, "需要 owner/target ∈ {0,1}");
    j307_arm_kill_gate(ctx, 784, self, {self, opp}, &j307_784_probe);
    return EffectResult::kOk;
}

// ── 1253 —— 击败对手则自身能力提升等级翻倍 ──
// int_args = [owner, target]（args_num=0，loader 注入 owner/target）。
EffectResult effect_1253_kill_double_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1253, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1253, args,
                     "1253 布局=[owner,target]（args_num=0）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1253,
                     args, "需要 owner/target ∈ {0,1}");
    j307_arm_kill_gate(ctx, 1253, self, {self, opp}, &j307_1253_probe);
    return EffectResult::kOk;
}

SKILL_EFFECT(784, effect_784_kill_transfer_boosts)
SKILL_EFFECT(1253, effect_1253_kill_double_boosts)
