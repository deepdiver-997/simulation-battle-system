// K337 受击窗增伤族 —— effect 695（1 宏 / 1 行）
//（斯洛斯 2990 args[1,2,100]；号段 4930-4939，worktree ../agents/k337，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 695，2026-10-21）：
//   「受到攻击则下2回合造成的伤害提高100%」
//   → db[1]=2 = 窗长回合（"下2回合"自证）；db[2]=100 = 增伤%（"提高100%"自证）；
//   db[0]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b695\b 无魂印硬编码；②effect_info
//    695 技能空间模板不同文；③soul_lib 全 worktree/registry 无 695 注册——真缺口。
// 实现（板行处方：K330 窗形换增益）：
//   ① EVENT_TAKE_DAMAGE watcher（own、两攻击伤害时点、actor 范围守卫）→ 挂窗账本
//   （plugin_storage = 受击轮 + db[1]，覆盖"下2回合"）。
//   ② BATTLE_FIRST/SECOND_SKILL_EFFECT 桶（view 已物化时点）：窗内且所选技 view>0
//   → skill_power_view ×(100+db[2])/100（636/2501 同款赋值形；幂等——乘区在桶内
//   单次执行）。boss 无效 → 节点旗 + watcher 手动门。
// 注册：SOUL_PROGRAM(695) 三节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4930_hitwindow_amp.cpp（号段 4930-4939）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 695;
constexpr int kWinKeyBase = 990837000;     // +owner：窗到期轮
constexpr int kWiredKeyBase = 990838000;   // +owner：watcher 已挂旗标

// 只认两攻击伤害时点（k_thorn 同款就地复刻）。
bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ② SKILL_EFFECT：窗内 → 威力视图 ×(100+db[2])/100。
EffectResult k337_695_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const auto win = ctx->plugin_storage.find(kWinKeyBase + owner);
    if (win == ctx->plugin_storage.end() ||
        any_int_or(win->second) < ctx->roundCount) {
        return EffectResult::kOk;   // 无窗/过期
    }
    int amp = 100;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 2 && db[2] > 0) {
            amp = db[2];
        }
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * (100 + amp) / 100;   // 636/2501 同款赋值形
    }
    return EffectResult::kOk;
}

// ① arm：受击挂窗。
EffectResult k337_695_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
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
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (ev.target != owner || ev.actor < 0 || ev.actor > 1 ||
                ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId) ||
                !is_attack_damage_event(ev)) {
                return;
            }
            int window_rounds = 2;
            const int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot >= 0 && slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                if (db.size() > 1 && db[1] > 0) {
                    window_rounds = db[1];
                }
            }
            c->plugin_storage[kWinKeyBase + owner] =
                c->roundCount + window_rounds;   // 受击轮 + db[1] = 到期轮
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（695 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k337_695_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k337_695_skill_effect),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k337_695_skill_effect)});
