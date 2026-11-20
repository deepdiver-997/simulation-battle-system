// K303 攻技烧伤+未烧伤反增伤族 —— effect 616（1 宏 / 1 行）
//（玛格丽特 2977 args[1,2,0,2]；号段 4590-4599，worktree ../agents/k303，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 616，2026-10-21）：
//   「每次使用攻击技能都会使对手烧伤,若对手未烧伤则自身攻击+2」
//   → db[1]=2 = 烧伤码（Burn=2 字面自证）；db[3]=2 = 攻击提升量（"+2"字面自证）；
//   db[0]/db[2] 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b616\b 无魂印硬编码；②effect_info
//    616 无模板行；③soul_lib 全 worktree/registry 无 616 注册——真缺口。
// 实现（板行处方：K2 358 概率烧伤同窗 + stat_change 自增益）：
//   EVENT_SKILL_USED（own + 攻击技，miss 也算 E13；属性技不触发）→ 码合法
//   （is_valid_abnormal_status_id 守卫——无效码跳过施加直落增伤分支，兼作场景
//   关闸档）→ apply_anomaly_checked 申报 pct=100（"都会"字面）→ 施加后对手仍无
//   烧伤（免疫/已异常/施加失败）→ stat_change(攻槽0, +db[3])（K211 315 同款
//   "攻击"=槽 0）。无 boss 注记。
// 注册：SOUL_PROGRAM(616) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4590_attack_burn_or_amp.cpp（号段 4590-4599）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 616;
constexpr int kWiredKeyBase = 990803000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=烧伤码（兜底 Burn）、db[3]=攻击提升量（兜底 2）。
void k303_read_row(BattleContext* ctx, int owner, int& burn_code, int& atk_boost) {
    burn_code = static_cast<int>(AbnormalStatusId::Burn);
    atk_boost = 2;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1 && db[1] > 0) {
        burn_code = db[1];
    }
    if (db.size() > 3 && db[3] > 0) {
        atk_boost = db[3];
    }
}

// arm：EVENT_SKILL_USED（own+攻击技）→ 必烧伤；未烧伤 → 自攻 +db[3]。
EffectResult k303_616_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked || !g_core->stat_change) {
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
            if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->stat_change ||
                !g_core->has_active_anomaly) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技才触发（"使用攻击技能"字面，属性技不算）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            int burn_code = static_cast<int>(AbnormalStatusId::Burn);
            int atk_boost = 2;
            k303_read_row(c, owner, burn_code, atk_boost);
            const int opp = 1 - owner;
            bool burned = false;
            if (burn_code > 0 && is_valid_abnormal_status_id(burn_code)) {
                (void)g_core->apply_anomaly_checked(c, opp, burn_code,
                                                    random_anomaly_duration(), owner,
                                                    /*chance_pct=*/100,
                                                    ChanceSource::SoulMark);
                burned = g_core->has_active_anomaly(c, opp, burn_code);
            }
            if (!burned && atk_boost > 0) {
                (void)g_core->stat_change(c, owner, /*stat=*/0, atk_boost);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（616 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k303_616_arm,
                        /*once=*/false, /*early=*/true)});
