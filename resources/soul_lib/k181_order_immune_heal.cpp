// K181 先/后手双分支族·免异常/回血版 —— effect 617（1 宏 / 2 行）
//                                                                          （2026-10-07，worktree ../agents/k181）
//
// 行清单（tools/soul_effects_dump.py 617，2026-10-07）：
//   617 托鲁克尔 2861 两行 args[2,3]（本体）/"伙伴效果"行 args[2,2]：
//     「先出手时{db[0]}回合内免疫异常状态；后出手时立即恢复1/{db[1]}体力」
//   → db[0]=免疫窗回合数（两行均 2）；db[1]=恢复分母（本体 1/3、伙伴进化 1/2——
//     伙伴行恢复更多，与"伙伴效果"增强语义互锚）。
//
// 实现（SOUL_PROGRAM 两节点对，K173 809 同族形态）：
//   ① BATTLE_FIRST_SKILL_EFFECT（持印者先手执行时点——桶按 (state,持印者) 配对，
//     对手先手时不触发；先手方行动前无对手威胁，执行时点授窗 ≈ 回合初授窗）→
//     grant_immunity(ANOMALY, Ancient 层全 mask, duration=db[0], STAGE 域)
//     （diwang_3512 窗口免疫直系：每次先手执行重授、随下场掉）。
//   ② BATTLE_SECOND_SKILL_EFFECT（持印者后手执行）→ heal_amount(maxHP/db[1])
//     （622/636 回血同原语；heal 内部钳 max；hp≤0 守卫）。
//   ⚠️ 看板行路线提的 prev_round_preemptive_right（T2）无需使用：FIRST/SECOND 两侧
//     时点节点本身就是"本回合先/后手"的判据（K173 同款记档）；被控/NONE 回合无执行
//     → 两分支都不触发（与"出手"字面一致）。
//
// 三查记录（2026-10-07，立单时已核）：①内核 grep 617 无魂印行为硬编码；②effect_info
//   617 = 技能空间同号（魂印以 effect_icon tips 为权威）；③soul_lib 并集无 617、
//   registry.soul 无 617——真缺口。
// 先例：diwang_3512（ANOMALY Ancient 窗口免疫）、K10 637（heal_amount）、K173 809
//   （FIRST/SECOND_SKILL_EFFECT 节点对 + hp 守卫）。
// 场景回归：test/scenario/scenario_3140_order_immune_heal.cpp（T3140-T3142）。

#include "common.h"

namespace {

constexpr int k617ImmunitySource = 990181100;   // 免疫票 source_id（本单键段）

// ── 宿主行参数（各文件自治同款）────────────────────────────────────────────
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

// ── ① 先手分支：db[0] 回合免异常窗 ─────────────────────────────────────────
EffectResult soulmark_617_first_mover(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 617)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 617);
    const int rounds = db_at(row, 0, 2);
    if (rounds <= 0) {
        return EffectResult::kOk;
    }
    // 次免/回合类免疫（soul=false, Modern 层）：走施加链 [3] 早检查（先于抗性判定，
    // artemis_4745 免异常实证口径）。diwang 的 Ancient+soul=true 是"连主动毒都挡"的
    // 特化口径——617 官方文只是普通"免疫异常状态"，不取。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/0,   // 0=新建（非零=复用更新句柄，自造句柄不落票）
                        /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/617, ImmunityTier::Modern);
    return EffectResult::kOk;
}

// ── ② 后手分支：立即恢复 1/db[1] 体力 ──────────────────────────────────────
EffectResult soulmark_617_second_mover(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 617)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 617);
    const int denom = db_at(row, 1, 3);
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, resolve_pet_max_hp(host) / denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(617, {SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT,
                                   &soulmark_617_first_mover),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT,
                                   &soulmark_617_second_mover)});
