// J 组 J268 · 双方护盾总量回血族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1497 [0] 恢复自身n点体力（n=双方当前护盾值总和）
//
// 实现口径（先例：artemis_skills.cpp 2198 的 shield_bank_.total() 读数；
// common.h heal_amount 恢复原语）：
//   - n = 自身 shield_bank_.total() + 对手 shield_bank_.total()（护盾 shield_bank_；
//     **不含护罩 hood**——两者是独立实体，模板字面"护盾"）。
//   - 恢复走 heal_amount(自身, n)：吃封回血 / 恢复%修正 / 上限钳制；护盾本身**不消耗**。
//   - 时点 = SKILL_EFFECT（本次攻击落地前），故读的是**本回合攻击前**的护盾值。
//   - 审计槽（场景精确断言）：991497200 + owner = 名义 n；
//     991497300 + owner = 实际恢复量（heal_amount 后体力差，满血封顶时小于 n）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1497 []（零参）：28618 萧风扬尘、33706 落花无息、33794 岩盾冲击、35634 尘沙气旋
//     （纯链）、35715 焰烬凝盾、37173 默日悲音、37426 悠古石铭、37777 垒间窥遨游
//     （8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1497 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1497) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5990_shield_sum_heal.cpp（号段 5990-5999）。

#include "common.h"

namespace {

constexpr int kAuditNominal = 991497200;   // +owner：名义 n（双方护盾总和）
constexpr int kAuditHealed = 991497300;    // +owner：实际恢复量

// ── 1497 —— 恢复自身 n 点体力（n=双方护盾值总和） ──
// int_args = [owner, target]（args_num=0）。
EffectResult effect_1497_shield_sum_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1497, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1497, args,
                     "1497 布局=[owner,target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1497, args, "需要 owner/target ∈ {0,1}");
    const int nominal = ctx->getPet(owner).shield_bank_.total()
                      + ctx->getPet(target).shield_bank_.total();
    ctx->plugin_storage[kAuditNominal + owner] = nominal;
    int healed = 0;
    if (nominal > 0 && g_core && g_core->heal_amount) {
        const int before = ctx->getPet(owner).hp;
        (void)g_core->heal_amount(ctx, owner, nominal);
        healed = ctx->getPet(owner).hp - before;
    }
    ctx->plugin_storage[kAuditHealed + owner] = healed;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1497, effect_1497_shield_sum_heal)
