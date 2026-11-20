// J 组 J154 · 每回合变化族 —— 643×11（2026-10-04，worktree ../agents/j154）
//
// 家族语义（effect_info 模板即权威）：
//   643（args_num=4）"{0}%概率使对手{1}回合内{2}能力每回合变化{3}"
//     —— 飞虎断魂斩 16498 [100,3,1,-1]（物160，100% 确定档：3回合 防御每回合-1）。
//     {2} 槽位码 = J19 759 同表（1防/2特防/3双防/4速/5命中——斗神岩斩破 [55,2,1,-1]
//     =防御 每回合-1 互证）；3=双防（清两槽）。
//
// 实现口径（工单行裁定落地）：
//   - 概率 = roll_percent({0})（J14 850 掷点口径；100% 确定档场景断言）。
//   - 变化窗 = 首跳切分（J14 1024 范式）：施放当回合即首跳、2..{1} 走 NextRounds({1}-1)
//     ROUND_START tick。每跳：delta<0 → stat_drop（对手施加查免弱）；delta>0 →
//     stat_change（原始变更）——分向记档（全库数据恒负，正 delta 分支为数据防御）。
//   - 引擎槽映射：1→2（防）、2→3（特防）、3→{2,3}（双防双槽）、4→4（速）、5→5（命中）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04，11/11
// [chance,回合,槽位码,delta] 自洽零剩余）。
// 动工前置三查（2026-10-04）：①内核 grep 643 无硬编码 ②effect_meta 无分类牵连
// ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2560_roundly_change.cpp（号段 2560-2569）。

#include "common.h"

namespace {

// 槽位码 → 引擎槽列表（759 同表；3=双防展开两槽）。
bool j154_engine_slots(int code, int* out, int* count) {
    switch (code) {
        case 0: out[0] = 0; *count = 1; return true;   // 攻击（35372 残壁石崩实例，fuzz 070519 批次实测补全）
        case 1: out[0] = 2; *count = 1; return true;
        case 2: out[0] = 3; *count = 1; return true;
        case 3: out[0] = 2; out[1] = 3; *count = 2; return true;
        case 4: out[0] = 4; *count = 1; return true;
        case 5: out[0] = 5; *count = 1; return true;
        default: return false;
    }
}

// 一跳：对槽列表施加 delta（负=stat_drop 查免弱；正=stat_change 原始变更）。
void j154_apply_once(BattleContext* ctx, int target, int slot_code, int delta, int owner) {
    if (!g_core) {
        return;
    }
    int slots[2] = {0, 0};
    int count = 0;
    if (!j154_engine_slots(slot_code, slots, &count)) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (delta < 0) {
            if (g_core->stat_drop) {
                (void)g_core->stat_drop(ctx, target, slots[i], -delta, owner);
            }
        } else if (delta > 0) {
            if (g_core->stat_change) {
                (void)g_core->stat_change(ctx, target, slots[i], delta);
            }
        }
    }
}

// ── 643 的 ROUND_START tick —— payload={owner, target, chance?, rounds 恒 0 不用,
// slot_code, delta}；tick 不再掷骰（概率只在施放时判一次，「每回合变化」是窗内必然）。
EffectResult j154_roundly_change_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slot_code = args.int_args[3];
    const int delta = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target) {
        return EffectResult::kOk;
    }
    j154_apply_once(ctx, target, slot_code, delta, owner);
    return EffectResult::kOk;
}

// ── 643 —— {0}%概率使对手{1}回合内{2}能力每回合变化{3} ──
// int_args = [owner, target, chance, rounds, slot_code, delta]（args_num=4）。
EffectResult effect_643_roundly_change(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 643, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 643, args,
                     "643 布局=[owner,target,chance,rounds,槽位码,delta]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int rounds = args.int_args[3];
    const int slot_code = args.int_args[4];
    const int delta = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     643, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && rounds > 0 && delta != 0, 643, args,
                     "需要 chance/rounds>0 delta≠0");
    int slots[2] = {0, 0};
    int count = 0;
    EFFECT_ARG_GUARD(j154_engine_slots(slot_code, slots, &count), 643, args,
                     "643 {2} 槽位码非法（0-5）");
    if (!roll_percent(chance)) {
        return EffectResult::kOk;   // 申报掷点（100% 确定档恒过）
    }
    // 首跳（施放当回合）
    j154_apply_once(ctx, target, slot_code, delta, owner);
    // 后续 2..{1} 跳
    if (rounds > 1) {
        (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 643,
                                          rounds - 1,
                                          std::vector<int>{owner, target, 0, slot_code, delta},
                                          &j154_roundly_change_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(643, effect_643_roundly_change)
