// J 组 J296 · 双概率异常/护盾兜底族 —— 1582×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1582（args_num=5）"{0}%令对手{1}，{2}%令自身{3}，均未触发则为自身附加{4}点护盾"
//
// 实现口径：
//   - 两段主干都走现代申报制 apply_anomaly_checked（ChanceSource::Skill）：
//     ① 对**对手**施加异常 {1}，概率 {0}%；② 对**自身**施加异常 {3}，概率 {2}%。
//   - "均未触发" = 两段**都返回 ROLL_FAILED**（概率没过门）。
//     ⚠️ 被判/抗性/反弹（TARGET_IMMUNE / RESISTED_BY_RESISTANCE / REFLECTED …）**不是**
//     "未触发"——ApplyAnomalyResult 语义 + J209/J210（2026-10-22 同批）口径：
//     只有 ROLL_FAILED 对应官方"未触发补偿"那一档。
//   - 兜底 = 自身 shield_bank_.add(priority=0, {4}, source=158200+owner, duration=0)
//     （普通来源护盾；J125 2005 / J295 1871 同通道）。
//   - pct 行参在位即消费：显式 0 = 该段必不触发（计入"未触发"），不报错（红线 7）；
//     {4} 显式 0 = 兜底不给盾（仍算落点，只是量 0）。
//   - 审计槽：991582100+owner / 991582101+owner = 两段 ApplyAnomalyResult 整数
//     （未调用记 -1）；991582200+owner = 本次授予盾量；991582300+owner = 兜底 0/1。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽、零剩余）：
//   m27162 双弹瓦斯 [1397,57,1582] → 1582=[60,9,30,6,400] /
//   m27616 古树庇护 → [50,27,50,1,300] / m29095 异质洋流 → [100,5,100,16,400] /
//   m35335 沙尘风暴眼 → [50,7,50,6,300] / m35411 平行镭射光 → [50,7,50,5,200] /
//   m36580 珍藏小食 → [15,1,15,1,200] /
//   **m37249 焰坠留痕 [1582] → [100,2,100,2,300]（纯链，测试载体）**。
//
// 动工前置三查（2026-10-22）：①内核 grep 1582 无行为硬编码（仅 j_shield_chance_double
//   注释引用其载体切片）②effect_info 仅本条模板 ③moves_lib/soul_lib 并集
//   （jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1582)——真缺口。
//
// 场景回归：test/scenario/scenario_6290_double_prob_shield.cpp（号段 6290-6299）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1582Result1Base = 991582100;   // +owner：对对手段结果
constexpr int k1582Result2Base = 991582101;   // +owner：对自身段结果
constexpr int k1582ShieldBase = 991582200;    // +owner：本次授予盾量
constexpr int k1582FallbackBase = 991582300;  // +owner：兜底 0/1

void j296_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 一段申报制施加。返回 true = "触发了"（任何非 ROLL_FAILED 档，含被免疫/抗性/反弹——
// 它们属于"触发了但被挡"，不是"未触发"）；false = 未触发（ROLL_FAILED / 未申报）。
// 同时把原始结果码写审计槽（未调用记 -1）。
bool j296_try_anomaly(BattleContext* ctx, int actor, int target, int pct, int anomaly,
                      int audit_key) {
    if (!g_core || !g_core->apply_anomaly_checked || pct <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        j296_set_storage(ctx, audit_key, -1);
        return false;   // pct 显式 0 / 非法码 = 必不触发（在位即消费）
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), actor, pct, ChanceSource::Skill);
    j296_set_storage(ctx, audit_key, static_cast<int>(r));
    return r != ApplyAnomalyResult::ROLL_FAILED;
}

}  // namespace

// ── 1582 —— {0}%令对手{1}，{2}%令自身{3}，均未触发则为自身附加{4}点护盾 ──
// int_args = [owner, target, pct1, anomaly1, pct2, anomaly2, shield]（args_num=5）。
EffectResult effect_1582_double_prob_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1582, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 1582, args,
                     "1582 布局=[owner,target,pct1,码1,pct2,码2,盾]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct1 = args.int_args[2];
    const int anomaly1 = args.int_args[3];
    const int pct2 = args.int_args[4];
    const int anomaly2 = args.int_args[5];
    const int shield = args.int_args[6];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1582, args, "需要 owner/target ∈ {0,1}");
    // pct/盾量在位即消费：显式 0 合法（该段必不触发 / 不给盾），负值才是布局错误。
    EFFECT_ARG_GUARD(pct1 >= 0 && pct2 >= 0 && shield >= 0, 1582, args,
                     "需要 pct1/pct2/盾 ≥ 0");

    const bool fired1 = j296_try_anomaly(ctx, owner, opp, pct1, anomaly1,
                                         k1582Result1Base + owner);
    const bool fired2 = j296_try_anomaly(ctx, owner, owner, pct2, anomaly2,
                                         k1582Result2Base + owner);
    if (fired1 || fired2) {
        j296_set_storage(ctx, k1582FallbackBase + owner, 0);
        j296_set_storage(ctx, k1582ShieldBase + owner, 0);
        return EffectResult::kOk;   // 任一段触发 → 不走兜底
    }
    j296_set_storage(ctx, k1582FallbackBase + owner, 1);
    j296_set_storage(ctx, k1582ShieldBase + owner, shield);
    if (shield > 0) {
        ctx->getPet(owner).shield_bank_.add(/*priority=*/0, shield,
                                            /*source_id=*/158200 + owner, /*duration=*/0);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1582, effect_1582_double_prob_shield)
