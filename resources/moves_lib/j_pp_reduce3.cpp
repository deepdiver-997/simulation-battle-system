#include <utils/sbs_rand.h>
// J 组 J252 · PP 削减族三 —— 3 宏/24 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   2033（arg_count=1）"未击败对手则降低对手所有技能{0}点PP值"
//   2081（arg_count=1）"对手不处于异常状态时令对手随机{0}个技能PP值归零"
//   1621（arg_count=2）"{0}%令对手所有技能PP值-{1}，自身满体力时效果翻倍"
//
// 实现口径（先例：j_notdefeat_pink.cpp 1285 / j_chance_drop_pp_zero.cpp 1519）：
//   - 2033 = 未击败门（行动末对手 hp>0，J50 644/579 同门）→ pp_reduce 全槽 {0}；
//     AFTER_ACTION 双侧注册、桶只跑行动方（52 教训），当回合即结算。
//   - 2081 = 门：对手无任一活跃异常（kOfficialAbnormalStatusMaxId 遍历）→ 从
//     **pp>0 槽**中 Fisher-Yates 随机不重复抽 {0} 个 → pp_zero_slot 原语清零
//     （PP 操作走原语，发 EVENT_PP_REDUCED；不直写 pp）。
//   - 1621 = 满体判 hp==max_hp → 有效概率 min(100,{0}×2)、PP 点数 {1}×2；否则
//     {0}% / {1} 点。pct 在位即消费（红线 7：显式 0 关闸档不回退）。
//   - 审计槽（场景精确断门，9xxxx 私有段）：
//       202033100 + owner   = 1 未击败门过（已削）/ -1 门闭（击败）
//       202081100 + owner   = -1 门闭（对手有异常）/ 1000 + 实际归零槽数
//       201621100 + owner*2 = hit*1000 + 有效概率；+1 = 实际使用的 PP 点数
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算，24/24 自洽）：
//   2033 [点数=2]：37754 折钢棱锋 [999,558,2033]（999/558 不碰 PP）、36689 极涛裂空杀、
//     36872 华灵绯天咒、37856 风砂暴击、37896 飞龙雷弧爪、38110 净罪璃火、
//     38131 绝不意气用事、38371 寂·妖胧天鸣。
//   2081 [槽数]：37627 虚实温度 [188,2081,190] [1]、36880 倒吊兰 [1]、37247 煜熠 [1]、
//     36994 虚昼幻空斥 [2]、38205 草绿唤春来 [2]、37634/38222/38351 [1]。
//   1621 [概率,点数]：36833 涛光灿灿（纯链）[100,1]、37898 破碎撕咬 [100,1]、
//     28287 天饶之国 [100,2]、34590 洪荒穿刺 [100,1]、34655/37397/37531/38092 [100,1]。
//
// 动工前置三查（2026-10-22）：①内核 grep 2033/2081/1621 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 无行 ③全库 moves_lib/soul_lib 并集（含各 worktree）
//   无 SKILL_EFFECT(2033/2081/1621) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5830_pp_reduce3.cpp（号段 5830-5839）。

#include "common.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace {

constexpr int k2033AuditBase = 202033100;   // +owner
constexpr int k2081AuditBase = 202081100;   // +owner
constexpr int k1621AuditBase = 201621100;   // +owner*2

// 未击败门：行动末对手仍存活（J50 934 存活镜像 / j58 同款）。
bool j252_not_defeated(BattleContext* ctx, int target) {
    return ctx && target >= 0 && target <= 1 && ctx->getPet(target).hp > 0;
}

// 对手有任一活跃异常（"不处于异常状态"门的取反；J52 768 同款遍历口径）。
bool j252_foe_statused(BattleContext* ctx, int target) {
    if (!ctx || !g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            return true;
        }
    }
    return false;
}

// 从目标方 **pp>0** 槽中随机不重复抽 want 个，走 pp_zero_slot 清零；返回实际归零槽数。
// 直接复用 j_chance_drop_pp_zero.cpp 的既有口径（Fisher-Yates 局部洗牌，656/931 同款）。
int j252_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
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

// AFTER_ACTION 双侧单发注册（InRounds rounds=1 含当回合；桶只跑行动方，52 教训）。
void j252_register_after_action(BattleContext* ctx, int owner, int effect_id,
                                std::vector<int> payload, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, effect_id, /*rounds=*/1, payload, fn);
    }
}

// ── 2033 的 AFTER_ACTION tick —— payload={owner,target,points}；未击败 → 对手全槽 PP-{0}。
EffectResult j252_2033_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int points = args.int_args[2];
    if (!j252_not_defeated(ctx, target)) {
        ctx->plugin_storage[k2033AuditBase + owner] = -1;   // 门闭：当回合击败了对手
        return EffectResult::kOk;
    }
    if (points > 0 && g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, target, points);
        ctx->plugin_storage[k2033AuditBase + owner] = 1;
    }
    return EffectResult::kOk;
}

// ── 2033 —— 未击败对手则降低对手所有技能{0}点PP值 ──
// int_args = [owner, target, points]（args_num=1）。
EffectResult effect_2033_not_defeat_pp_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2033, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2033, args,
                     "2033 布局=[owner,target,点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2033, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 2033, args, "需要 点数 > 0");
    j252_register_after_action(ctx, owner, 2033, {owner, target, points}, &j252_2033_tick);
    return EffectResult::kOk;
}

// ── 2081 —— 对手不处于异常状态时令对手随机{0}个技能PP值归零 ──
// int_args = [owner, target, slots]（args_num=1）。
EffectResult effect_2081_no_anomaly_random_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2081, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2081, args,
                     "2081 布局=[owner,target,槽数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slots = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2081, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(slots > 0, 2081, args, "需要 槽数 > 0");
    if (j252_foe_statused(ctx, target)) {
        ctx->plugin_storage[k2081AuditBase + owner] = -1;   // 门闭：对手处于异常状态
        return EffectResult::kOk;
    }
    const int zeroed = j252_zero_random_pp_slots(ctx, target, slots, owner);
    ctx->plugin_storage[k2081AuditBase + owner] = 1000 + zeroed;
    return EffectResult::kOk;
}

// ── 1621 —— {0}%令对手所有技能PP值-{1}，自身满体力时效果翻倍 ──
// int_args = [owner, target, pct, points]（args_num=2）。
EffectResult effect_1621_chance_pp_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1621, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1621, args,
                     "1621 布局=[owner,target,概率,点数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1621, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 1621, args, "需要 点数 > 0");

    const bool full_hp = ctx->getPet(owner).hp >= max_hp_of(ctx, owner);
    const int effective_pct = full_hp ? std::min(100, pct * 2) : pct;   // pct 在位即消费
    const int effective_points = full_hp ? points * 2 : points;
    const bool hit = roll_percent(effective_pct);
    ctx->plugin_storage[k1621AuditBase + owner * 2] = (hit ? 1000 : 0) + effective_pct;
    ctx->plugin_storage[k1621AuditBase + owner * 2 + 1] = effective_points;
    if (hit && g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, target, effective_points);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2033, effect_2033_not_defeat_pp_reduce)
SKILL_EFFECT(2081, effect_2081_no_anomaly_random_zero)
SKILL_EFFECT(1621, effect_1621_chance_pp_reduce)
