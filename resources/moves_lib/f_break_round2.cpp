// F1 · 消回合成功族二批 —— 950×1 / 789×1 / 1036×2（2026-09-29，worktree ../agents/f1）
//
// E2（e_break_round.cpp）的姊妹批：同一家族形状——break_round_effects 返回
// SUCCESS（真的清掉了东西）才执行后续子句；NO_EFFECTS/IMMUNE 都不算成功。
//
// 950（args_num=1）"消除对手回合类效果，消除成功则对手{0}回合内攻击技能MISS"：
//   - MISS = 命中率压 0 的近似，字面同款先例 = 721（artemis_skills.cpp"后出手时使
//     对手下{0}回合攻击技能MISS"，ws.hit_rate_mod=0）。已知偏差记档（721 同款）：
//     ① hit_rate_mod 同时作用于属性技（官方只说攻击技）；② 必中技能绕过命中判定
//     （内核口径，skills.cpp 命中链必中早退）。
//   - "{0}回合内"含当回合（470 双写口径）：消除当回合立即写一次 hit_rate_mod=0
//     （先手消除 → 对手当回合攻击就 MISS）+ register_round_effect（InRounds 窗口，
//     后出手顺延由 round_effect_start_round 官方折算）在后续每回合 ROUND_START 补写
//     （ws 每回合 reset → 每回合重写）。
//   - 载体：鬼影千重阵 19626 [950,101]|[2,50]、死亡弥撒 19865 [950,633]|[2,…]。
// 789（args_num=1）"消除对手回合类效果，消除成功对手下{0}回合受到的伤害翻倍"：
//   - 直系先例 = effect_1010（batch_b_skills.cpp"消强成功→下{0}回合造成的伤害提高
//     {1}%"）：NextRounds 窗口 + ROUND_START tick 写 ws.damage_add_pct。789 只换
//     门槛（break SUCCESS）与比例（args_num=1 无比例参数 → 翻倍 = +100 固定）。
//   - 方向："对手**受到的**翻倍" = 己方**造成的**增伤（damage_add_pct[owner]），
//     不搞反。
//   - 载体：光辉之羽 17619 [789]|[1]。
// 1036（args_num=2）"消除对手回合类效果，消除成功则下{0}次攻击附加{1}点固定伤害"：
//   - 计次型攻击附伤。计次计数放 plugin_storage（int，键段 990103600+owner——
//     "bool 撞 int 坑"纪律：本键段只存 int）；EVENT_HIT watcher（攻击命中事件）
//     消费 1 次 + g_core->fixed_damage。计次先例 = arm_immunity_reflect_true
//     （816 闭包计数）/ 1770（EVENT_HIT watcher + plugin_storage 门卫防重复注册）。
//   - "下{0}次"不含当次：watcher 注册于本次技能的 SKILL_EFFECT 桶，而本次攻击的
//     EVENT_HIT 已在 ON_SKILL_HIT 桶后 drain 完（mistike_skills.cpp 时点注释）→
//     天然不含当次，无需 1850 式 AFTER_ACTION 延后。
//   - "攻击"限定：EVENT_HIT 属性技也发（skills.cpp"属性技能也算命中"）→ 回调内
//     selected_skill_is_attack 复核。
//   - 已知偏差记档：① 计数不随换宠清除（内核 attack_boost_grants 换宠即清、
//     battleContext.h:1009；插件计数无换宠钩子——已知切片均 count=1，偏差极小）；
//     ② 固定伤害落点在本次红伤结算之前（EVENT_HIT drain 早于伤害管线；官方"附加"
//     语义在红伤之后）——总量一致，与 1770 同款时点家族；③ watcher 挂 9 回合自然
//     上限防泄漏（1770 同款；计次耗尽后回调空转）。
//   - 载体：违·永恒之寂 19940 [1036,897]|[1,300,…]、极品乐淘淘 30320 [1036]|[1,300]。
//
// 前置三查（2026-09-29）：①内核无 950/789/1036 硬编码（grep src/、include/ 无命中）
// ②effect_meta 无该三 id 声明 ③全库无既有实现（无 SKILL_EFFECT(950/789/1036)）。
//
// 场景回归：test/scenario/scenario_610_break_round2.cpp。

