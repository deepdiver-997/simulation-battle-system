// K333 累积威力提升族 —— effect 667（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 667，2026-10-21）：
//   667 史卡夫 2943 args[5,30]（kind=[5,10]）：「每次使用伤害技能都能使自身技能威力
//     提升5%,最高30%」
//     → **db[0] = 每层提升 5%**（tips 字面 "5%" ↔ 5）；**db[1] = 上限 30%**（tips
//       字面 "最高30%" ↔ 30）。两参数与文本**完全自洽** → 均消费。
//     ⚠️ "伤害技能"口径：官方措辞用"伤害技能"（非"攻击技能"）——本单按**攻击技**
//       判读（属性技不造成伤害 → 不计入层数），记档此解读。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ①EVENT_SKILL_USED watcher（arm，early 幂等旗，TEAM scope）：自己使用**攻击技**
//      → 层数 +1（账本 soulmark_storage，**随宠存活**——官方"每次使用…提升"是
//      战斗内累积，回合间不清零；K244 842 叠层账本同款）。
//   ②BATTLE_FIRST/SECOND_SKILL_EFFECT 节点：印主本回合**确实出了技能**
//      （roundChoice[owner][0] == SELECT_SKILL）→ 威力视图改写：
//          v = v.value * (100 + min(层数 × db[0], db[1])) / 100
//      （K4 636 威力视图改写同款：ws.skill_power_view[owner] = ...，写即打
//      "变威力"标记 → 收尾重算公式；⚠️ view.value == -1 表示未物化 → 跳过，
//      K4 636 同款守卫）。
//   ⚠️ **层数与生效的时序**：SKILL_USED（使用起点）先于 SKILL_EFFECT（效果结算）
//      → 第 1 次使用后层数=1，**本次**技能按 1×5% 生效。官方"每次使用…都能使
//      自身技能威力提升5%"读作"使用即涨、随后生效"（K244 842 同款"每回合结束提高
//      多少，本回合伤害吃的是上一层"的相邻口径）——本单取"含本次"的读法，记档。
//   ⚠️ 封顶 = min(层数×db[0], db[1])（官方"最高30%"）。
//
// ⚠️ 三查记录（author a13-kprod 2026-10-21）：①内核 grep 667 无魂印行为硬编码；
//    ②effect_icon kind=[5,10]、tips 为权威文（dump 原文已核，effect_info 无 667
//    技能模板）；③registry soul 集合无 667、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k244_stack_amp_growth 842（累积层数 + 封顶 + 账本，旗舰）、k_round_stat 636
//   （SKILL_EFFECT 威力视图改写）、k_attack_prob 109/116/151（EVENT_SKILL_USED
//   攻击技窗）、K259 345（arm 幂等旗 + TEAM watcher）。
// 场景回归：test/scenario/scenario_4890_stack_power_667.cpp（号段 4890-4899）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 667;
constexpr int kStepFallback = 5;    // tips 字面 5%（db[0] 读不到时回退）
constexpr int kCapFallback = 30;    // tips 字面"最高30%"（db[1] 读不到时回退）
constexpr int kWiredKeyBase = 991333000;   // +owner：watcher 已挂旗标

// ── 账本：累积层数（随宠存活 = 下场保留）───────────────────────────────────
struct K333Runtime {
    int stacks = 0;
};

K333Runtime& k333_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K333Runtime{};
    }
    return std::any_cast<K333Runtime&>(storage[kMarkId]);
}

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

// arm：挂 EVENT_SKILL_USED watcher（自己用攻击技 → 层数 +1）。
EffectResult k333_667_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
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

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c) {
                return;
            }
            if (ev.actor != owner) {
                return;   // 只认自己使用
            }
            if (ev.used_attribute) {
                return;   // "伤害技能" = 攻击技（属性技不造成伤害，不计层数）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            K333Runtime& rt = k333_runtime(c, owner);
            ++rt.stacks;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// SKILL_EFFECT：按累积层数改写威力视图（封顶 db[1]）。
EffectResult k333_667_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;   // 本回合没出技能 → 不改写
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int step = db_at(row, 0, kStepFallback);
    const int cap = db_at(row, 1, kCapFallback);
    if (step <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    const K333Runtime& rt = k333_runtime(ctx, owner);
    if (rt.stacks <= 0) {
        return EffectResult::kOk;
    }
    int pct = rt.stacks * step;
    if (pct > cap) {
        pct = cap;   // 封顶（官方"最高30%"）
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value <= 0) {
        return EffectResult::kOk;   // 未物化（-1）/ 0 威力 → 不改写（K4 636 同款守卫）
    }
    view = view.value * (100 + pct) / 100;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（667 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k333_667_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k333_667_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k333_667_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
