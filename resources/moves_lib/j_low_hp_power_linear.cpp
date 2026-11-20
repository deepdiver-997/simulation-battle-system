// J 组 J234 · 残血威力线性族 —— 1 宏/12 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   100 [0] 自身体力越少则威力越大
//
// 实现口径（直系先例 crawford_skills.cpp 1444 同文零参）：
//   - SKILL_EFFECT 时点 ws.skill_power_view 已被引擎物化为基线威力 → 按"已损失体力比例"
//     线性加算：view += base × (max_hp - hp) / max_hp（operator+= 自动打 rewritten →
//     走标准变威力重算管线；1252/1444 同通道）。
//   - base 取本次执行技能槽的静态威力（roundChoice[..][1]；替换技场景与 1444 同口径）。
//   - 满血（lost=0）与 base<=0 不改写。max 用 max_hp_of（common.h）。
//   - 零参效果（args_num=0 → 装载器注入 owner/target）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 12/12 自洽，全部 0 参）：18351 致命扭曲（纯链 pow150，场景载体）/ 33077 暗凌波（120）/
//   11703 凶险幻象 / 12101 搏杀意志 / 12283 背水之击 / 12624 嗜血攻击 / 13086 愤怒猛冲 /
//   15539 汹涌浪花 / 34569 绝命反扑 / 34650 万圣夜之梦（带链兄弟）/ 35008 精神封锁 /
//   35070 圣火攻心。
//
// 动工前置三查（2026-10-22）：①内核 grep 100 无硬编码（仅数字噪声）②effect_info 与
//   1444 同文异构（1444 已实现于 crawford_skills.cpp，公式即本族口径）③moves_lib 并集
//   （jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5650_low_hp_power_linear.cpp（号段 5650-5659）。

#include "common.h"

namespace {

// ── 100 —— 自身体力越少则威力越大（线性）──
// int_args = [owner, target]（0 参效果）。
EffectResult effect_100_low_hp_power_linear(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 100, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 100, args,
                     "100 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 100, args, "owner 越界");
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(owner).skills.size())) {
        return EffectResult::kOk;
    }
    const int base = ctx->getPet(owner).skills[static_cast<std::size_t>(idx)].power;
    const int max_hp = max_hp_of(ctx, owner);
    const int lost = max_hp - ctx->getPet(owner).hp;
    if (base <= 0 || lost <= 0 || max_hp <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_power_view[owner] += base * lost / max_hp;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(100, effect_100_low_hp_power_linear)
