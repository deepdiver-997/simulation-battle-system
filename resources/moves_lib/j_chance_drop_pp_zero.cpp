#include <utils/sbs_rand.h>
// J 组 J220 · 概率随机清 PP 族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1519 [4] {0}%令对手随机{1}项技能PP值归零，若对手处于能力下降状态则{2}%使对手
//            随机{3}项技能PP值归零
//
// 实现口径：
//   - SKILL_EFFECT 时点：两条分支各自独立申报——
//     ① 无条件 roll {0}% → 随机不重复 {1} 个 pp>0 槽（pp_zero_slot 原语清零，
//        发 EVENT_PP_REDUCED，不直写 pp）。
//     ② 门 = 对手六槽（含命中）任一负等级（J63/J71/J182 同判据）→ roll {2}% →
//        从**剩余 pp>0 槽**再抽 {3} 个（已归零槽不再空转，等价"额外"语义）。
//   - pct 在位即消费：roll_percent(p) 直调（p<=0 天然 false、p>=100 短路不耗 rand），
//     不用 >0 守卫（红线 7）。
//   - 审计槽 plugin_storage[990151900 + owner*2]     = 分支① hit*1000 + 实际清零槽数；
//     plugin_storage[990151900 + owner*2 + 1]       = 分支② 同上；门闭记 -1。
//     +self*2 避免跨侧相邻键撞键（J207/J209 勘误口径）。
//
// 载体切片（side_effect JSON 按 effect_info.args_num 位切，2026-10-22 脚本核算，9/9 自洽）：
//   28447 [191,938,1519] → 1519=[100,2,100,3]（其余 8 载体 1519=[100,1,100,2]）
//   33807 剑破红尘 / 33880 赤诚红中 / 34101 灵鹿仙踪逝 / 34334 无影袭 /
//   34920 亡魂绝命刺 / 35197 凝霜覆岭诀 / 35416 森芜荒生印 / 36363 魂沉
//   场景取 34334（链兄弟 1350「反转自身能力下降」——自身无降态时空跑）与
//   28447（链兄弟 191 免异常/938 全属性-{0}，938 天然把门打开，用于双分支参数档）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1519 无硬编码 ②effect_info 仅本条模板，
//   无同文孪生 ③moves_lib/kbatch/j186/j198 worktree 并集无 1519 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5490_chance_drop_pp_zero.cpp（号段 5490-5499）。

#include "common.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

constexpr int k1519AuditBase = 990151900;   // + owner*2：分支①；+1：分支②

// 对手处于能力下降状态 = 六槽（含命中）任一负等级（J63/J71/J182 同判据）。
bool j220_foe_has_drop(const BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// 从目标方**仍有余量（pp>0）**的技能槽中随机不重复抽 want 个，走 pp_zero_slot 清零。
// 返回实际归零的槽数（pp 原值 > 0 且清零成功）。want>=槽数 → 全清。
int j220_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return 0;
    }
    ElfPet& pet = ctx->getPet(target);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(pet.skills.size()); ++i) {
        if (pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    const int n = std::min(want, static_cast<int>(candidates.size()));
    int zeroed = 0;
    for (int i = 0; i < n; ++i) {
        // Fisher-Yates 局部洗牌：每次从 [i, size) 抽一个换到 i（656/931 同款）。
        const int j = i + (sbs_rand() % (static_cast<int>(candidates.size()) - i));
        std::swap(candidates[static_cast<std::size_t>(i)],
                  candidates[static_cast<std::size_t>(j)]);
        const int slot = candidates[static_cast<std::size_t>(i)];
        const int before = pet.skills[static_cast<std::size_t>(slot)].pp;
        (void)g_core->pp_zero_slot(ctx, target, slot, actor);
        if (before > 0 && pet.skills[static_cast<std::size_t>(slot)].pp == 0) {
            ++zeroed;
        }
    }
    return zeroed;
}

// ── 1519 —— {0}%随机{1}项PP归零；对手有降态则额外 {2}%随机{3}项 ──
// int_args = [owner, target, pct1, slots1, pct2, slots2]（args_num=4）。
EffectResult effect_1519_double_branch_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1519, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1519, args,
                     "1519 布局=[owner,target,pct1,slots1,pct2,slots2]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct1 = args.int_args[2];
    const int slots1 = args.int_args[3];
    const int pct2 = args.int_args[4];
    const int slots2 = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1519, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }

    // 分支①：无条件概率门。pct 在位即消费（0 档显式关闸，不回退）。
    const bool hit1 = roll_percent(pct1);
    const int zeroed1 = hit1 ? j220_zero_random_pp_slots(ctx, target, slots1, owner) : 0;
    ctx->plugin_storage[k1519AuditBase + owner * 2] = (hit1 ? 1000 : 0) + zeroed1;

    // 分支②：对手处于能力下降状态才申报；同样在位即消费。
    const bool gate = j220_foe_has_drop(ctx, target);
    int audit2 = -1;
    if (gate) {
        const bool hit2 = roll_percent(pct2);
        const int zeroed2 = hit2 ? j220_zero_random_pp_slots(ctx, target, slots2, owner) : 0;
        audit2 = (hit2 ? 1000 : 0) + zeroed2;
    }
    ctx->plugin_storage[k1519AuditBase + owner * 2 + 1] = audit2;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1519, effect_1519_double_branch_pp_zero)
