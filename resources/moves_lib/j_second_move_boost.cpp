// J 组 J219 · 后手增伤族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1718 [2] 后出手时造成的攻击伤害提升{0}%，若对手当前体力值高于自身则伤害额外提升{1}%
//
// 实现口径：
//   - SKILL_EFFECT 时点判 acting_side_is_first(ctx)：先手直接不武装，只有后手武装。
//   - 武装 = 注册 DETECT 攻方桶（register_damage_effect，1481/J196 同通道）：自卡本次
//     红伤 attacker==owner / defender==target / isRed / final>0。
//   - 乘区 delta = {0} + (对手当前体力 > 自身当前体力 ? {1} : 0)；int 乘区统一走
//     checked_pct_mul（溢出响亮 throw，不绕）。
//   - 行参在位即消费：显式 0 也参与计算与审计，不用 >0 守卫关闸（红线 7）。
//   - 审计槽 plugin_storage[990171800 + owner*2] = 本次总乘区（100+delta）；+self*2
//     避免跨侧相邻键撞键（J207 勘误口径）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 脚本核算）：
//   1718 [pct, extra] 9/9 自洽：
//   35288 [100,50] / 36671 [50,50] / 37484 [50,50] / 37651 [50,50] / 37859 [100,50] /
//   37964 [100,50] / 38032 [50,50] / 38152 [25,50] / 38169 [100,100]
//   注：36671 炫色星辰 priority=3（天然先手），不适合做"后手"场景载体；场景取
//   37484 破灭天启剑（物 160 priority=0，链兄弟 1589 无异常即空跑、1304 只挂下回合）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1718 无硬编码 ②effect_info 仅 1686 同文孪生
// （先手版，已实现于 lingchao_skills.cpp，注册键 1686）③moves_lib 并集（含 kbatch/
// agents 各 worktree）无 1718 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5480_second_move_boost.cpp（号段 5480-5489）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <numerical-calculation/calculation.h>

namespace {

constexpr int k1718AuditBase = 990171800;   // + owner*2：本次总乘区（100+delta）

// ── 1718 —— 后出手时造成的攻击伤害提升{0}%，对手体力高于自身时额外提升{1}% ──
// int_args = [owner, target, pct, extra]（args_num=2）。
EffectResult effect_1718_second_move_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1718, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1718, args,
                     "1718 布局=[owner,target,pct,extra]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int extra = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1718, args, "需要 owner/target ∈ {0,1}");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 仅后出手时生效
    }
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, pct, extra](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤判定
            }
            int delta = pct;
            if (c->getPet(target).hp > c->getPet(owner).hp) {
                delta += extra;   // 对手当前体力更高 → 额外提升
            }
            c->plugin_storage[k1718AuditBase + owner * 2] = 100 + delta;
            if (delta != 0) {
                c->resolvedDamage.final =
                    checked_pct_mul(c->resolvedDamage.final, delta, "effect_1718");
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1718, effect_1718_second_move_boost)
