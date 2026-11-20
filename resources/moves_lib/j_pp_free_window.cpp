// J184 PP 自由窗族 —— effect 2232（1 宏 10 载体）
//
// 行清单（DB moves.side_effect 含 2232 的全载体，2026-10-07）：
//   官方模板「下{0}回合使用技能不受PP值限制」，arg_count=1=args_num；
//   全载体切片 = arg 数组末位（2232 均居 se 末位，arg_count(2232)=1）：
//     28867 欑壑孽海[2] / 28957 不朽荣光[1] / 28958 崩坏圣堂[1] / 29017 荒宇神悼[3] /
//     29074 岩崖崩坍[1] / 29263 闭目灵视[2] / 37762 无止星途[1] / 37764 渐近审判号[1] /
//     37787 登神日讴歌[1] / 38398 玄冰崩陨[3] —— {0}∈{1,2,3} 自洽。
//
// 落法（731 直系，batch_a_skills.cpp）：SKILL_EFFECT 时点读行参 N →
//   register_next_rounds_effect(ROUND_START 桶) → tick 比回合写
//   ctx->ignore_pp[owner]（skills.cpp usable/query_selectable 消费；2492
//   魂印同字段）。⚠️ ignore_pp **没有每回合重置**（只在换宠/clearAllEffects 清，
//   实测 R4 残留）→ 照 2492"每回合无脑重写"形：窗长注册 N+1（覆盖过期首回合），
//   tick 搔带 until，roundCount<=until 写 true、否则写 false——窗内每回合置位、
//   过期首回合显式清除，此后 tick 不再跑且无人再置位。
// 「下{0}回合」口径 = NextRounds（不含当回合，[R+1,R+N]；round_effect_start_round
//   注释官方通用规则：当回合 PP 已正常消耗，"下"字明确指向后续回合）。
//
// 三查记录（2026-10-07）：①内核无 2232 硬编码 ②effect_info 2232 args_num=1 与
// side_effect.arg_count=1 双表一致 ③registry moves 无 2232、moves_lib 无同 id 注册。
// 场景回归：test/scenario/scenario_3650_j184_effect2232.cpp（号段 3650-3659）。

#include "common.h"

namespace {

// 比回合 ROUND_START tick：窗内写 true，过期首回合写 false（见头注残留坑）
EffectResult effect_2232_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int until = args.int_args[1];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ignore_pp[owner] = (ctx->roundCount <= until);
    return EffectResult::kOk;
}

// SKILL_EFFECT 时点：落 NextRounds 窱（"下{0}回合"，本回合不算）
EffectResult effect_2232_pp_free_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 官方生效窗 [R+1, R+N]；tick 窨多注册一回合（R+N+1）用于过期首回合显式清 false
    const int start = ctx->round_effect_start_round(owner, rounds,
                                                    EffectWindowKind::NextRounds);
    const int until = start + rounds - 1;
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 2232,
                                /*rounds=*/rounds + 1, {owner, until},
                                &effect_2232_tick);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）────────────────────────────────────────────────────
SKILL_EFFECT(2232, effect_2232_pp_free_window);
