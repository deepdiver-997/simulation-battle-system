// K339 低体先制/高体增伤双分支族 —— effect 708（1 宏 / 1 行）
//（时空界王 3013 args[2,1,50]；号段 4950-4959，worktree ../agents/k339，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 708，2026-10-21）：
//   「回合开始时体力小于1/2时则所有技能先制+1; 回合开始时体力大于1/2时则所有攻击
//   技能伤害提升50%」
//   → db[1]=1 = 先制量（"+1"自证）；db[2]=50 = 增伤%（"50%"自证）；db[0]=2 废位
//   不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b708\b 无魂印硬编码；②effect_info
//    708 技能空间模板不同文；③soul_lib 全 worktree/registry 无 708 注册——真缺口。
// 实现（K236 账本形 + K283 增伤槽拼装，双分支同源 hp 门、回合开始评估）：
//   BATTLE_ROUND_START 桶（门在回合开始评估——tips 显式锚）：
//   · hp×2 < max（"小于1/2"严格）→ 账本记当轮 → BATTLE_FIRST_MOVE_RIGHT 桶
//     ws.preemptive_level[owner] += db[1]（memset 后写，K236 同款）；
//   · hp×2 > max（"大于1/2"严格）→ ws.damage_add_pct[owner] += db[2]（K283 同槽，
//     ws 每回合 reset → 每回合重写）；
//   · 恰 = 1/2 → 双分支都不落（两条严格比较）。
// 注册：SOUL_PROGRAM(708) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4950_lowpreempt_highamp.cpp（号段 4950-4959）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 708;
constexpr int kGateKeyBase = 990839000;   // +owner：低体先制账本（值=当轮 roundCount）

// 行参：db[1]=先制量（兜底 1）、db[2]=增伤%（兜底 50）。
void k339_read_row(BattleContext* ctx, int owner, int& preempt, int& amp) {
    preempt = 1;
    amp = 50;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 1 && db[1] > 0) {
        preempt = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        amp = db[2];
    }
}

// ROUND_START：hp 门双分支评估。
EffectResult k339_708_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    int preempt = 1;
    int amp = 50;
    k339_read_row(ctx, owner, preempt, amp);
    if (self.hp * 2 < max_hp) {
        ctx->plugin_storage[kGateKeyBase + owner] = ctx->roundCount;   // 低体账本
    } else if (self.hp * 2 > max_hp) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);   // 高体增伤
    }
    return EffectResult::kOk;
}

// FMR：低体账本当轮有效 → 先制 +db[1]。
EffectResult k339_708_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const auto gate = ctx->plugin_storage.find(kGateKeyBase + owner);
    if (gate == ctx->plugin_storage.end() ||
        any_int_or(gate->second) != ctx->roundCount) {
        return EffectResult::kOk;   // 无账本/非当轮
    }
    int preempt = 1;
    int amp = 50;
    k339_read_row(ctx, owner, preempt, amp);
    ctx->ws.preemptive_level[owner] += preempt;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（708 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k339_708_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k339_708_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
