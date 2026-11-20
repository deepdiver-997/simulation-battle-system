#include <utils/sbs_rand.h>
// K2536 库拉塔克·吸取能力族 —— effect 2152（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 2152，2026-10-21）：
//   2152 库拉塔克 4547 args[4,1,2,1,300,1]（kind=[12,1,9]）：
//     「自身攻击技能命中时吸取对手4项能力1级，自身属性技能命中时吸取对手第五技能2点
//       PP值；|战斗阶段结束时，若本回合自身未受到攻击伤害则下回合所有技能先制+1，
//       若自身受到了攻击伤害则下回合使用技能后附加300点固定伤害（boss无效）」
//     → **db[0] = 吸取项数 4**（tips 字面 "4项" ↔ 4）；**db[1] = 每项级数 1**
//       （tips 字面 "1级" ↔ 1）；**db[2] = 吸取 PP 点数 2**（tips 字面 "2点PP值" ↔ 2）；
//       db[3] = 1（tips 无对应子句，不消费——记档）；**db[4] = 固伤 300**（tips 字面
//       "300点固定伤害" ↔ 300）；db[5] = 1（不消费，记档）。
//     → 四个参数（4/1/2/300）与文本**完全自洽** → 均消费。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参，方案 B）：
//   子句①「自身**攻击技能命中**时吸取对手4项能力1级」
//     - 时点 = BATTLE_FIRST/SECOND_ON_SKILL_HIT（命中时点：ws.skill_exec_result
//       已物化 → 可判 HIT；K231 1396 同款读法）。
//     - 吸取 = **随机挑 db[0] 个互不重复的能力槽** → 对手该槽 **stat_drop(对手,
//       槽, db[1])**（弱化原语：查附加禁令 ATTACH_BAN + **免弱 STAT_DROP** +
//       钳 -6）→ **只有 stat_drop 返回 SUCCESS 才 stat_change(自己, 槽, +db[1])**
//       （自身增益通道，不查免弱；K255 320 / K282 645 同款）。
//       ⚠️ **用户口径（2026-10-21 拍板）**：**面对免弱对手时不会给自己附加提升**
//         ——对手免弱 → stat_drop 返回 IMMUNE → 不给自己加。本实现严格照此：
//         免弱判定天然由 stat_drop 的 IMMUNE 档承担，无需另查。
//       ⚠️ **用户口径（同日）**：随机能力等级改变**不忽略已达上限(6)的条目**——
//         随机仍会随机到它、仍会调用 stat_change，只是原语内钳 AT_CAP → 看起来
//         没发生什么。本实现**不做"跳过满级槽"的预筛**（与 k893/k268 随机降属
//         的"去重"不同：去重是"不选重复槽"，不是"跳过满级槽"）。
//     - 选槽去重：picked[6] 循环（k893 / K268 796 / K282 645 同款——只避免同槽
//       重复被选，不跳过满级槽）。
//   子句②「自身**属性技能命中**时吸取对手第五技能2点PP值」
//     - 同一 ON_SKILL_HIT 时点分流：属性技 → 对手 **skills[4]**（第五技能）的
//       pp -= db[2]（clamp ≥0）+ emit EVENT_PP_REDUCED（slot=4）——直改 pp 的
//       范式抄 k_round_triple_branch（减 PP 必须 emit 事件，core_api 注释口径）。
//       ⚠️ 无限 PP 槽（pp == -1）跳过。
//   子句③「战斗阶段结束时…下回合所有技能先制+1 / 下回合使用技能后附加300点固定伤害」
//     - ROUND_END：本回合**未受攻击伤害** → 置"下回合先制"旗；**受了攻击伤害**
//       → 置"下回合技能后固伤"旗（两者互斥，官方"若…则…若…则…"）。
//       判"本回合是否受攻击伤害" = EVENT_TAKE_DAMAGE watcher 记伤旗（红伤时点，
//       K230/K260/K268 同款）。
//     - 下回合**先制+1**：BATTLE_FIRST_MOVE_RIGHT 节点消费旗 → ws.preemptive_level
//       [owner] += 1（**必须写 FMR 桶**：handle_BattleFirstMoveRight 入口 memset
//       会清零 ws.preemptive_level，SOP §二.2；K189 1016 / K227 2011 同款）。
//     - 下回合**使用技能后**附加300点固伤：SKILL_EFFECT 节点消费旗 → 对手
//       deal_pink_damage(opp, db[4], FIXED)（"固定伤害"= FIXED 档粉伤，K260 332 /
//       K233 844 同款口径）。⚠️ 旗标带回合号，避免跨回合误消费。
//   ⚠️ boss 无效：tips 尾注带（boss无效）→ 节点 boss_invalid 置位。
//
// ⚠️ 三查记录（author a13-kprod 2026-10-21）：①内核 grep 2152 无魂印行为硬编码
//    （moves_lib 的 2152 是**技能空间**同号：crawford_skills「构造IV：先出手时
//    造成的攻击伤害提升50%」——不同注册空间，不算已实现）；②effect_icon
//    kind=[12,1,9]、tips 为权威文（dump 原文已核，effect_info 无 2152 技能模板）；
//    ③registry soul 集合无 2152、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：K282 645 / K268 796（随机去重选槽 + stat_change/stat_drop）、
//   K189 1016（下回合旗 + FMR 消费 + 一次性消费）、k_round_triple_branch（减指定
//   槽 PP + emit EVENT_PP_REDUCED）、K260 332（粉伤 FIXED 口径）。
// 场景回归：test/scenario/scenario_5230_kulatake_drain_2152.cpp（号段 5230-5239）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 2152;
constexpr int kDrainSlotsFallback = 4;    // tips 字面"4项"（db[0] 读不到时回退）
constexpr int kDrainLevelFallback = 1;    // tips 字面"1级"（db[1] 读不到时回退）
constexpr int kPpDrainFallback = 2;      // tips 字面"2点PP"（db[2] 读不到时回退）
constexpr int kFixedFallback = 300;       // tips 字面"300点固定伤害"（db[4] 读不到时回退）
constexpr int kFifthSkillSlot = 4;        // 第五技能 = skills[4]