#include "common.h"

namespace {

// ── 950 窗口 tick —— 每回合开始把对手命中率压 0（ws 每回合 reset → 每回合重写）──
EffectResult effect_950_miss_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    if (opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[opp] = 0.0f;   // 对手本回合攻击必 MISS（721 同款近似）
    return EffectResult::kOk;
}

// ── 950 —— 消除对手回合类效果，消除成功则对手{0}回合内攻击技能MISS ──
EffectResult effect_950_break_then_miss_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 对手本来就没有回合类效果 → 子句不触发
    }
    // "{0}回合内"含当回合（470 双写口径）：当回合立即压 0（先手消除 → 对手当回合
    // 攻击即 MISS）；后续回合 ROUND_START tick 补写（InRounds 窗口，后出手顺延由
    // round_effect_start_round 折算——先手窗口 [R, R+{0})，tick 覆盖 R+1..）。
    ctx->ws.hit_rate_mod[opp] = 0.0f;
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 950, rounds,
                          {owner, opp}, &effect_950_miss_tick);
    return EffectResult::kOk;
}

// ── 789 窗口 tick —— ROUND_START 写"造成的伤害 +100%"（1010 同款增伤槽；
//    ws 每回合 reset → 窗口内每回合重写一次）──
EffectResult effect_789_damage_up_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += 100.0f;   // 翻倍 = +100%（args_num=1 无比例参数）
    return EffectResult::kOk;
}

// ── 789 —— 消除对手回合类效果，消除成功对手下{0}回合受到的伤害翻倍 ──
EffectResult effect_789_break_then_damage_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不翻倍
    }
    // "下{0}回合"= NextRounds（本回合不算）；"对手受到的翻倍"= 己方造成的增伤
    // （damage_add_pct[owner]，AMP 通用加算通道）——1010 直系同款窗口。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 789, rounds,
                                {owner, opp}, &effect_789_damage_up_tick);
    return EffectResult::kOk;
}

// ── 1036 —— 消除对手回合类效果，消除成功则下{0}次攻击附加{1}点固定伤害 ──
EffectResult effect_1036_break_then_attack_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->break_round_effects || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    const int fixed = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || count <= 0 || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不武装计数
    }
    // 计数放 plugin_storage（int 键段；同技能重复施放 = 叠加次数），watcher 只挂
    // 一次（1770 门卫同款：键存在即不重复注册，防一条命中消费多次）。
    constexpr int kCounterKeyBase = 990103600;
    const int key = kCounterKeyBase + owner;
    auto it = ctx->plugin_storage.find(key);
    if (it != ctx->plugin_storage.end()) {
        it->second = any_int_or(it->second) + count;   // 叠加，不重复挂 watcher
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, count);
    // EVENT_HIT = 攻击命中事件（miss 不发；属性技也发 → 回调内复核攻击限定）。
    // 跨回合窗口 → WatcherScope::TEAM（ON_STAGE 活不过回合交界）；9 回合上限防泄漏。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/9,
        /*once=*/false,
        [owner, opp, fixed, key](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (!executed_skill_is_attack(c, owner)) {
                return;   // "下{0}次攻击"——属性技能命中不消费
            }
            auto slot = c->plugin_storage.find(key);
            if (slot == c->plugin_storage.end()) {
                return;
            }
            int remaining = any_int_or(slot->second);
            if (remaining <= 0) {
                return;   // 次数耗尽：watcher 留到窗口上限自然过期，回调空转
            }
            slot->second = remaining - 1;
            (void)g_core->fixed_damage(c, opp, fixed);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(950, effect_950_break_then_miss_window)
SKILL_EFFECT(789, effect_789_break_then_damage_double)
SKILL_EFFECT(1036, effect_1036_break_then_attack_fixed)
