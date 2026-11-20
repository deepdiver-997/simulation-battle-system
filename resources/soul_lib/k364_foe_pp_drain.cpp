// K364 对手PP消耗+免弱族 —— effect 639（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 639，2026-10-24）：
//   639 英普力亚 2898 两行（kind=[15]，row438 come=伙伴效果）：
//     row315 args[1,1]：「每回合，对手所有技能PP减少1点（boss无效）;免疫能力下降」
//     row438 args[2,1]：「伙伴效果 :每回合，对手所有技能PP减少2点（boss无效）；免疫能力下降」
//   行参解码（db_at 原序，与 tips 数值子句自证）：db[0]=每回合对手 PP 消耗量（1/2）；
//   db[1]=免疫能力下降旗（两行均 1）。
//
// 落法（K355 2381 k_dual_immune_pp 直系换参）：
//   ①PP 消耗 = **BATTLE_ROUND_START 节点** pp_reduce(对手, db[0])（原语自带全技能循环、
//     钳 0、无限 PP(pp==-1) 不参与；k355 回合首 PP 面同锚）。子句带"（boss无效）"
//     → 本节点 boss_invalid=true。
//   ②免疫能力下降 = grant_immunity STAT_DROP coverage_all ON_STAGE 永久票
//     （K275/K299/K355 同通道，Modern 档）；db[1] 旗驱动（两行均 1）。子句无 boss 尾注
//     → 本节点 boss_invalid=false（标注单位=子句，两节点各随各句）。
//
// 三查记录（2026-10-24 本 worktree 复核）：①内核 grep 639 无魂印行为硬编码；
//   ②effect_info 639 = 异文技能模板（"造成伤害{0}{1}，则下{2}回合所有技能附带{3}点
//   固定伤害"），魂印以 effect_icon tips 为权威（SOP §六.1，dump 两行已核）；
//   ③registry soul 集合无 639、soul_lib 全库无 SOUL_PROGRAM(639)——真缺口。
//   4399✓2898（id≤5000 真精灵）。
// 先例：k_dual_immune_pp 2381（STAT_DROP 永久票 + 回合首 PP 面直改，旗舰直系）、
//   core_api pp_reduce（全技能循环原语，PP 消耗面标准通道）。
// 场景回归：test/scenario/scenario_7090_foe_pp_drain_639.cpp（号段 7090-7099）。

#include "common.h"

#include <algorithm>

namespace {

constexpr int kMarkId = 639;

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致，K279 口径）──
struct HostRow {
    const std::vector<int>* db = nullptr;
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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── ①ROUND_START：对手所有技能 PP -db[0]（原语全技能循环、钳 0、无限 PP 不参与）──
EffectResult k364_639_pp_drain(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->pp_reduce || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    if (!onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int drain = db_at(row, 0, 1);
    if (drain <= 0) {
        return EffectResult::kOk;   // 无参不挂（K1 纪律）
    }
    (void)g_core->pp_reduce(ctx, 1 - a.owner, drain);
    return EffectResult::kOk;
}

// ── ②enter(early)：免疫能力下降（STAT_DROP 永久票，db[1] 旗驱动；K355 直系）─────
EffectResult k364_639_grant(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1
        || !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    if (db_at(row, 1, 1) == 0) {
        return EffectResult::kOk;   // 行未声明免疫（两行均 1；旗驱动）
    }
    ctx->grant_immunity(a.owner, ImmunityType::STAT_DROP, coverage_all(),
                        0, 0, 63901, true, EffectScope::ON_STAGE,
                        0, kMarkId, ImmunityTier::Modern);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（639 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k364_639_grant,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_START, &k364_639_pp_drain,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
