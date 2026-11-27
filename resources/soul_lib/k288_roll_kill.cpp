// K288 概率秒杀族 —— effect 627（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 627，2026-10-21）：
//   627 剑魂·特兰西 2878 args[4,0]（kind=[1]）：「4%秒杀对手」
//     → **db[0] = 概率 4%**（**百分比**口径 —— 与 1147「5%秒杀对手」args[5,...]
//       的 db[0]=5 同款；⚠️ **不同于** K1 151「攻击时有2%秒杀」args[20,0] 的
//       **千分比**口径（db/10）——K4 头注："概率单位逐宏互锚，严禁跨宏复用换算"，
//       本单按 tips 字面 4% 落地，不做 /10 折算）。db[1]=0 语义存疑不消费、记档。
//     ⚠️ 口径记档：tips 文本**无动作锚点**（未写"攻击时"/"受击时"）。同族对照：
//       151「攻击时有2%秒杀」/ 350「使用攻击技能直接秒杀」都明写动作锚，627 没写
//       → 本单按**自己使用攻击技能时**判定（同族默认锚点），此为本单最大歧义点。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   触发源 = **EVENT_SKILL_USED** watcher（K1 151 / K2 358 同款：own + 攻击技门；
//     miss 也算"使用"，E13 锚点语义）。
//   门 = ev.actor == 印主 且 ev.used_attribute == false（攻击技）。
//   效果 = roll_percent(db[0]) 掷点 → 命中则 force_hp_to_zero(对手, 印主)
//     （秒杀原语；**存活门**：对手 hp<=0 不重复秒——K1 151 同款，J23 36 同款）。
//   ⚠️ 审计槽 = 本次申报概率%（F9 口径，K241 804 / K259 345 同款）→ 场景可对
//     概率做**无随机精确断言**。
//
// ⚠️ 三查记录（author a12-kprod 2026-10-21）：①内核 grep 627 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核，effect_info 无 627
//    技能模板）；③registry soul 集合无 627、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_attack_prob 151（EVENT_SKILL_USED 攻击技窗 + 概率秒杀 + 存活门，旗舰）、
//   k_anomaly_chance 358（arm 幂等旗 + TEAM watcher）、K259 345（审计槽）。
// 场景回归：test/scenario/scenario_4440_roll_kill_627.cpp（号段 4440-4449）。

#include "common.h"

namespace {

constexpr int kMarkId = 627;
constexpr int kChanceFallback = 4;    // tips 字面 4%（db[0] 读不到时回退）
constexpr int kChanceAuditBase = 991288000;   // +owner：本次申报概率%（审计槽，只读）
constexpr int kWiredKeyBase = 990288000;       // +owner：watcher 已挂旗标

// 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// arm：挂 EVENT_SKILL_USED watcher（自己用攻击技 → db[0]% 概率秒杀对手）。
EffectResult k288_627_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    // 行参在 arm 时读一次（宿主行参不随回合变）→ 捕获进闭包（K259 345 同款）。
    const HostRow row = host_row(ctx, a, kMarkId);
    const int chance = row.valid ? db_at(row, 0, kChanceFallback) : kChanceFallback;
    if (chance <= 0) {
        return EffectResult::kOk;
    }

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, chance](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->force_hp_to_zero) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己使用
            }
            if (ev.used_attribute) {
                return;   // 攻击技门（属性技不触发）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            // 审计槽：本次申报概率%（F9 口径）。
            c->plugin_storage[kChanceAuditBase + owner] = chance;
            if (!roll_percent(c->rng(), chance)) {
                return;
            }
            const int opp = c->opponent(owner);
            if (c->getPet(opp).hp <= 0) {
                return;   // 存活门（K1 151 同款）
            }
            (void)g_core->force_hp_to_zero(c, opp, owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（627 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k288_627_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