constexpr int kPreemptFlagBase = 992152000;   // +owner：下回合先制+1 旗
constexpr int kFixedFlagBase = 992152100;     // +owner：下回合技能后固伤旗（值=回合号）
constexpr int kTookDamageBase = 992152200;    // +owner：本回合受攻击伤害记伤旗
constexpr int kPickMaskBase = 992152400;      // +owner：本次随机选中槽掩码（审计槽，只读）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）─────────
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

int flag_get(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second, 0);
}

// ── ① 攻击技命中 → 随机 4 项能力吸取（降 1 级给自己）────────────────────────
EffectResult k2536_2152_on_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 只在命中时触发
    }
    const int idx = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (idx < 0 || idx >= static_cast<int>(skills.size())) {
        return EffectResult::kOk;
    }
    const bool is_attribute = skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute;
    const HostRow row = host_row(ctx, a, kMarkId);
    const int opp = ctx->opponent(owner);

    if (!is_attribute) {
        // 子句①：随机 db[0] 个互不重复的槽 → 对手降 db[1] 级 → 自己升 db[1] 级。
        const int slots = db_at(row, 0, kDrainSlotsFallback);
        const int level = db_at(row, 1, kDrainLevelFallback);
        if (slots <= 0 || level <= 0 || !g_core || !g_core->stat_drop || !g_core->stat_change) {
            return EffectResult::kOk;
        }
        int picked[6] = {};
        int done = 0;
        int guard = 0;
        while (done < slots && guard < 100) {
            ++guard;
            const int slot = sbs_rand() % 6;
            if (picked[slot]) {
                continue;   // 去重：同槽不重复选（**不是**跳过满级槽）
            }
            picked[slot] = 1;
            ++done;
            ctx->plugin_storage[kPickMaskBase + owner] =
                ctx->plugin_storage.count(kPickMaskBase + owner)
                    ? (any_int_or(ctx->plugin_storage[kPickMaskBase + owner]) | (1 << slot))
                    : (1 << slot);   // 审计槽：本次选中的槽位掩码
            // 弱化原语：查附加禁令 + **免弱 STAT_DROP**；IMMUNE（对手免弱）→
            // 等级一点不动 → **不给��己附加提升**（用户口径 2026-10-21）。
            if (g_core->stat_drop(ctx, opp, slot, level, owner) == StatDropResult::SUCCESS) {
                (void)g_core->stat_change(ctx, owner, slot, level);
            }
            // ⚠️ 满级槽（自己该槽已 +6）：不预筛、照常调用 stat_change → 原语内
            // 钳 AT_CAP → 看起来没发生什么（用户口径 2026-10-21）。
        }
        return EffectResult::kOk;
    }

    // 子句②：属性技命中 → 吸取对手**第五技能** db[2] 点 PP。
    const int pp_down = db_at(row, 2, kPpDrainFallback);
    if (pp_down <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(opp);
    if (foe.skills.size() <= static_cast<std::size_t>(kFifthSkillSlot)) {
        return EffectResult::kOk;
    }
    Skills& fifth = foe.skills[static_cast<std::size_t>(kFifthSkillSlot)];
    if (fifth.pp == -1) {
        return EffectResult::kOk;   // 无限 PP 槽跳过（core 同款口径）
    }
    const int before = fifth.pp;
    fifth.pp = std::max(0, fifth.pp - pp_down);
    if (fifth.pp != before) {
        // 减 PP 必须 emit 事件（core_api 注释：直写 pp 会绕过事件）。
        ctx->event_center_.emit(BattleEvent{
            EventType::EVENT_PP_REDUCED, /*actor=*/owner, opp,
            /*amount=*/before - fifth.pp, static_cast<int>(ctx->currentState),
            /*grant_id=*/-1, /*blocked=*/false, /*slot=*/kFifthSkillSlot});
    }
    return EffectResult::kOk;
}

