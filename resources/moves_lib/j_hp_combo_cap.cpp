#include <utils/sbs_rand.h>
// J 组 J253 · 体力差连击族 —— 1593×7（2026-10-24，worktree ../agents/j253，orchestrator 代开）
//
// 家族语义（effect_info 官方模板即权威）：
//   1593 [3] 1回合做{0}-{1}次攻击，自身体力低于对手时连击上限为{2}
//
// 实现口径（1500 连击上限直系，batch_b_skills effect_1500_combo_cap）：
//   连击区间 {0}~{1} 已由内核 combo_arg_indices（skills.cpp 开关表 case 1593 → (0,1)）
//   在装弹时取走、每次使用掷一次写 ws.combo_view——插件不重复。本处理器只补条件子句：
//   SKILL_EFFECT 时自身体力 < 对手体力 → 上限改为 {2}：有效区间 [lo,cap] 均匀重掷一次
//   并写 combo_view（写=声明变威力，ATTACK_DAMAGE 收尾重算第二次）。
//   ⚠️ 纯链载体（连环殴打 [3,7,9] / 月夜琳琅 [15,25,30]）的 cap 全部 **> hi** ——
//   官方这里是"体力低时上限提高"的绝境加成（不是向下钳；向下钳是 1577"PP 小于"那族）。
//   cap ≤ hi 的行重掷区间 [lo,cap] 同样成立（上限字面替换），无需分支。
//
// 载体切片（unity 库 json 解析精确匹配 pure=2 mixed=5）：
//   1593：纯链 连环殴打 36895 [3,7,9] 物40 pri0 / 月夜琳琅 36991 [15,25,30] 特10 pri0；
//   混链 5 行（影随暗香/弹雨狂袭/寒·雪域魂歌/深水黑裂击/绝境孤袭——1593 切片后仍
//   [lo,hi,cap] 三参形，混链兄弟效果未实现的部分按哑链处理）。
//
// 动工前置三查（2026-10-24 本 worktree 复核）：
//   ①内核 grep：combo_arg_indices 已含 case 1593（只取参数、不注册行为）——插件侧
//   SKILL_EFFECT(1593 无注册；②effect_meta 无 Penetration/特判；③moves_lib 并集无
//   SKILL_EFFECT(1593——真缺口（1500 同族已落地，形制一致）。
// 先例：1500（batch_b_skills.cpp:1530：combo_view 绝对写 + 变威力重算）、
//   118（j_power_view3：rand 绝对写范式）。
// 场景回归：test/scenario/scenario_7040_hp_combo_cap.cpp（7040 号段）。

#include "common.h"

namespace {

// ── 1593 —— 1回合做{0}-{1}次攻击，自身体力低于对手时连击上限为{2} ──
// int_args = [owner, target, lo, hi, cap]（args_num=3；前两位为运行时拼的 own/opp）。
EffectResult effect_1593_hp_combo_cap(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1593, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1593, args,
                     "1593 布局=[owner,target,lo,hi,cap]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int lo = args.int_args[2];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1593, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(lo > 0 && cap >= lo, 1593, args, "需要 lo ≥ 1 且 cap ≥ lo");
    if (ctx->getPet(owner).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // 体力不低于对手：内核掷的 {0}~{1} 照常
    }
    // 绝境档：上限改为 cap → 有效区间 [lo,cap] 均匀重掷（消耗一次 rand，118 同款）
    const int span = cap - lo + 1;
    ctx->ws.combo_view[owner] = lo + sbs_rand() % span;   // 写=声明变威力（触发重算）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1593, effect_1593_hp_combo_cap)
