// J 组 J238 · 低伤降级族 —— 1 宏/7 载体 8 实例（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   501（args_num=3）"若造成的伤害不足{0}，则对手{1}等级-{2}"
//
// 实现口径（J204 107 低伤自身升级族直系镜像：DETECT 攻方桶 + 一次性 armed）：
//   - SKILL_EFFECT 时点注册 DETECT 攻方桶回调（541/1104/107 同通道），读本次
//     攻击 final；只认自身→对手红伤，且只在**施放当回合的首刀**有效
//     （armed/round 双闸，miss/打盔不残留跨回合误触发）。
//   - final < {0}（"不足" = 严格小于）→ stat_drop(对手, 槽, {2}, actor=owner)
//     （弱化真原语：查免弱 STAT_DROP、不穿强化保护、钳 -6、本体/视图同步）。
//   - {1} 是**官方码序**（0攻 1防 2特攻 3特防 4速 5命中——速查 §三 双序坑），
//     不是引擎槽语义序：映射 {0,2,1,3,4,5}（473/593/585 kStatMap 同款）。
//     证据：16957 [300,1,1]+[300,3,1]（防+特防各降 1）、37442 [240,3,2]（特防降 2）。
//   - 审计槽 plugin_storage[990501100+owner] = 本刀实际伤害（场景精确断门）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算，7 技/8 实例）：
//   501 [伤害阈值,官方槽码,降级]：14530 月牙无双刃 [300,1,1]（纯链）、
//     15504 灵魂穿刺 [33,501]|[300,1,1]、16957 秘灵炎破斩 [501,501]|
//     [300,1,1,300,3,1]（双实例）、17563 时空收束弹 [300,1,2]、
//     34780 冬日巡礼 [1036,501]|[1,300,250,1,2]、37442 艳阳步 [240,3,2]、
//     37916 千羽雷葬 [1468,501]|[300,1,300,1,2]（8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 501 无硬编码 ②custom_effect_overrides/
// programs 无行；effect_info 同文仅 501 一条（473/503 是自身版/固伤版，已实现）
// ③全库 soul_lib/moves_lib 并集（含 kbatch 等 worktree）无 SKILL_EFFECT(501)
// ——真缺口。
// 场景回归：test/scenario/scenario_5690_low_damage_stat_drop.cpp（号段 5690-5699）。

#include "common.h"

#include <primitives/battle_primitives.h>

#include <memory>

namespace {

constexpr int k501AuditBase = 990501100;   // +owner：本刀实际伤害

// 官方码序（0攻/1防/2特攻/3特防/4速/5命中）→ 引擎能力槽
// （0攻/1特攻/2防/3特防/4速/5命中）。防与特攻互换是本映射的全部意义。
constexpr int k501OfficialCodeToSlot[6] = {0, 2, 1, 3, 4, 5};

// ── 501 —— 若造成的伤害不足{0}，则对手{1}等级-{2} ──
// int_args = [owner, target, 伤害阈值, 官方槽码, 降级数]（args_num=3）。
EffectResult effect_501_low_damage_stat_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 501, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 501, args,
                     "501 布局=[owner,target,伤害阈值,官方槽码,降级数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int code = args.int_args[3];
    const int amount = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     501, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold >= 0, 501, args, "需要 伤害阈值 ≥ 0");
    EFFECT_ARG_GUARD(code >= 0 && code < 6, 501, args,
                     "官方槽码 " + std::to_string(code) + " 越界（0..5）");
    EFFECT_ARG_GUARD(amount > 0, 501, args, "需要 降级数 > 0");

    const int slot = k501OfficialCodeToSlot[code];
    auto armed = std::make_shared<bool>(true);
    auto registered_round = std::make_shared<int>(ctx->roundCount);
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, opp, threshold, slot, amount, armed, registered_round](
            BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || !*armed) {
                return;
            }
            if (c->roundCount != *registered_round) {
                return;   // 首刀未结算 → 不留跨回合误触发
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId != opp || !d.isRed) {
                return;
            }
            *armed = false;
            c->plugin_storage[k501AuditBase + self] = d.final;
            if (d.final < threshold && g_core && g_core->stat_drop) {
                (void)g_core->stat_drop(c, opp, slot, amount, /*actor=*/self);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(501, effect_501_low_damage_stat_drop)
