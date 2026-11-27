// K317 先手攻击吸血+睡眠族 —— effect 339（1 宏 / 1 行）
//（圣瞳·天蛇太祖 2695 args[1,20,15,8]；号段 4730-4739，worktree ../agents/k317，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 339，2026-10-21）：
//   「若每回合先出手攻击，则造成伤害的20%恢复自身体力并附带15%概率令对手睡眠」
//   → db[1]=20 = 吸血%（字面自证）；db[2]=15 = 睡眠概率%（字面自证）；db[3]=8 =
//   睡眠码（Sleep=8 官方码自证）；db[0]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b339\b 唯一命中为 idx=339 官方
//    文档散文（battle_primitives.h 注释），非硬编码；②effect_info 339 技能空间模板
//    不同文；③soul_lib 全 worktree/registry 无 339 注册——真缺口。
// 实现（板行处方：TAKE_DAMAGE 读快照 K3 口径 + 先手门）：
//   EVENT_TAKE_DAMAGE watcher（target=对手 && actor=own——自己打中对手的落地伤害；
//   读 ev.amount 快照不受 AFTER_ACTION 时点影响）→ 先手门（当回合 preemptive_right
//   == 己方）→ heal_amount(owner, ev.amount×db[1]/100) + roll_percent(db[2]) →
//   apply_anomaly_checked(对手, db[3], duration, owner, 100, SoulMark)。
//   吸取走 heal 原语（封回血覆盖则不涨）。
// 注册：SOUL_PROGRAM(339) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4730_first_lifesteal_sleep.cpp（号段 4730-4739）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 339;
constexpr int kWiredKeyBase = 990817000;   // +owner：watcher 已挂旗标（防再登场双挂）

// 行参：db[1]=吸血%（兜底 20）、db[2]=睡眠概率%（兜底 15；0=显式关闸）、db[3]=睡眠码
//（兜底 Sleep）。
void k317_read_row(BattleContext* ctx, int owner, int& lifesteal, int& sleep_pct,
                   int& sleep_code) {
    lifesteal = 20;
    sleep_pct = 15;
    sleep_code = static_cast<int>(AbnormalStatusId::Sleep);
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1 && db[1] > 0) {
        lifesteal = db[1];
    }
    if (db.size() > 2) {
        sleep_pct = db[2];
    }
    if (db.size() > 3 && db[3] > 0) {
        sleep_code = db[3];
    }
}

// arm：TAKE_DAMAGE（自己打中对手）→ 先手门 → 吸血 + 概率睡眠。
EffectResult k317_339_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->apply_anomaly_checked) {
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
            if (!c || !g_core || !g_core->heal_amount ||
                !g_core->apply_anomaly_checked) {
                return;
            }
            if (!(ev.target == 1 - owner && ev.actor == owner)) {
                return;   // 只认"自己打中对手"的落地伤害
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 先手门：当回合先手权（TAKE_DAMAGE 时当回合 pr 仍在）。
            const bool i_am_first =
                (owner == 0 && c->preemptive_right == PreemptiveRight::SEER_ROBOT_1) ||
                (owner == 1 && c->preemptive_right == PreemptiveRight::SEER_ROBOT_2);
            if (!i_am_first) {
                return;
            }
            int lifesteal = 20;
            int sleep_pct = 15;
            int sleep_code = static_cast<int>(AbnormalStatusId::Sleep);
            k317_read_row(c, owner, lifesteal, sleep_pct, sleep_code);
            const int amount = ev.amount > 0 ? ev.amount : 0;
            if (lifesteal > 0 && amount > 0) {
                (void)g_core->heal_amount(c, owner, amount * lifesteal / 100);
            }
            if (sleep_pct > 0 && roll_soul_percent(*c, kMarkId, 0, owner, ev.target, sleep_pct) &&
                is_valid_abnormal_status_id(sleep_code)) {
                (void)g_core->apply_anomaly_checked(c, 1 - owner, sleep_code,
                                                    random_anomaly_duration(), owner,
                                                    /*chance_pct=*/100,
                                                    ChanceSource::SoulMark);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（339 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k317_339_arm,
                        /*once=*/false, /*early=*/true)});
