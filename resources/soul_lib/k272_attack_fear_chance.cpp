// K272 攻技概率害怕族 —— effect 395（1 宏 / 1 行）
//（天石灵尊 2821 args[1,1,10,6] kind[?]；号段 4280-4289，worktree ../agents/k272，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 395，2026-10-21）：
//   「每次使用攻击技能有10%概率使对手害怕（boss无效）」
//   → db[0]/db[1]=1,1 废位不消费（K1 纪律，K2 358 [1,1,…] 行首同款）；db[2]=10 =
//   概率%（字面自证）；db[3]=6 = Fear 官方码（字面自证）——后两参自洽均消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b395\b 无硬编码；②effect_info 395
//    无模板行；③soul_lib 全 worktree/registry 无 395 注册——真缺口。
// 实现（板行处方：K269 186 同窗直系——EVENT_SKILL_USED own+攻击技，miss 也算"攻击"
//   E13 使用即触发；属性技不触发）→ apply_anomaly_checked 申报制（概率门原语内单掷，
//   Modern 全链，K2 口径；pct<=0 本地短路 = 场景关闸档）。
//   boss 无效子句 → watcher 手动门 is_boss_challenge（diwang 半边同款——节点旗管不住
//   事件面回调）。
// 注册：SOUL_PROGRAM(395) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4280_attack_fear_chance.cpp（号段 4280-4289）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 395;
constexpr int kWiredKeyBase = 990774000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[2]=概率%（兜底 10）、db[3]=异常码（兜底 Fear；非法码回退 Fear）。
void k272_read_row(BattleContext* ctx, int owner, int& pct, int& anomaly) {
    pct = 10;
    anomaly = static_cast<int>(AbnormalStatusId::Fear);
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 2) {
        pct = db[2];   // 在位即消费（0 = 显式关闸，watcher pct<=0 短路）
    }
    if (db.size() > 3 && db[3] > 0) {
        anomaly = db[3];
    }
}

// arm：EVENT_SKILL_USED（own+攻击技）→ db[2]% 令对手进 db[3] 异常（申报制单掷）。
EffectResult k272_395_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
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
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"攻击技能"字面，属性技不算）
            }
            if (c->is_boss_challenge) {
                return;   // boss 无效子句（watcher 手动门）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int pct = 10;
            int anomaly = static_cast<int>(AbnormalStatusId::Fear);
            k272_read_row(c, owner, pct, anomaly);
            if (pct <= 0) {
                return;   // 0% 关闸（不耗 rand，场景对照档）
            }
            (void)g_core->apply_anomaly_checked(c, 1 - owner, anomaly,
                                                random_anomaly_duration(), owner, pct,
                                                ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（395 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k272_395_arm,
                        /*once=*/false, /*early=*/true)});
