// J 组 J265 · 消除回合类恢复族 —— 1 宏/7 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1877 [2] 消除对手回合类效果且恢复自身{0}点体力，消除成功则额外吸取对手{1}点体力
//
// 实现口径（先例：J1 j_purge_round.cpp 856/1523 的 break_round_effects + 后缀门槛；
// 1523 点数吸取的"实际扣减回血"；heal_amount 原语吃封回血/恢复修正）：
//   1) break_round_effects(对手)（内核断回合原语，吃免断 + EVENT_BREAK）：
//      SUCCESS=清掉了东西 / NO_EFFECTS=本来没有 / IMMUNE=免断。
//   2) 基础恢复**无条件**：heal_amount(自身, {0}) —— 模板"消除…且恢复…"是并列句，
//      与"消除成功则 X"的 J1 门槛族不同（{0} 是点数，0=显式不恢复，按数量语义跳过）。
//   3) 额外吸取**仅 SUCCESS**：fixed_damage(对手, {1}) 后按**实际扣减量**回血
//      （1523 口径；固定伤害吃护盾/固定抗性，与 1523 的直写 hp 差异记档）。
//   4) 审计槽（场景精确断门，9xxxx 私有段）：
//        991877100 + owner = 断回合结果（0=NONE / 1=SUCCESS / 2=IMMUNE）
//        991877200 + owner = 基础恢复实际量
//        991877300 + owner = 额外吸取实际落地量（0=未成功/0 点）
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1877 [恢复,吸取]：36199 奈落纠缠 [250,250]、36529 凛冬狂镰扫 [200,100]、
//     36599 魔动聚能击 [250,250]、37109 净眸清灵语 [300,300]、37304 秽灵魅语 [300,300]、
//     38026 倩影揽滔潮 [300,300]、38380 对等交流 [300,200]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1877 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1877) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5960_purge_round_heal.cpp（号段 5960-5969）。

#include "common.h"

namespace {

constexpr int kAuditBreak = 991877100;   // +owner：0=NONE / 1=SUCCESS / 2=IMMUNE
constexpr int kAuditHeal = 991877200;    // +owner：基础恢复实际量
constexpr int kAuditDrain = 991877300;   // +owner：额外吸取实际落地量

// ── 1877 —— 消除对手回合类效果 + 恢复自身{0}，消除成功额外吸取对手{1} ──
// int_args = [owner, target, 恢复点数, 吸取点数]（args_num=2）。
EffectResult effect_1877_purge_heal_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1877, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1877, args,
                     "1877 布局=[owner,target,恢复点数,吸取点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int heal = args.int_args[2];
    const int drain = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1877, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(heal >= 0, 1877, args, "需要 恢复点数 >= 0");
    EFFECT_ARG_GUARD(drain >= 0, 1877, args, "需要 吸取点数 >= 0");

    int break_state = 0;   // 0=NONE/未执行，1=SUCCESS，2=IMMUNE
    if (g_core && g_core->break_round_effects) {
        const BreakResult r = g_core->break_round_effects(ctx, target);
        if (r == BreakResult::SUCCESS) {
            break_state = 1;
        } else if (r == BreakResult::IMMUNE) {
            break_state = 2;
        }
    }
    ctx->plugin_storage[kAuditBreak + owner] = break_state;

    // 基础恢复：并列句，无条件（{0}=0 是显式不恢复）。
    int healed = 0;
    if (heal > 0 && g_core && g_core->heal_amount) {
        const int before = ctx->getPet(owner).hp;
        (void)g_core->heal_amount(ctx, owner, heal);
        healed = ctx->getPet(owner).hp - before;
    }
    ctx->plugin_storage[kAuditHeal + owner] = healed;

    // 额外吸取：仅断回合成功；按实际扣减量回血（1523 口径）。
    int dealt = 0;
    if (break_state == 1 && drain > 0 && g_core && g_core->fixed_damage
        && ctx->getPet(target).hp > 0) {
        const int before = ctx->getPet(target).hp;
        (void)g_core->fixed_damage(ctx, target, drain);
        dealt = before - ctx->getPet(target).hp;
        if (dealt > 0 && g_core->heal_amount) {
            (void)g_core->heal_amount(ctx, owner, dealt);
        }
    }
    ctx->plugin_storage[kAuditDrain + owner] = dealt;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1877, effect_1877_purge_heal_drain)
