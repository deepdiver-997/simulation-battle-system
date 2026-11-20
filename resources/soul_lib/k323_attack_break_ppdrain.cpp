// K323 攻消回合+全PP压制族 —— effect 614（1 宏 / 1 行）
//（苏希斯 2862 args[1,1,1,1]；号段 4790-4799，worktree ../agents/k323，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 614，2026-10-21）：
//   「每次使用攻击技能消除对手回合类效果,每次使用攻击技能降低对手所有PP1点（boss无效）」
//   → args 全 1：消除/压制均为定值语义（db 全废——数值按 tips 定值 1）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b614\b 无魂印硬编码；②effect_info
//    614 技能空间模板不同文；③soul_lib 全 worktree/registry 无 614 注册——真缺口。
// 实现（板行处方：K310 830 直系——双锚点改全槽压制）：
//   ① EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ break_round_effects(对手)
//   （使用即消，无 SUCCESS 门槛）。② EVENT_TAKE_DAMAGE（target=对手 && actor=own
//   —— "命中后"落地锚）→ 对手全 5 技能 pp-1 钳 0（K321 全槽直改手法）。
//   boss 无效 → 双 watcher 手动门。
// 注册：SOUL_PROGRAM(614) 单节点（arm 双 watcher），总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4790_attack_break_ppdrain.cpp（号段 4790-4799）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 614;
constexpr int kWiredKeyBase = 990823000;   // +owner：watcher 已挂旗标（防再登场双挂）

// arm：双 watcher 接线。
EffectResult k323_614_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    // ① 攻击 → 消除对手回合类效果。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->break_round_effects) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            (void)g_core->break_round_effects(c, 1 - owner);
        },
        WatcherScope::TEAM);
    // ② 命中后 → 对手全技能 pp-1（钳 0）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (!(ev.target == 1 - owner && ev.actor == owner)) {
                return;   // 只认"自己打中对手"的落地伤害
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            ElfPet& foe = c->getPet(1 - owner);
            for (int i = 0; i < 5; ++i) {
                if (foe.skills[i].pp > 0) {
                    foe.skills[i].pp -= 1;   // 1 点定值（tips），恒 ≥0
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（614 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k323_614_arm,
                        /*once=*/false, /*early=*/true)});
