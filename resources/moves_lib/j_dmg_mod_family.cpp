// J20 · 承伤/输出修正族 —— 1265×22 / 566×22 / 567×22（2026-10-02，worktree ../agents/j20）
//
// 家族语义（effect_info 官方模板），三条都是"伤害公式侧的修正"：
//
// 1265（arg_count=2）"下{0}次受到的攻击伤害减少{1}%"：
//   - **897 计数窗直系**（f_onhit_family.cpp effect_897_arm_counted_reduce 逐行同构）：
//     register_damage_effect(REDUCE_PCT, self, MITIGATE) 回调 + shared_ptr 计数按次
//     消费；只管"自己受到的攻击红伤"（isRed、非属性直伤、final>0 才消费——
//     1984 同判据）；公式 d.final -= d.final*pct/100。
//   - 实例：虚无残魂 26650 [1,50]、神之守卫 25973 [1,50]（全库 22 载体，全部属性技
//     载体——自身无红伤，武装干净）。
// 566（arg_count=1）"攻击时忽略对手{0}%的防御值"：
//   - 落点 = `ws.defense_ignore_pct[**受击方**]`（calculation.h:32 在读防御值前折减；
//     物理走防御、特殊走特防——886 镫戒同槽位）。"攻击时" = 本次攻击：SKILL_EFFECT
//     时点直写对手槽位（FSM 序 First Skill Effect → First Attack Damage，当击生效；
//     miss 也已写但本回合不会再有己方攻击 → 无害），ws 每回合 reset 自清。
//     886 的 pending+常驻节点是"**下**次攻击"（跨回合承诺）才需要的，本条不需要。
//   - 实例：混元黑光斩 15469 [20]、入水突袭 15536 [50]（全库 22 载体，全部独立载体）。
// 567（arg_count=0）"对手为自身天敌时造成的伤害翻倍"：
//   - is_predator（common.h：对手固有属性克制自身）→ ws.damage_add_pct[自身] += 100
//     （776"翻倍"同通道：AMP 加法通道，applyDamageReduction/AMP 侧自然封顶）；
//     判定在 SKILL_EFFECT 时点（对手在场宠固有属性，1530/464 同判据）。
//   - 零参 → bind_participants 仍带 [owner, target]（2295 同款）。
//   - 实例：碎灭猛击 15590、陨石招来 16059（全库 22 载体）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1265/566/567 无行为硬编码（defense_ignore_pct
//   是套装线留下的通用槽，非专设）②effect_meta 无分类牵连 ③moves_lib 无既有注册——
//   真缺口。载体数核对：22/22/22 与扫描一致；全量切片脚本核验 66 载体布局零异常
//   （1265 [times,pct] / 566 [pct] / 567 空参）。
// 先例：897/1984（计数 REDUCE_PCT）、886（defense_ignore_pct 槽位语义，kongyuan_skills）、
//   776（damage_add_pct 翻倍）、428/1529（is_predator）、2295（零参绑定）。
// 场景回归：test/scenario/scenario_1050_dmg_mod.cpp（1050 号段，一文件三宏）。

#include "common.h"

#include <memory>

namespace {

// ── 1265 —— 下{0}次受到的攻击伤害减少{1}%（897 计数窗直系）──
EffectResult effect_1265_counted_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1265, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1265, args,
                     "布局=[owner,target,times,pct]");
    const int self = args.int_args[0];
    const int times = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && times > 0 && pct > 0, 1265, args,
                     "需要 times>0 && pct>0");
    auto counts = std::make_shared<int>(times);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, pct, counts](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || *counts <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            // 只管"自己受到的攻击红伤"（非属性直伤；final<=0 的空担不消费——897 口径）。
            if (d.defenderId != self || !d.isRed || d.isDirect || d.final <= 0) {
                return;
            }
            --*counts;
            d.final -= d.final * pct / 100;
        });
    return EffectResult::kOk;
}

// ── 566 —— 攻击时忽略对手{0}%的防御值 ──
EffectResult effect_566_ignore_def(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 566, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 566, args,
                     "布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 566, args, "owner 越界");
    EFFECT_ARG_GUARD(pct > 0, 566, args, "需要 pct>0");
    // 槽位索引是**防御方**（calculation.h 折减读法）→ 写对手槽位 = 己方本次攻击吃到。
    ctx->ws.defense_ignore_pct[1 - self] += pct;
    return EffectResult::kOk;
}

// ── 567 —— 对手为自身天敌时造成的伤害翻倍（零参）──
EffectResult effect_567_predator_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 567, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 567, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 567, args, "owner 越界");
    if (!is_predator(ctx, self)) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[self] += 100.0f;   // "翻倍"= 通用增伤 +100%（776 同通道）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1265, effect_1265_counted_reduce)
SKILL_EFFECT(566, effect_566_ignore_def)
SKILL_EFFECT(567, effect_567_predator_double)
