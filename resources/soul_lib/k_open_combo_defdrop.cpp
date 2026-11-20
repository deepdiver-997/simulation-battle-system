// K170 开场连击降防族 —— effect 263（1 宏 2 行，零参）
//
// 行清单（tools/soul_effects_dump.py 263，2026-10-07）：
//   263：2452 拉姆 / 2453 拉姆塞 "开场两回合内所有直接攻击都会使对手防御-1"
//       （两行同文同构、args 空、kind[12]）
// 实现：K2 265 开场 2 回合窗同款——early arm（OPERATION_ENTER_EXIT_STAGE）挂
// TEAM watcher（EVENT_SKILL_USED，plugin_storage 旗标防 early 双触发）；窗判据
// roundCount+1 > 2 退出（round N 时 roundCount==N-1）；「所有直接攻击」= 持有方
// 直接攻击（自方 SKILL_USED + used_attribute 属性技排除；官方主语省略，工单记档
// 取持有方攻击解读）；每击结算 = 每次直接攻击一次 stat_drop(对手, 防御槽2, 1)
// （附加禁令/免弱/钳 -6 原语内查）。
// ⚠️ 三查记录（2026-10-07）：①内核无 263 硬编码；②effect_meta overrides 0 行
// （零参行自洽）；③全库无同 id 注册（并集 registry 无 263）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990701000;   // +宏*10+owner：watcher 已挂旗标（防 early 双触发）
constexpr int kWindowRounds = 2;           // "开场两回合内"（两行同文，无参可读）
constexpr int kDefSlot = 2;                // 等级槽序 0攻 1特攻 2防 3特防 4速 5命中
constexpr int kDropAmount = 1;             // "防御-1"（stat_drop 正数=降幅）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// "直接攻击"事件判据：自家魂印在场 + 本方使用了攻击类技能（used_attribute=属性技排除）。
bool self_attack_skill_event(BattleContext* ctx, int owner, const BattleEvent& ev, int mark_id) {
    if (ev.actor != owner || ev.used_attribute) {
        return false;
    }
    return onstage_has_soul_mark(ctx, owner, mark_id);
}

// ── effect 263：开场两回合内所有直接攻击使对手防御-1（拉姆双生）──────────────

EffectResult soulmark_263_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, 263, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !self_attack_skill_event(wctx, owner, ev, 263)) {
                return;
            }
            if (wctx->roundCount + 1 > kWindowRounds) {
                return;   // "开场两回合内"：round N 时 roundCount==N-1（advanceRound 在
                          // ROUND_COMPLETION，battleFsm handle_BattleRoundStart 注释锚点）
            }
            if (g_core && g_core->stat_drop) {
                (void)g_core->stat_drop(wctx, wctx->opponent(owner), kDefSlot, kDropAmount,
                                        owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
// 263 = early arm（ENTER_EXIT_STAGE，挂 TEAM watcher，plugin_storage 旗标防 early 双触发）。
SOUL_PROGRAM(263, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_263_arm,
                    /*once=*/false, /*early=*/true)});
