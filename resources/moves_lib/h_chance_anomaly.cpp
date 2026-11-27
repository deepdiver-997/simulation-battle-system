#include <utils/sbs_rand.h>
// H 组 H7 · 概率异常族 —— 99×27 / 451×24（2026-09-30，worktree ../agents/h7）
//
// 家族语义（effect_info 模板 + 4399 双源逐字）：
//   99（args_num=1）"命中时，{0}%概率令对手混乱"
//     —— 噩梦缠身 11701 [10]="10%几率令对方混乱"、混乱魔光 20845 [100]="100%"、
//     虚晃一击 34663 [30]="30%"。混乱 = AbnormalStatusId::Confusion(10)。
//   451（args_num=2）"{0}%随机附加烧伤，冻伤，中毒中的{1}种"
//     —— 煞星玄黄令 17682 [100,1]="100%随机令对手进入烧伤，冻伤，中毒中的一种"、
//     天之环 13553 [30,1]="30%…一种"、荒凉幻境 19156 [100,1] 同文。
//     {1}=种数——全库 24 载体主干切片 [chance, 1]（16 条），无 {1}>1 实载
//     （多值切片均为链式溢出：[3,100,1] 等前缀属链上其他效果）。
//
// 实现范式：
//   - 99 = apply_anomaly_checked 直接申报（概率闸门管道，F9 782 同款调用面）——
//     "命中时"时点 = SKILL_EFFECT 桶（效果结算期、本就后于命中判定，天然成立）。
//   - 451 = 种类 roll 本地三选一（rng.next()%3：烧伤2/冻伤5/中毒1）+ apply_anomaly_
//     checked(chance={0}) 申报——概率仍走闸门管道（申报制：未中无副作用，种类
//     roll 白掷无害）；与 F8 1244 随机区间的本地 rng.next() 同风格（数值 roll 本地、
//     概率判定归原语）。{1}>1 无实载，按 kinds 循环不重复选种实现（每步独立申报
//     chance），记档待官方实测翻案点。
//
// 动工前置三查（2026-09-30）：①内核 grep 99/451 无硬编码（命中均为 99% 数值注释
// 噪声）②custom_effect_seed.sql 零命中 ③moves_lib 无既有注册——真缺口。
// 先例：F9 782（apply_anomaly_checked 申报面）、F8 1244（随机本地 roll）、
// 782 审计槽口径（场景无随机断言用，本族概率单一值无需审计槽——场景走统计带）。
//
// 场景回归：test/scenario/scenario_850_chance_anomaly.cpp（850-859 号段）：
//   概率类断言口径（常识速查 §五 + F9 782 先例）：chance=100 → 无随机精确断言
//   （必然施加）；chance=0/载体外对照 → 必不施加；中间档（45/30/50）→ 统计带
//   侧证（40 回合落地次数 ≈ 期望 ± 余量，不精确断言）；451 的种类分布 = 落地
//   回合里三种各出现 ≥1（三选一均匀性弱断言）。

#include "common.h"

#include <set>
#include <vector>

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 99 —— 命中时，{0}%概率令对手混乱 ──
// int_args = [owner, target, {0}=概率百分点]。
EffectResult effect_99_chance_confusion(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 99, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 99, args,
                     "99 布局=[owner,target,概率]（int_count>=3）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     99, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 99, args, "需要 0 ≤ 概率 ≤ 100");
    if (!g_core || !g_core->apply_anomaly_checked || chance <= 0) {
        return EffectResult::kOk;   // chance=0：必不施加（申报原语对 0 也拒，早退省调用）
    }
    g_core->apply_anomaly_checked(ctx, target, static_cast<int>(AbnormalStatusId::Confusion),
                                  random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 451 —— {0}%随机附加烧伤，冻伤，中毒中的{1}种 ──
// int_args = [owner, target, {0}=概率百分点, {1}=种数]。
EffectResult effect_451_random_triple_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 451, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 451, args,
                     "451 布局=[owner,target,概率,种数]（int_count>=4）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int kinds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && target != owner,
                     451, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 451, args, "需要 0 ≤ 概率 ≤ 100");
    EFFECT_ARG_GUARD(kinds >= 1 && kinds <= 3, 451, args, "需要 1 ≤ 种数 ≤ 3");
    if (!g_core || !g_core->apply_anomaly_checked || chance <= 0) {
        return EffectResult::kOk;
    }
    // 候选池 = 烧伤(2) / 冻伤(5) / 中毒(1)；不重复选 kinds 种（全库实载 kinds=1 主干）。
    const std::vector<int> pool = {static_cast<int>(AbnormalStatusId::Burn),
                                   static_cast<int>(AbnormalStatusId::Frostbite),
                                   static_cast<int>(AbnormalStatusId::Poison)};
    std::set<int> picked;
    while (static_cast<int>(picked.size()) < kinds) {
        picked.insert(pool[ctx->rng().next() % static_cast<int>(pool.size())]);
    }
    for (const int anomaly : picked) {
        // 概率判定归申报原语（闸门管道一致）；未中无副作用，种类 roll 白掷无害。
        g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(), owner,
                                      chance, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(99, effect_99_chance_confusion);
SKILL_EFFECT(451, effect_451_random_triple_anomaly);
