// K342 双防递增+提升段数附加族 —— effect 707（1 宏 / 1 行）
//（海拉瑞德 3007 args[1,50]；号段 4980-4989，worktree ../agents/k342，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 707，2026-10-21）：
//   「每回合双防加1;攻击技能附加自身防御和特防能力提升总段数乘以50的固定伤害」
//   → db[0]=1 = 每回合双防增量（"+1"自证）；db[1]=50 = 段数乘数（"乘以50"自证）——
//   双参自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b707\b 无魂印硬编码；②effect_info
//    707 技能空间模板不同文；③soul_lib 全 worktree/registry 无 707 注册——真缺口。
// 实现（双 rider：K301 ROUND_START 递增 + K164 rider）：
//   ① BATTLE_ROUND_START 桶：stat_change(防2,+db[0]) + (特防3,+db[0])（自增益通道；
//   ±6 钳由原语管）。
//   ② EVENT_SKILL_USED（own + 攻击技，miss 也算 E13）→ fixed_damage(对手,
//   (防lv+特防lv)×db[1])（等级直读 chaos_sorensen 146 同款；0 段不附加）。
// 注册：SOUL_PROGRAM(707) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4980_deframp_scale_rider.cpp（号段 4980-4989）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 707;

// ① ROUND_START：双防各 +db[0]。
EffectResult k342_707_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    int gain = 1;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            gain = db[0];
        }
    }
    (void)g_core->stat_change(ctx, owner, /*stat=*/2, gain);
    (void)g_core->stat_change(ctx, owner, /*stat=*/3, gain);
    return EffectResult::kOk;
}

// ② arm：攻击技 → (防lv+特防lv)×db[1] 固伤。
EffectResult k342_707_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = 990842000 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"攻击技能附加"字面）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int def_lv = c->ability_levels[owner][2];
            const int spdef_lv = c->ability_levels[owner][3];
            const int stages = def_lv + spdef_lv;
            if (stages <= 0) {
                return;   // 无提升段 → 不附加
            }
            int mult = 50;
            const int my_slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (my_slot >= 0 && my_slot <= 5) {
                const auto& db =
                    c->seerRobot[owner].elfPets[my_slot]
                        .soulMark.args.owned_int_args;
                if (db.size() > 1 && db[1] > 0) {
                    mult = db[1];
                }
            }
            (void)g_core->fixed_damage(c, 1 - owner, stages * mult);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（707 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k342_707_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k342_707_arm,
                        /*once=*/false, /*early=*/true)});
