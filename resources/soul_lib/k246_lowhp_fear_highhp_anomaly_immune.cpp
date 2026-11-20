// K246 低体害怕+高体免异常窗族 —— effect 826（1 宏 / 2 行）
//（霍德尔 3200；author k-prod-5 2026-10-13 立单，号段 3930-3939）
//
// 行清单（tools/soul_effects_dump.py 826，2026-10-13）：
//   row523 args[2,2,1]    「战斗阶段结束时，若自身体力小于1/2则使对手害怕；战斗阶段
//                          结束时，若自身体力大于1/2则下回合自身免疫异常状态（BOSS无效）」
//   row532 args[2,2,1,1]  同上「下2回合自身免疫异常状态」（霍德尔暗影传承魂印进化）
//   → 免异常回合数 = db[2]+db[3]（缺 0）：行1=1、行2=2，与两行文一一对应（**和式解码
//     记档**：单参无法同时表达两行时长）；db[0]=db[1]=2 与行差无关、无对应子句——
//     不消费，记档（疑为 1/2 阈值分母）。
//
// 实现（BATTLE_ROUND_END 桶单节点，两分支按自身体力对自身 maxHP/2 严格对照，相等不触发）：
//   · 低体：apply_anomaly_checked(对手, 害怕6, random_anomaly_duration, own, 100,
//     SoulMark)（无条件施加=100% 申报）；
//   · 高体：grant_immunity(自身, ANOMALY, coverage_all, duration=N+1)。
//     ⚠️ rc 滞后补偿（K228/1576 同族口径）：grant_immunity 以授予时 roundCount 为
//     register_round；ROUND_END 授予时 rc=R-1（0 基），"下N回合"须覆盖 R+1..R+N →
//     window_expired（cur-reg>=rem）下 duration=N+1 恰好（cur>=R-1+N+1=R+N 过期）。
// boss 无效标注：tips 尾注 → 节点 boss_invalid。
// ⚠️ 三查记录（k-prod-5 2026-10-13）：①内核无 826 硬编码；②effect_info 826 同号模板
//    不同文（args_num=1 错位行，按行 tips 落地）；③soul_lib/registry 无 826 注册——真缺口。
// 先例：chaos_sorensen ④（ROUND_END 锚+grant_immunity 形）、K172（申报制）、
//   k225（soulmark runtime）。
// 场景回归：test/scenario/scenario_3930_lowhp_fear_highhp_immune.cpp（号段 3930-3939）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 826;

struct K246Row {
    int immune_rounds = 1;   // 行1=1 / 行2=2（和式解码）
};

K246Row k246_read_row(BattleContext* ctx, int owner) {
    K246Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 2 && db[2] > 0) {
        row.immune_rounds = db[2];
    }
    if (db.size() > 3 && db[3] > 0) {
        row.immune_rounds += db[3];
    }
    return row;
}

// ── 战斗阶段结束：低体害怕 / 高体免异常窗 ─────────────────────────────────────

EffectResult k246_826_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    const int half = resolve_pet_max_hp(self) / 2;
    const int opp = ctx->opponent(owner);
    if (self.hp < half) {
        // 低体：使对手害怕（无条件 = 100% 申报）
        (void)g_core->apply_anomaly_checked(
            ctx, opp, static_cast<int>(AbnormalStatusId::Fear),
            random_anomaly_duration(), owner, /*chance_pct=*/100,
            ChanceSource::SoulMark);
        return EffectResult::kOk;
    }
    if (self.hp > half) {
        // 高体：下 N 回合自身免疫异常（duration=N+1 补 rc 滞后，见头注）
        const int n = k246_read_row(ctx, owner).immune_rounds;
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/n + 1,
                            /*source_id=*/0, /*soul_immunity=*/false,
                            EffectScope::ON_STAGE, /*counts=*/0, kMarkId);
    }
    return EffectResult::kOk;   // hp == half：两分支都不触发（严格对照）
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（826 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k246_826_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
