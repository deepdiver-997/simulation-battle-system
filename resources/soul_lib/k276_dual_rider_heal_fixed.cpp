// K276 攻技回血/属技固伤双 rider 族 —— effect 606（1 宏 / 1 行）
//（尤西比奥 2841 args[1,120,2,120]；号段 4320-4329，worktree ../agents/k276，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 606，2026-10-21）：
//   「每次使用攻击技能恢复120点HP；每次使用属性技能附加120点固定伤害」
//   → db[0]=1、db[2]=2 废位不消费（K1 纪律）；db[1]=120 = 攻击技回血点数（字面自证）；
//   db[3]=120 = 属性技固伤点数（字面自证）——两值自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b606\b 无硬编码；②effect_info 606
//    技能空间模板不同文（"威力翻倍"，无关）；③soul_lib 全 worktree/registry 无 606
//    注册——真缺口。
// 实现（K164 109 同窗双 rider，按 used_attribute 分派）：
//   EVENT_SKILL_USED（own）：
//   · 攻击技（!used_attribute，miss 也算 E13）→ heal_amount(owner, db[1])——自回血，
//     走恢复原语（封回血覆盖时点则不涨，上限钳 max）；
//   · 属性技（used_attribute）→ fixed_damage(对手, db[3])——粉伤通道。
//   无概率槽 → 两 rider 均 100% 必发；无 boss 注记 → 不设 boss 门。
// 注册：SOUL_PROGRAM(606) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4320_dual_rider_heal_fixed.cpp（4320-4329）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 606;
constexpr int kWiredKeyBase = 990776000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=攻击技回血（缺位兜底 120）、db[3]=属性技固伤（兜底 120）。
void k276_read_row(BattleContext* ctx, int owner, int& heal, int& points) {
    heal = 120;
    points = 120;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1 && db[1] > 0) {
        heal = db[1];
    }
    if (db.size() > 3 && db[3] > 0) {
        points = db[3];
    }
}

// arm：EVENT_SKILL_USED（own）→ 攻击技回血 / 属性技附固伤。
EffectResult k276_606_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->fixed_damage) {
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
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->heal_amount || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor != owner) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int heal = 120;
            int points = 120;
            k276_read_row(c, owner, heal, points);
            if (heal <= 0 && points <= 0) {
                return;
            }
            if (!ev.used_attribute) {
                if (heal > 0) {
                    (void)g_core->heal_amount(c, owner, heal);
                }
            } else if (points > 0) {
                (void)g_core->fixed_damage(c, 1 - owner, points);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（606 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k276_606_arm,
                        /*once=*/false, /*early=*/true)});
