// K259 每次用技概率速度+1族 —— effect 345（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 345，2026-10-21）：
//   345 Z 2708 args[10,7,7,7,7,8,7]（kind=[11]）：「每次使用技能有10%的概率提升速度等级1」
//     → **db[0] = 申报概率 10%**（tips 字面，db[0] 与文本一致）；db[1..6]=
//       [7,7,7,7,8,7] 语义存疑不消费、记档——全库**独一份**形态（7 位参数、
//       无兄弟行互锚），K2 纪律"解析不了宁可不用、不猜"。
//       ⚠️ 尾数里的 7/8 与能力槽号（0攻1特攻2防3特防4速5命中）不对应，**不猜**。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   触发源 = **EVENT_SKILL_USED**（技能**使用**即发，miss/被封也算——E13 锚点
//     语义）——官方"每次使用技能"锚在使用时点，不是命中时点。
//     门 = ev.actor == 印主（自己使用）。
//     ⚠️ **不限技能类型**（官方文本只说"使用技能"，没说"攻击技能"）→ 不做
//       攻击技过滤（与 K2 358 / K164 109/116/151 的"攻击技门"刻意不同）。
//   效果 = stat_change(印主, 速度槽 4, +1)（自身增益通道，**不查免弱**——
//     stat_drop 才查免弱；K255 320 / K196 390 同款自增益写法）。
//     ⚠️ 等级上限 ±6 由原语内钳。
//   概率 = **申报制**：roll_percent(db[0])（common.h 内的 inline 掷点器）——
//     掷点在插件侧一次，不重复掷（K191 双掷勘误口径）。
//   审计槽 = 本次掷点结果（1=触发/0=未触发）写 plugin_storage（F9 口径，K241 804
//     同款）→ 场景可对**触发与否**做无随机精确断言（配种子分岔）。
//   ⚠️ watcher 登记在 arm 节点（early 幂等旗，K164 / K178 351 同款）——
//     **跨回合 watcher 必须 WatcherScope::TEAM**（ON_STAGE 的 valid_id 活不过
//     回合交界，常识速查 §五）。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 345 无魂印行为硬编码；
//    ②effect_icon kind=[11]、tips 为权威文（dump 原文已核，effect_info 无 345
//    技能模板）；③registry soul 集合无 345、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_attack_prob 109/116/151（EVENT_SKILL_USED 窗 + roll_percent）、
//   k178 351（arm 幂等旗 + TEAM watcher）、K255 320（stat_change 自增益通道）、
//   k241 804（审计槽）。
// 场景回归：test/scenario/scenario_4150_roll_speed_345.cpp（号段 4150-4159）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 345;
constexpr int kSpeedSlot = kAbilityLevelIndexSpeed;   // 4（速度等级槽）
constexpr int kRollAuditBase = 991459000;              // +owner：本次掷点结果（1/0）

// arm：挂 EVENT_SKILL_USED watcher（自己用技 → db[0]% 概率速度+1）。
EffectResult k259_345_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // arm 幂等（early 节点开局双触发防重）：plugin_storage 旗。
    const int wired_key = 990259000 + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->stat_change) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己使用
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 概率取宿主行参 db[0]（tips 字面 10% 为回退）。
            int slot = c->on_stage[owner];
            if (slot < 0 || slot > 5) {
                slot = c->find_pet_with_soulmark(owner, kMarkId);
            }
            if (slot < 0 || slot > 5) {
                return;
            }
            const auto& db = c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
            const int chance = (db.size() > 0) ? db[0] : 10;
            if (chance <= 0) {
                // 0% 档：roll_percent(0) 本就返回 false，但**要记账**（审计槽 = 未触发）
                // ——否则场景无法区分"0% 档未掷"与"根本没触发"。
                c->plugin_storage[kRollAuditBase + owner] = 0;
                return;
            }
            const bool hit = roll_percent(c->rng(), chance);
            // 审计槽：本次掷点结果（1=触发 / 0=未触发）。
            c->plugin_storage[kRollAuditBase + owner] = hit ? 1 : 0;
            if (!hit) {
                return;
            }
            (void)g_core->stat_change(c, owner, kSpeedSlot, /*delta=*/1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（345 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k259_345_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
