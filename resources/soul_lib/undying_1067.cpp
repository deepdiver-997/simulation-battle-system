// 免死 · 锁血族（不灭·艾恩斯 1067；真 2 命复活后续同族）
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；代码零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

constexpr int kUndyingMarkId = 1067;
struct UndyingRuntime {
    bool watcher_armed = false;    // 受击监控器只 arm 一次（切走/重登 TEAM 不丢）
    bool consumed = false;         // ①免死是否已消耗（全场仅一次）
    bool high_hp_pending = false;  // ②本回合"高体待焚烬"是否仍待触发
};

// 取/建 1067 运行时槽。
UndyingRuntime& undying_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kUndyingMarkId)) {
        storage[kUndyingMarkId] = UndyingRuntime{};
    }
    return std::any_cast<UndyingRuntime&>(storage[kUndyingMarkId]);
}

// ② 回合开始：体力>对手 → 置"本回合受击焚烬"待触发。
EffectResult soulmark_1067_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& opp = ctx->getPet(ctx->opponent(owner));
    UndyingRuntime& rt = undying_runtime(ctx, owner);
    rt.high_hp_pending = self.hp > 0 && opp.hp > 0 && self.hp > opp.hp;
    return EffectResult::kOk;
}

// ①+② 共用的受击监控器（arm 一次, TEAM 永久）：
//   受致命伤(①) → 锁1血 + 清双方提升/回合 + 令对手烧伤2回合（每场1次）;
//   高体待焚烬(②) → 受击使对手焚烬（触发即清标）。
EffectResult soulmark_1067_arm_undying(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    UndyingRuntime& rt = undying_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (ev.target != owner || ev.actor == owner) {
                return;  // 仅"对手出手回合"受击（被打, 不是自伤/非艾恩斯）
            }
            ElfPet& self = wctx->getPet(owner);
            UndyingRuntime& und = undying_runtime(wctx, owner);
            // ① 致命伤（hp 已归 0）→ 不死锁血（全场仅一次）。
            if (self.hp == 0 && !und.consumed) {
                und.consumed = true;   // 先耗, 幂等
                self.hp = 1;           // 残留 1 点体力
                const int opp = wctx->opponent(owner);
                g_core->clear_stat_boosts(wctx, owner);
                g_core->clear_stat_boosts(wctx, opp);
                g_core->break_round_effects(wctx, owner);
                g_core->break_round_effects(wctx, opp);
                g_core->apply_anomaly(wctx, opp, static_cast<int>(AbnormalStatusId::Burn),
                                      /*rounds=*/2, owner);
                return;  // 致命伤优先, 本次受击不再走②焚烬
            }
            // ② 高体待焚烬 → 当回合受击使对手焚烬（触发即清, 不再断回合）。
            if (und.high_hp_pending) {
                und.high_hp_pending = false;
                g_core->apply_anomaly(wctx, wctx->opponent(owner),
                                      static_cast<int>(AbnormalStatusId::Incinerate),
                                      /*rounds=*/2, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 回合结束：
//   ② 本回合"高体待焚烬"还没触发 → 消除对手回合类效果（清标）;
//   ③ 若自身体力<对手 → 恢复自身已损失体力的1/2。
EffectResult soulmark_1067_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& opp = ctx->getPet(ctx->opponent(owner));
    UndyingRuntime& rt = undying_runtime(ctx, owner);
    if (rt.high_hp_pending) {  // ② 未焚烬 → 断对手回合类
        rt.high_hp_pending = false;
        g_core->break_round_effects(ctx, ctx->opponent(owner));
    }
    if (self.hp > 0 && opp.hp > 0 && self.hp < opp.hp) {  // ③ 低体 → 回已损1/2
        const int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
        const int lost = max_hp > 0 ? max_hp - self.hp : 0;
        if (lost > 0) {
            g_core->heal_amount(ctx, owner, lost / 2);
        }
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 不灭·艾恩斯 1067：三子句——①不死锁血(受致命伤留1血+清双方+烧伤,每场1次)
    //   ②回合开始高体→受击焚烬/未焚断回合 ③回合结束低体→回已损1/2。
    SOUL_PROGRAM(kUndyingMarkId, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1067_arm_undying,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1067_round_start,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER),
         SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1067_round_end,
                         /*once=*/false, /*early=*/false, SoulScope::ROSTER)});

}  // namespace
