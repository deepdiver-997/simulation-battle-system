// K273 攻技概率固伤族 —— effect 603（1 宏 / 1 行）
//（邪念魔尊 2837 args[1,50,200]；号段 4290-4299，worktree ../agents/k273，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 603，2026-10-21）：
//   「每次使用攻击技能有50%概率附加200点固定伤害」
//   → db[0]=1 废位不消费（K1 纪律）；db[1]=50 = 概率%（字面自证）；db[2]=200 =
//   固伤点数（字面自证）——后两参自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b603\b 无硬编码；②effect_info 603
//    技能空间模板不同文（"概率陷入状态"，无关）；③soul_lib 全 worktree/registry 无
//    603 注册——真缺口。
// 实现（板行处方：K164 109 直系同窗——EVENT_SKILL_USED own+攻击技，miss 也算"攻击"
//   E13 使用即触发；属性技不触发）→ roll_percent(db[1]) 本地单掷 → fixed_damage
//   (对手, db[2])（粉伤通道：吃护罩/免粉/减伤；fixed_damage 无概率槽 → 本地 roll，
//   K164 109 同款纪律）。无 boss 注记 → 不设 boss 门。
// 注册：SOUL_PROGRAM(603) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4290_attack_fixed_chance.cpp（号段 4290-4299）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 603;
constexpr int kWiredKeyBase = 990775000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=概率%（在位即消费，0=显式关闸；缺位兜底 50）、db[2]=固伤点数（兜底 200）。
void k273_read_row(BattleContext* ctx, int owner, int& pct, int& points) {
    pct = 50;
    points = 200;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1) {
        pct = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        points = db[2];
    }
}

// arm：EVENT_SKILL_USED（own+攻击技）→ db[1]% → 对手吃 db[2] 点固伤。
EffectResult k273_603_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->fixed_damage) {
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
            if (!c || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"攻击技能"字面，属性技不算）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int pct = 50;
            int points = 200;
            k273_read_row(c, owner, pct, points);
            if (pct <= 0 || !roll_soul_percent(*c, kMarkId, 0, owner, 1 - owner, pct)) {
                return;   // 0% 关闸 / 概率未中（本地单掷，K164 109 同款）
            }
            (void)g_core->fixed_damage(c, 1 - owner, points);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（603 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k273_603_arm,
                        /*once=*/false, /*early=*/true)});
