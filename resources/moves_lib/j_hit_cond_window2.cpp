// J 组 J132 · 命中后窗族 —— 1028×13（2026-10-04，worktree ../agents/j132）
//
// 家族语义（effect_info 模板即权威）：
//   1028（args_num=8）"命中后{0}回合内每回合使对手{1}，若未触发则减少对手最大体力的1/{7}"
//     —— 切片 [rounds, 语义序六槽×6, denom]：28739 星光·压迫 [3,-2,0,-2,0,-2,-2,3]、
//        19909 粉碎敲头棰 [3,0,-2,0,0,-2,0,3]（纯链）。{1}=语义序六槽负值（0=跳槽），
//        语义序（攻,防,特攻,特防,速,命中）→ 引擎槽 {0→0,1→2,2→1,3→3,4→4,5→5}
//        （H2 158/J121 473/J125 407 同映射，星光·压迫 4399 逐字互证）。
//
// 实现口径（工单行裁定落地）：
//   - 命中门 = ws.skill_exec_result[owner]==HIT（J23 36/J85 1195 同判据；SKILL_EFFECT
//     桶 miss 也会到，显式门拦截）。
//   - 首跳切分（J14 1024 范式）：命中当回合即首跳（施放时点直接逐槽 stat_drop），
//     2..{0} 回合走 NextRounds({0}-1) ROUND_START tick（窗口 [R+1, R+{0}-1] 恰后续
//     {0}-1 跳；{0}=1 不注册 tick）。"每回合使对手{1}"=每跳对全部非零槽 stat_drop
//     （免弱通道，actor=owner）。
//   - "未触发"= 本跳**零槽 SUCCESS**（stat_drop 返回 IMMUNE/AT_FLOOR 档——免降票/
//     已在 -6 地板）→ reduce_max_hp_pct(对手, on_stage[slot], 100/{7}, floor=1)
//     （1102 直系 d5_D3 同款：消逝 floor 守不变量）。
//   - denom 换算：pct=100/{7}（整数除；{7}=3 → 33%，工单行 ceil 口径按 1102 现行
//     整数直除落地，差异记档）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04，13/13 自洽）：
//   [rounds,六槽,denom]：28739 星光·压迫（纯链）、19909 粉碎敲头棰（纯链）、
//   32189、37785 纯链×4 场景利；链上兄弟（如 48/570/616）已实现良性或未实现静默。
//
// 动工前置三查（2026-10-04）：①内核 grep 1028 无硬编码 ②effect_meta 无分类牵连
// ③moves_lib 无既有注册——真缺口（stat_drop/reduce_max_hp_pct 均既有设施）。
// 场景回归：test/scenario/scenario_2270_hit_cond_window.cpp（号段 2270-2279）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎槽（0攻1特攻2防3特防4速5命中）。
constexpr int kSemanticToEngine[6] = {0, 2, 1, 3, 4, 5};

// 一跳：对全部非零槽 stat_drop（免弱通道），返回 SUCCESS 槽位数。
int j132_drop_once(BattleContext* ctx, int owner, int target, const int* vals) {
    if (!g_core || !g_core->stat_drop) {
        return 0;
    }
    int landed = 0;
    for (int i = 0; i < 6; ++i) {
        if (vals[i] >= 0) {
            continue;   // 0=跳槽；正值为畸形数据（模板只出负值），按跳过处理
        }
        if (g_core->stat_drop(ctx, target, kSemanticToEngine[i], -vals[i], owner) ==
            StatDropResult::SUCCESS) {
            ++landed;
        }
    }
    return landed;
}

// 未触发兜底：减对手最大体力 1/{denom}%（1102 直系：slot=on_stage，floor=1）。
void j132_fallback_max_hp(BattleContext* ctx, int target, int denom) {
    if (!g_core || !g_core->reduce_max_hp_pct) {
        return;
    }
    (void)g_core->reduce_max_hp_pct(ctx, target, ctx->on_stage[target], 100 / denom,
                                    /*floor=*/1);
}

// ── 1028 的 ROUND_START tick —— payload={owner, target, v1..v6, denom}。
EffectResult j132_hit_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 9) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[8];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target || denom <= 0) {
        return EffectResult::kOk;
    }
    if (j132_drop_once(ctx, owner, target, args.int_args + 2) == 0) {
        j132_fallback_max_hp(ctx, target, denom);
    }
    return EffectResult::kOk;
}

// ── 1028 —— 命中后{0}回合内每回合使对手{1}，若未触发则减少对手最大体力的1/{7} ──
// int_args = [owner, target, rounds, v1..v6, denom]（args_num=8）。
EffectResult effect_1028_hit_cond_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1028, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 10, 1028, args,
                     "1028 布局=[owner,target,rounds,语义序六槽,denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[9];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1028, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 1028, args, "需要 回合/分母 > 0");
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 命中门（J23 36 同判据）
    }
    // 首跳（命中当回合）
    if (j132_drop_once(ctx, owner, target, args.int_args + 3) == 0) {
        j132_fallback_max_hp(ctx, target, denom);
    }
    // 后续 2..{0} 跳
    if (rounds > 1) {
        std::vector<int> payload{owner, target};
        for (int i = 0; i < 6; ++i) {
            payload.push_back(args.int_args[3 + i]);
        }
        payload.push_back(denom);
        (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1028,
                                          rounds - 1, std::move(payload),
                                          &j132_hit_window_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1028, effect_1028_hit_cond_window)
