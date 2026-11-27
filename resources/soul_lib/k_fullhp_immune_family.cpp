// K171 满体力条件免疫族 —— effect 285（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 285，2026-10-07）：
//   285 将神·吕布 2496 两行 args[免异常%, 免降%]：
//     「满体力时100%免疫异常状态，满体力时{1}%概率免疫能力下降状态」
//     [100,50] / [100,100]（进化行 100/100，come=将神·吕布魂印进化限时活动）
//
// 落法（2492 圣莫概率免控同款，shengguang_monika_2492.cpp）：enter + ROUND_START
// 双节点挂两把固定句柄窗票（duration=1，enter 覆盖首回合、ROUND_START 每回合重授
// 刷新），**满体力门与掷骰都在 condition 里现场判**——condition 是逐次施加时点的
// 判定（grant_immunity 注释：查询点每次施加掷一次，回合窗口票不因掷骰消耗），
// 回合中途掉血/回血立即反映，忠实"满体力时"字面（不在挂票时判——那会把整回合
// 钉死在挂票时刻的血量上）。
//   票1 ANOMALY 魂免（soul=true，221 首回合免异常同层，K163）：满体力 && roll(db[0])
//   票2 STAT_DROP 免弱（270 免降同层，K163）：满体力 && roll(db[1])
// db[0] 两行恒 100（roll_percent(100) 恒真=确定免）；db[1]=50/100（50 档逐次掷）。
// enter 首挂的理由：R1 的票赶在首个 ROUND_START 桶之前生效（enter 在部署段执行），
// 且场景可在 start() 停泊点直接断言首回合行为（2880/221 同款时序）。
//
// 三查记录（2026-10-07）：①内核 grep 285 无魂印行为硬编码 ②effect_info 无 285 行
// ③soul_lib/registry（soul=118 基线）无注册——真缺口。4399✓2496 将神·吕布收录。
// 场景回归：test/scenario/scenario_3050_k171_effect285.cpp（号段 3050-3059）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
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


// ── 挂票体（enter 首挂 / ROUND_START 每回合重授共用；T4 类型化行参）─────────
EffectResult k171_285_grant(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 285)) {
        return EffectResult::kOk;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, 285);
    }
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, 285);
    const int anomaly_pct = db_at(row, 0, -1);
    const int drop_pct = db_at(row, 1, -1);
    if (anomaly_pct <= 0 && drop_pct <= 0) {
        return EffectResult::kOk;   // 无参不挂票（K1 纪律：不造 0% 票）
    }
    // 逐次结算时先查满体力，再用本局 RNG 掷点。
    auto roll_at_full_hp = [owner, slot](BattleContext* c, int subsite, int pct) {
        if (!c || c->on_stage[owner] != slot) {
            return false;   // 换下场即失去"在场满体力"前提
        }
        const ElfPet& pet = c->seerRobot[owner].elfPets[slot];
        return pet.hp > 0 && pet.hp >= pet.numericalBase[NumericalPropertyIndex::HP]
            && roll_soul_percent(*c, 285, subsite, owner, owner, pct);
    };
    if (anomaly_pct > 0) {
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/1,
                            /*source_id=*/285100, /*soul_immunity=*/true,
                            EffectScope::ON_STAGE, /*counts=*/0,
                            /*source_effect_id=*/285, ImmunityTier::Modern,
                            /*condition=*/nullptr, [anomaly_pct, roll_at_full_hp](BattleContext* c, int) {
                                return roll_at_full_hp(c, 0, anomaly_pct);
                            });
    }
    if (drop_pct > 0) {
        ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/1,
                            /*source_id=*/285200, /*soul_immunity=*/true,
                            EffectScope::ON_STAGE, /*counts=*/0,
                            /*source_effect_id=*/285, ImmunityTier::Modern,
                            /*condition=*/nullptr, [drop_pct, roll_at_full_hp](BattleContext* c, int) {
                                return roll_at_full_hp(c, 1, drop_pct);
                            });
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(285, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k171_285_grant,
                             /*once=*/false, /*early=*/false),
                   SOUL_NODE(State::BATTLE_ROUND_START, &k171_285_grant,
                             /*once=*/false, /*early=*/false)});
