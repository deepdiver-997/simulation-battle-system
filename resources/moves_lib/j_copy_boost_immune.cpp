// J 组 J304 · 复制提升+异常次免族 —— 1949×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1949（args_num=1）"复制对手能力提升状态，复制成功则自身免疫下{0}次受到的异常状态"
//
// 实现口径：
//   - **复制**（不是吸取）：逐槽读对手正等级（六槽含命中），用 stat_change 给自己
//     加**同样的正增量**；对手的正等级**保持不动**（与 1482 的 transfer_stat_boosts
//     吸取语义刻意不同——那条会把对手清零）。自身到 +6 时由原语钳住。
//   - "复制成功" = 对手至少存在 1 个正等级槽（有东西可复制）。自身全部到顶而无法
//     再加，仍算"复制成功"（复制动作成立），场景只锁"有正槽/无正槽"两档。
//   - 次免 = grant_immunity(ANOMALY, counts={0}, source_effect_id=1949, ON_STAGE)
//     （2059 同款次数票；counts=0 = 不授票，红线 7 在位即消费）。
//   - 审计槽：991949100+owner = 复制到的正槽数；991949200+owner = 授予的次免次数。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽）：
//   **m37318 秋叶静美 [1683,1949]|[3,1] → 1949=[1]（cat1 攻击技，场景载体；
//   1683 = 自侧提升不可被消除 3 回合，良性）** / m29080 回想重叠 → [1] /
//   m36513 二象抚心阵 → [1] / m36696 至暗·无量空邃 → [2] / m37435 旧日的井然 → [1] /
//   m38376 夜话 → [1] / m38397 流转 → [1]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1949 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1949)——真缺口。
//
// 场景回归：test/scenario/scenario_6370_copy_boost_immune.cpp（号段 6370-6379）。

#include "common.h"

namespace {

constexpr int kCopiedAuditBase = 991949100;    // +owner：复制到的正槽数
constexpr int kImmuneAuditBase = 991949200;    // +owner：授予的次免次数

void j304_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

}  // namespace

// ── 1949 —— 复制对手能力提升；复制成功则免疫下{0}次异常 ──
// int_args = [owner, target, counts]（args_num=1）。
EffectResult effect_1949_copy_boost_then_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1949, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1949, args,
                     "1949 布局=[owner,target,次数]（args_num=1）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1949,
                     args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    // 复制半：逐槽正等级 → 自身同增量；对手等级**不动**。
    int copied = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[opp][i];
        if (lv <= 0) {
            continue;
        }
        ++copied;
        (void)g_core->stat_change(ctx, owner, i, lv);   // 到顶由原语钳 +6
    }
    j304_set_storage(ctx, kCopiedAuditBase + owner, copied);
    if (copied == 0) {
        j304_set_storage(ctx, kImmuneAuditBase + owner, 0);
        return EffectResult::kOk;   // 复制未成功 → 后缀不申报
    }
    // 次免半：{0} 在位即消费——0/负档 = 不授票（不回退默认次数）。
    if (counts > 0) {
        (void)ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                  /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                  /*source_id=*/0, /*soul_immunity=*/false,
                                  EffectScope::ON_STAGE, /*counts=*/counts,
                                  /*source_effect_id=*/1949);
        j304_set_storage(ctx, kImmuneAuditBase + owner, counts);
    } else {
        j304_set_storage(ctx, kImmuneAuditBase + owner, 0);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1949, effect_1949_copy_boost_then_immune)
