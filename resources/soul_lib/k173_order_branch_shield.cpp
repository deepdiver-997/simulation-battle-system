// K173 先/后手双分支族·护盾版 —— effect 809（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 809，2026-10-07）：
//   809 王·卡修斯 3170 两行 args[200,1,2,1]（本体）/ [250,1,2,1]（"大地传承魂印进化"
//   进化行，同宠同 id）："先出手时，对手当回合技能无效；后出手时，当回合回合结束后
//   增加{db[0]}点护盾并使自身下回合自身先制+{db[2]}（BOSS无效）"
//   → db[0]=护盾量（200/250，进化行更大）；db[1]=先手分支开关（恒1）；
//     db[2]=下回合先制增量（两行均 2，与 tips "先制+2" 互锚）；db[3]=后手分支开关（恒1）。
//
// 实现（SOUL_PROGRAM 四节点；尾注（BOSS无效）作用于整印 → 全节点标 boss_invalid）：
//   ① BATTLE_FIRST_SKILL_EFFECT：持印者**先手执行**时点（桶按 (state,持印者) 配对——
//     对手先手时本节点不触发）→ seal_skill(attribute+attack=SEAL_ALL, count=1)：
//     对手紧接着的当次技能执行被无效（chaos_sorensen 1011③"下1次属性技无效"同通道、
//     双旗全开即全技能；1217/936 "无效"口径=非 hit_invalid）。
//     ⚠️ 边界记档：若对手当回合不执行技能（NONE/被控），次数封印顺延到其下次执行才被
//     消费——与官方"当回合"字面有偏差，先行例口径（1011 同款 count 型）不另设回合闸。
//   ② BATTLE_SECOND_SKILL_EFFECT：持印者**后手执行** → 置旗（soulmark_storage，
//     990173 键段）：护盾旗 + 先制旗（db[3] 门）。
//   ③ BATTLE_ROUND_END：消费护盾旗 → shield_bank_.add(kSoulShieldPriority, db[0],
//     source, 0)（artemis_4745 魂印盾同款：priority 1000、同源刷新不过量叠条）。
//   ④ BATTLE_FIRST_MOVE_RIGHT：消费先制旗 → ws.preemptive_level[owner]+=db[2]
//     （diwang_3512 "先制+2" MOVE_RIGHT 节点同款——FMR 桶在 preemptive memset 之后
//     跑 → 当回合生效；hamo_wing 旗标消费同窗）。下回合唯一性 = 旗在消费时清除，
//     无旗的回合节点空转。
//
// 三查记录（2026-10-07）：①内核无 809 魂印行为硬编码；②effect_info 无 809 技能模板行
//   （魂印语义以 effect_icon tips 为权威）；③soul_lib 并集无 809 注册、registry.soul
//   无 809——真缺口。
// 场景回归：test/scenario/scenario_3070_order_shield.cpp（T3070-T3073）。

#include "common.h"

#include <entities/shield_bank.h>

namespace {

// 旗标/护盾来源键（990173 = 本单键段，plugin/魂印 storage 互不冲突）。
constexpr int k809ShieldFlagKey = 990173001;  // 后手→本回合末加盾（int 1）
constexpr int k809PreemptFlagKey = 990173002; // 后手→下回合先制+db[2]（int 1）
constexpr int k809ShieldSource = 990173100;   // shield_bank 来源身份

// 宿主行参数视图（K168/K10 各文件自治同款）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// int 旗标读写（K10 227 计数器同形：先 find 后 insert，any_cast 指针形式）。
int flag_get(ElfPet& pet, int key) {
    auto it = pet.soulmark_storage.find(key);
    if (it == pet.soulmark_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

void flag_set(ElfPet& pet, int key, int value) {
    pet.soulmark_storage[key] = int{value};
}

// 旗标消费：返回旧值并清零。
int flag_consume(ElfPet& pet, int key) {
    const int old = flag_get(pet, key);
    if (old != 0) {
        flag_set(pet, key, 0);
    }
    return old;
}

// ── ① 先手分支：对手当回合技能无效 ─────────────────────────────────────────
EffectResult soulmark_809_first_mover(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 809)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 809);
    if (db_at(row, 1, 1) <= 0) {
        return EffectResult::kOk;   // db[1]=先手分支开关（恒1）
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/1 - owner, /*effect_id=*/809,
                       /*attribute=*/true, /*attack=*/true, /*count=*/1,
                       /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
                       EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── ② 后手分支：置旗（回合末盾 + 下回合先制）────────────────────────────────
EffectResult soulmark_809_second_mover(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 809)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 809);
    if (db_at(row, 3, 1) <= 0) {
        return EffectResult::kOk;   // db[3]=后手分支开关（恒1）
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    if (db_at(row, 0, 0) > 0) {
        flag_set(host, k809ShieldFlagKey, 1);
    }
    if (db_at(row, 2, 0) > 0) {
        flag_set(host, k809PreemptFlagKey, 1);
    }
    return EffectResult::kOk;
}

// ── ③ 回合末：消费护盾旗 → +db[0] 护盾 ─────────────────────────────────────
EffectResult soulmark_809_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 809)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.hp <= 0 || !flag_consume(host, k809ShieldFlagKey)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 809);
    const int amount = db_at(row, 0, 0);
    if (amount > 0) {
        host.shield_bank_.add(ShieldBank::kSoulShieldPriority, amount, k809ShieldSource,
                              /*duration=*/0);
    }
    return EffectResult::kOk;
}

// ── ④ 下回合 FMR：消费先制旗 → 先制+db[2] ──────────────────────────────────
EffectResult soulmark_809_fmr_preempt(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 809)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.hp <= 0 || !flag_consume(host, k809PreemptFlagKey)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 809);
    const int bonus = db_at(row, 2, 0);
    if (bonus > 0) {
        ctx->ws.preemptive_level[owner] += bonus;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(809, {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT,
                                   &soulmark_809_first_mover,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT,
                                   &soulmark_809_second_mover,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_809_round_end,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT,
                                   &soulmark_809_fmr_preempt,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});
