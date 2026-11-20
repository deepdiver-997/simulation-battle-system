// J 组 J119 · 条件恢复族 —— 704×13 / 1635×13（2026-10-03，worktree ../agents/j119）
//
// 家族语义（effect_info 模板即权威）：
//   704 （args_num=2）"恢复{0}点体力，若当前体力低于{1}则直接回满"
//     —— 烈阳瞬闪 32611 [250,250]，4399 原文同文（"恢复250点体力，若当前体力低于250
//        则直接回满"）。口径记档："直接回满"取**替换**读法（hp<{1} → 只回满、不再叠
//        {0}；"直接"修饰跳过常规恢复），门在恢复前求值——[250,250] 载体按序列后判
//        读法会使后半句在多数血量下不可达，非官方意图。
//   1635（args_num=2）"立刻恢复自身{0}点体力，{1}回合后恢复自身全部体力"
//     —— 誓言之约 27399 [200,5]（属），moves.info 同文。
//
// 实现口径：
//   - 704 = hp < {1} 门 → heal_max_hp_fraction(1)（common.h 回满=heal(maxHP)，吃封回血/
//     恢复修正）；否则 g_core->heal_amount({0})（原语，返回三态）。
//   - 1635 = heal_amount({0}) 立刻 + **NextRounds({1}) ROUND_START tick**（J61/J70
//     NextRounds 同族），payload 带 deadline=arm_round+{1}：tick 内 roundCount<deadline
//     跳过（防同回合早执行 + 窗内逐回合跳），恰在 R+{1} 的回合开始回满（heal_max_hp_
//     fraction(1)）。rounds={1} 窗宽恰好覆盖到期回合，到期后窗自然过期。
//   - 换宠口径记档：ContinuousEffect 随 owner 宠走（下场即失），官方"回合后回满"对
//     换宠行为无语料，按引擎窗口模型落地。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-03，26/26 自洽）：
//   704  [点数,阈值]：32611 烈阳瞬闪 [250,250]（纯链）、21729 [0,1]、22987 [100,100]…
//   1635 [点数,回合]：27399 誓言之约 [200,5]（纯链属）、28154 对成霜 [350,2]、
//        28273 幽皑之阵 [300,3]、29331 聆主天答 [777,3]…
//
// 动工前置三查（2026-10-03）：①内核 grep 704/1635 无硬编码 ②effect_meta 无分类牵连
// （Heal 分类仅文档性）③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2130_cond_heal.cpp（号段 2130-2139）。

#include "common.h"

namespace {

// ── 1635 的 ROUND_START tick —— payload={owner, deadline}；到期回满，未到跳过。
EffectResult j119_delayed_full_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int deadline = args.int_args[1];
    if (owner < 0 || owner > 1 || ctx->roundCount < deadline) {
        return EffectResult::kOk;   // 窗内未到期（NextRounds 起点后逐回合跳）
    }
    (void)heal_max_hp_fraction(ctx, owner, 1);
    return EffectResult::kOk;
}

// ── 704 —— 恢复{0}点体力，若当前体力低于{1}则直接回满 ──
// int_args = [owner, target, 点数, 阈值]（args_num=2）。
EffectResult effect_704_cond_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 704, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 704, args,
                     "704 布局=[owner,target,点数,阈值]");
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && amount > 0 && threshold > 0, 704, args,
                     "需要 owner∈{0,1} 点数/阈值>0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp < threshold) {
        (void)heal_max_hp_fraction(ctx, owner, 1);   // "直接回满"（替换读法，见头注）
    } else {
        (void)g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

// ── 1635 —— 立刻恢复自身{0}点体力，{1}回合后恢复自身全部体力 ──
// int_args = [owner, target, 点数, 回合]（args_num=2）。
EffectResult effect_1635_delayed_full_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1635, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1635, args,
                     "1635 布局=[owner,target,点数,回合]");
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && amount > 0 && rounds > 0, 1635, args,
                     "需要 owner∈{0,1} 点数/回合>0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, amount);   // 立刻恢复
    const int deadline = ctx->roundCount + rounds;
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1635, rounds,
                                      std::vector<int>{owner, deadline},
                                      &j119_delayed_full_heal_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(704, effect_704_cond_heal)
SKILL_EFFECT(1635, effect_1635_delayed_full_heal)
