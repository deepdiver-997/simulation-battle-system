// J 组 J197 · 低伤→下N次点减伤族 —— 1 宏/10 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   1202 [3] 造成的伤害低于{0}则自身下{1}次受到的攻击伤害降低{2}点
//
// 实现口径：
//   - 双通道，全部走既有伤害管线阶段，不新建乘区/机制：
//     ① **伤害检测**：register_damage_effect(DETECT, self, DETECT) 读本次攻击的
//        resolvedDamage.final；只认"自身→对手的红伤"，且只在**施放当回合的一刀**有效
//        （shared_ptr round/armed 双闸：首刀未命中/被打盔时残留条目也不会跨回合误触发）。
//        final < {0}（严格小于）→ 次数计数器 plugin_storage[k1202ChargeBase+self]={1}。
//     ② **点数减伤**：register_damage_effect(REDUCE_FLAT, self, MITIGATE) 读受击 final，
//        有剩余次数 → final 扣 {2} 点（下限 0）并消耗一层。MITIGATE 类 → 不可被"挡伤失效"
//        无视（damage_pipeline.h 口径；1153 直系）。
//   - 次数容器 = plugin_storage int（跨回合有效、消耗即减）；条目随换宠由
//     pipeline_valid_id 惰性失效（人格化减伤下场即失，存量为既有口径）。
//   - 审计槽 plugin_storage[k1202AuditBase+self] = 本刀实际伤害（场景可断言"门真的走到"，
//     无随机）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1202 [阈值,次数,点数]：33740 冰脉护手 纯链 [300,1,300]（物 140，场景载体）、
//        30904 蝶影振云击 链头 [300,1,300]、35567 暗冥翅刃 [300,1,300]、
//        33721 叶落悲鸣 [250,1,250]…（10/10 自洽；载体内 {1} 多为 1）
//
// 动工前置三查（2026-10-22）：①内核 grep 1202 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(1202)——真缺口。
// 场景回归：test/scenario/scenario_5200_low_damage_flat_reduce.cpp（号段 5200-5209）。

#include "common.h"

#include <primitives/battle_primitives.h>

#include <memory>

namespace {

constexpr int k1202ChargeBase = 990120200;   // +self：剩余减伤次数
constexpr int k1202AuditBase = 990120201;    // +self：本刀实际伤害（审计槽）

// ── 1202 —— 造成的伤害低于{0}则自身下{1}次受到的攻击伤害降低{2}点 ──
// int_args = [owner, target, 阈值, 次数, 点数]（args_num=3）。
EffectResult effect_1202_low_damage_flat_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1202, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1202, args,
                     "1202 布局=[owner,target,阈值,次数,点数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int times = args.int_args[3];
    const int points = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1202, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && times > 0 && points > 0, 1202, args,
                     "需要 阈值/次数/点数 > 0");

    // ① 本次施放的一刀伤害检测（DETECT 相位：此时 final 已过增伤/减伤，读到的就是落地值）。
    //    armed/registeredRound 双闸保证只认"施放当回合的首个自身→对手红伤"。
    auto armed = std::make_shared<bool>(true);
    auto registered_round = std::make_shared<int>(ctx->roundCount);
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, opp, threshold, times, armed, registered_round](BattleContext* c,
                                                               int bucket_owner) {
            if (!c || bucket_owner != self || !*armed) {
                return;
            }
            if (c->roundCount != *registered_round) {
                return;   // 首刀未结算（miss/打盔）→ 不留跨回合误触发
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId != opp || !d.isRed) {
                return;
            }
            *armed = false;
            c->plugin_storage[k1202AuditBase + self] = d.final;
            if (d.final < threshold) {
                // "造成的伤害低于{0}"（严格小于）→ 授 {1} 次点数减伤。
                c->plugin_storage[k1202ChargeBase + self] = times;
            }
        });

    // ② 受击点数减伤（REDUCE_FLAT / MITIGATE：点数削减，不可被"挡伤失效"无视）。
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, points](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            auto it = c->plugin_storage.find(k1202ChargeBase + self);
            if (it == c->plugin_storage.end()) {
                return;
            }
            const int left = any_int_or(it->second);
            if (left <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只对真正落地的受击扣减/消费（打盔不消费，811 同口径）
            }
            c->plugin_storage[k1202ChargeBase + self] = left - 1;
            d.final = std::max(0, d.final - points);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1202, effect_1202_low_damage_flat_reduce)