// ── ② ROUND_END：未受攻击伤害 → 下回合先制+1 旗；受了 → 下回合技能后固伤旗 ──
EffectResult k2536_2152_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const bool took_damage = flag_get(ctx, kTookDamageBase + owner) == 1;
    if (!took_damage) {
        ctx->plugin_storage[kPreemptFlagBase + owner] = 1;   // 下回合所有技能先制+1
    } else {
        ctx->plugin_storage[kFixedFlagBase + owner] = ctx->roundCount + 1;  // 下回合技能后固伤
    }
    return EffectResult::kOk;
}

// ── ③ 下回合 FMR：消费先制旗 → ws.preemptive_level += 1 ─────────────────────
EffectResult k2536_2152_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int key = kPreemptFlagBase + owner;
    if (!ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(key);   // 一次性消费
    ctx->ws.preemptive_level[owner] += 1;   // 「所有技能先制+1」（无攻击门）
    return EffectResult::kOk;
}

// ── ④ 下回合 SKILL_EFFECT：消费固伤旗 → 附加 db[4] 点固定伤害 ───────────────
EffectResult k2536_2152_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int key = kFixedFlagBase + owner;
    const int armed_round = flag_get(ctx, key);
    if (armed_round <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(key);   // 一次性消费
    if (armed_round != ctx->roundCount) {
        return EffectResult::kOk;   // 旗是别的回合的 → 不消费（防跨回合误伤）
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;   // 本回合没出技能
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int fixed = db_at(row, 4, kFixedFallback);
    if (fixed <= 0) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, fixed, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── ⑤ arm：EVENT_TAKE_DAMAGE watcher 记"本回合受攻击伤害"旗 ─────────────────
EffectResult k2536_2152_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = 992152300 + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            wctx->plugin_storage[kTookDamageBase + owner] = 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（2152 入账，只增不改）──────
// （boss无效）尾注 → boss_invalid 置位。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2536_2152_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_ON_SKILL_HIT, &k2536_2152_on_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_ON_SKILL_HIT, &k2536_2152_on_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k2536_2152_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k2536_2152_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k2536_2152_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k2536_2152_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
