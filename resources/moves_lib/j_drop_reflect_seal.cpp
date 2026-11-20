// J 组 J298 · 下降双倍反馈+封技族 —— 1289×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1289（args_num=1）"将自身的能力下降状态双倍反馈给对手，反馈成功则对手下{0}次技能无效"
//
// 实现口径（直系先例）：
//   - 反馈半 = j_drop_reflect_immune.cpp 1260 / j_stat_drop_reflect.cpp 813：
//     自身每个负槽（六槽含命中）按 2×|深度| 走 stat_drop(对手, 同槽, ...)——
//     弱化原语查免弱（STAT_DROP）与附加禁令、钳 -6；自身下降**不清除**（模板无清除子句）。
//   - 反馈成功 = ≥1 槽 stat_drop 落地 SUCCESS（1260 同判据；对手免弱 / 全在 -6 →
//     反馈未落地 → 不授封技票）。
//   - 封技半 = 次数型 SEAL_ALL（884 同款）：attribute=true + attack=true，count={0}，
//     duration=0；penetrable=true（次数型技能无效可被穿盔凭证穿，884 口径）。
//   - 行参在位即消费：{0} 显式 0/负 = 不授票（反馈照常落地，不回退默认次数）。
//   - 审计槽：991289100+owner = 反馈落地槽数（SUCCESS 计数）；
//     991289200+owner = 本次授予的封技次数（0 = 未授）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，8/8 自洽）：
//   **m36720 炽风雷灼 [1289]|[1]（纯链，cat2 特攻，场景载体）** / m25892 洪荒龙威 →
//   [1] / m26949 红叶丹歌 → [3] / m28654 淬焰凡芯 → [1] / m28679 寒缠凄风 → [250]（异常
//   行参错位已记档，不影响本效果） / m36301 → [1] / m38125 → [1] / m38137 → [2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1289 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1289)——真缺口。
//
// 场景回归：test/scenario/scenario_6310_drop_reflect_seal.cpp（号段 6310-6319）。

#include "common.h"

namespace {

constexpr int kLandedAuditBase = 991289100;   // +owner：反馈落地槽数
constexpr int kSealAuditBase = 991289200;     // +owner：授予封技次数

void j298_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ── 1289 —— 自身下降双倍反馈；反馈成功则对手下{0}次技能无效 ──
// int_args = [owner, target, seal_count]（args_num=1 → 装载器注入 owner/target）。
EffectResult effect_1289_reflect_double_then_seal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1289, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1289, args,
                     "1289 布局=[owner,target,次数]（args_num=1）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int seal_count = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1289,
                     args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // 反馈半：逐负槽双倍深度镜像（SUCCESS 含"压到 -6 为止"的部分生效）。
    int landed = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int depth = ctx->ability_levels[owner][i];
        if (depth >= 0) {
            continue;
        }
        if (g_core->stat_drop(ctx, opp, i, -2 * depth, owner) == StatDropResult::SUCCESS) {
            ++landed;
        }
    }
    j298_set_storage(ctx, kLandedAuditBase + owner, landed);
    if (landed == 0) {
        j298_set_storage(ctx, kSealAuditBase + owner, 0);
        return EffectResult::kOk;   // 反馈未落地 → 后缀不申报
    }
    // 封技半：{0} 在位即消费——0/负档 = 不授票（不回退默认次数）。
    if (seal_count > 0 && g_core->seal_skill) {
        (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/1289,
                                 /*attribute=*/true, /*attack=*/true, /*count=*/seal_count,
                                 /*duration_rounds=*/0, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
        j298_set_storage(ctx, kSealAuditBase + owner, seal_count);
    } else {
        j298_set_storage(ctx, kSealAuditBase + owner, 0);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1289, effect_1289_reflect_double_then_seal)
