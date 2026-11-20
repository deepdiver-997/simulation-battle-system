// K188 死亡消回合+下只减伤族 —— effect 983（1 宏 / 2 行）
//                                                                          （2026-10-07，worktree ../agents/k188）
//
// 行清单（tools/soul_effects_dump.py 983，2026-10-07）：
//   983 步练师 3385 两行 args[2,50,1]（本体）/"共承天地伙伴效果"行 args[3,50,1]：
//     「自身死亡时消除对手回合类效果，同时使己方下只出战精灵下{db[0]}次受到的攻击伤害
//     减少{db[1]}%（BOSS无效）」
//   → db[0]=承担次数（2/3，伙伴行多一次）；db[1]=减伤百分比（恒 50）；db[2]=boss旗
//     （tips 尾注（BOSS无效）→ 节点级 boss_invalid，db 位不消费，记档）。
//
// 实现（SOUL_PROGRAM 死亡双桶节点 + 死亡时安装的 TEAM 常驻 MITIGATE 回调）：
//   ① BATTLE_FIRST/SECOND_MOVER_DEATH（324 同桶对，同 handler）：门 = 持印者在场
//     （死亡时点桶宿主仍占 on_stage 槽）+ 宿主已倒 → break_round_effects(opp)
//     （1006/323/puni_1038 同原语；NO_EFFECTS/IMMUNE 三态不区分、照常给承担）→
//     承担账本落 plugin_storage（k983BequestCounts/Pct+owner），并**首次**安装
//     REDUCE_PCT MITIGATE 回调（k983Wired 门防二次注册；二次死亡只刷新账本）。
//   ② MITIGATE 回调（batch_c REDUCE_PCT 同形）：bucket_owner==own &&
//     d.defenderId==own && d.isRed && !d.isDirect && d.final>0 && counts>0 →
//     d.final -= d.final*pct/100，counts--（扣完自动哑火）。
//   ⚠️"下只出战精灵"口径：宿主死亡后己方场上无活体，下一笔己方承伤必然来自接班
//     精灵（死亡→CHOOSE_AFTER_DEATH 强制换人），无需显式换人旗；若继任者把次数
//     消耗完之前再次死亡，账本被新死亡刷新（官方文未细分，取"最后一次死亡优先"）。
//
// 三查记录（2026-10-07，立单时已核）：①内核 grep 983 无魂印行为硬编码；②effect_info
//   同号为技能空间模板（魂印以 effect_icon tips 为权威）；③soul_lib 并集无 983、
//   registry.soul 无 983——真缺口。
// 先例：k_death_revenge 324（MOVER_DEATH 桶对+宿主已倒门）、puni_1038（break_round_effects
//   三态）、batch_c REDUCE_PCT MITIGATE（d.final 减法式+红伤非直伤门）、K168（wired 门）。
// 场景回归：test/scenario/scenario_3200_death_break_bequest.cpp（T3200-T3202）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k983BequestCounts = 990188001;   // +owner：剩余承担次数
constexpr int k983BequestPct = 990188003;      // +owner：减伤百分比
constexpr int k983Wired = 990188005;           // +owner：MITIGATE 回调已装旗
constexpr int k983ArmedEpoch = 990188007;      // +owner：已落账死亡时的 pipeline epoch

int stor_int(BattleContext* ctx, int key) {
    auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second, 0);
}

void stor_set(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = int{value};
}

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

// ── ① 死亡双桶：断对手回合类 + 落承担账本 ──────────────────────────────────
EffectResult soulmark_983_death(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 983)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    if (ctx->seerRobot[owner].elfPets[slot].hp > 0) {
        return EffectResult::kOk;   // 宿主未倒（死亡时点桶）
    }
    // ⚠️ 更新器每回合重注册节点、MOVER_DEATH 态每回合都走 → 同一具尸体在占槽期间
    // 会反复触发本节点：以死亡时的 pipeline epoch 作"同尸"门（新接班宠 epoch 必变，
    // 其后再死再落账）。断回合/落账都只在真死亡那次执行。
    const int epoch = ctx->pipeline_valid_id[owner];
    if (stor_int(ctx, k983ArmedEpoch + owner) == epoch) {
        return EffectResult::kOk;
    }
    stor_set(ctx, k983ArmedEpoch + owner, epoch);
    const int foe = 1 - owner;
    if (g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, foe);
    }
    const HostRow row = host_row(ctx, args, 983);
    const int counts = db_at(row, 0, 0);
    const int pct = db_at(row, 1, 50);
    if (counts <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    stor_set(ctx, k983BequestCounts + owner, counts);
    stor_set(ctx, k983BequestPct + owner, pct);
    if (stor_int(ctx, k983Wired + owner) == 0) {
        stor_set(ctx, k983Wired + owner, 1);
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                int remain = stor_int(c, k983BequestCounts + owner);
                if (remain <= 0) {
                    return;   // 次数耗尽/未落账 → 哑火
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != owner || !d.isRed || d.isDirect || d.final <= 0) {
                    return;   // 只管己方受到的攻击红伤（非直伤）
                }
                const int pct2 = stor_int(c, k983BequestPct + owner);
                if (pct2 <= 0) {
                    return;
                }
                d.final -= d.final * pct2 / 100;
                stor_set(c, k983BequestCounts + owner, remain - 1);
            });
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(983, {SoulMarkNodeRef(State::BATTLE_FIRST_MOVER_DEATH,
                                   &soulmark_983_death,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true),
                   SoulMarkNodeRef(State::BATTLE_SECOND_MOVER_DEATH,
                                   &soulmark_983_death,
                                   /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                   /*boss_invalid=*/true)});
