// K298 低体瞬杀攻击门族 —— effect 350（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 350，2026-10-21）：
//   350 幻世帝天 2723 args[2,200,5]（kind=[1]）：「对手体力少于200时，使用攻击技能
//     直接秒杀对手」
//     → **db[1]=200 = 绝对门限**（tips 字面"体力少于200"自证）；
//       db[0]=2 / db[2]=5 语义存疑不消费（记档）。
//     ⚠️ db[0]/db[2] 不可解的依据（不猜）：全库"体力少于N时秒杀"仅本行一条
//       （DB 查证），无同族行可交叉锚定；tips 的三个语义子句（门限 200 / 动作
//       锚点"使用攻击技能"）只对应两个数字，第三个数字无处安放 → 按 K2 纪律
//       「解析不了宁可不用、不猜」：**只消费 db[1]**，其余记档。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   触发源 = **EVENT_SKILL_USED** watcher（K1 151 / K2 358 同款：own + 攻击技门；
//     miss 也算"使用"，E13 锚点语义）。
//   门 = ①ev.actor == owner（自己使用）②ev.used_attribute == false（**攻击技**——
//     tips 明写"使用攻击技能"）③对手 hp < db[1]（**严格小于**，tips 字面"少于200"；
//     恰好 200 不触发）④对手 hp > 0（**存活门**，J23 36 / K1 151 同款：不秒已死的）。
//   效果 = force_hp_to_zero(对手, 印主)（秒杀原语；J23 36 / K1 151 同款）。
//   ⚠️ **无概率**（与 K1 151「2%概率秒杀」不同）：本单是**确定性**门——满足即秒杀，
//     故不掷点、不写审计槽（审计槽是概率类专用）。
//   ⚠️ 秒杀通道**不参与护盾/护罩/减伤**（force_hp_to_zero = 直接归零，battleContext
//     头注"它不是伤害"），故"对手体力少于200"读的是**当前体力**（未被护盾抵消过的
//     真实 hp），不随本次攻击的护盾吸收而变。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 350 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核；effect_info 无 350
//    技能模板）；③registry soul 集合无 350、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_attack_prob 151（**EVENT_SKILL_USED 攻击技窗 + force_hp_to_zero + 存活门，
//   旗舰直系**——本单是它的确定性版）、k_anomaly_chance 358（arm 幂等旗 + TEAM
//   watcher）、J23 36（force_hp_to_zero + 存活门）。
// 场景回归：test/scenario/scenario_4540_lowhp_kill_350.cpp（号段 4540-4549）。

#include "common.h"

namespace {

constexpr int kMarkId = 350;
constexpr int kThresholdFallback = 200;   // tips 字面"体力少于200"（db[1]）
constexpr int kWiredKeyBase = 990298000;  // +owner：watcher 已挂旗标

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）────────
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

// arm：挂 EVENT_SKILL_USED watcher（自己用攻击技 + 对手低体力 → 秒杀对手）。
EffectResult k298_350_arm(BattleContext* ctx, const SoulNodeArgs& a) {
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
    const int threshold = row.valid ? db_at(row, 1, kThresholdFallback) : kThresholdFallback;
    if (threshold <= 0) {
        return EffectResult::kOk;
    }

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, threshold](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->force_hp_to_zero) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己使用
            }
            if (ev.used_attribute) {
                return;   // 攻击技门（tips 明写"使用攻击技能"）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = c->opponent(owner);
            const int hp = c->getPet(opp).hp;
            if (hp <= 0) {
                return;   // 存活门（J23 36 / K1 151 同款）：不秒已死的
            }
            if (hp >= threshold) {
                return;   // 门闭：体力不低于门限（严格小于才触发）
            }
            (void)g_core->force_hp_to_zero(c, opp, owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（350 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k298_350_arm,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
