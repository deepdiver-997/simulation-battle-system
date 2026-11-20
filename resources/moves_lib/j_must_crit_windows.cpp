// J 组 J55 · 必暴窗族 —— 160×17 / 702×17 / 652×17（2026-10-03，worktree ../agents/j55）
//
// 家族语义（effect_info 官方模板即权威），三条都以"必定致命一击"为轴：
//   160（arg_count=1）"{0}回合内若对手MISS则下回合自身必定致命一击"
//   652（arg_count=1）"消除对手能力上升状态，消除成功下{0}回合必定暴击"
//   702（arg_count=1）"下{0}回合若打出致命一击则必定恢复全部体力"
//
// 实现口径：
//   - 160 = 1320 MISS watcher 直系（EVENT_SKILL_INVALID、emit {actor=使用方}、
//     "攻击MISS"字面取命中失败路径、属性技无效不算——1320 同款过滤）置当回合
//     武装旗（plugin_storage 键 kArmBase+roundCount）；ROUND_START tick 窗
//     rounds+1（覆盖"末回合 MISS→下回合"的跨窗武装）读上一回合旗 →
//     ws.must_crit（681/58 必暴通道，1204 J18 直系）。
//   - 652 = clear_stat_boosts(opp) 返回 >0 门槛（J2 538/1170 直系，STAT_CLEAR
//     免消除票挡下即门槛不成立）+ NextRounds({0}) ROUND_START tick 写 must_crit
//     （1204 同通道；"下{0}回合"每回合都必暴）。
//   - 702 = NextRounds({0}) 双态 AFTER_ACTION 探针（J18 1483 同构、duration 按
//     窗注册）：行动方==自身 且 crit_happened[自身]（query_usage 掷、每次技能
//     使用先清——行动结束时点真值即"本次攻击打出过致命一击"）→ heal_amount
//     回满（155 heal_amount(max_hp) 直系，吃封回血；满血为无害 no-op）。
//     "下{0}回合"= NextRounds 本回合不算：当回合暴击（含载体自身那刀）不回。
//
// 载体切片（unity 库 json_each 精确匹配 17×3=51；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：全族 [rounds] 单参——
//   160 {3}×10/{5}×3/{2}×2/{4}×2（纯链载体 圣甲幻瞳术 16183 [2]）
//   652 {2}×14/{1}×2/{3}×1（纯链载体 EX咖喱棒 16172 [2]）
//   702 {2}×15/{3}×2（纯链载体 抛掷蓄力 28040 [2]，属性技）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 160/652/702 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1320（MISS watcher+旗，J42）、1204（ROUND_START tick 写 must_crit，J18）、
//   1483（双态 AFTER_ACTION 探针+crit_happened 读，J18）、538/1170（消强门槛，J2）、
//   155（heal_amount 回满，batch_b）。
// 场景回归：test/scenario/scenario_1410_must_crit_windows.cpp（1410 号段，一文件三宏）。

#include "common.h"

namespace {

constexpr int k160ArmBase = 990160000;   // +roundCount：该回合对手 MISS → 下回合必暴武装

// AFTER_ACTION 桶的当前行动方（897/J12/J18 同款小工具，插件各自持有一份）。
int j55_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 160 的 ROUND_START tick —— 上一回合对手 MISS 过 → 本回合必暴 ──
EffectResult j55_160_round_start_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int key = k160ArmBase + ctx->roundCount - 1;
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) != 1) {
        return EffectResult::kOk;   // 上一回合对手没 MISS
    }
    ctx->plugin_storage.erase(it);
    ctx->ws.must_crit[self] = true;
    return EffectResult::kOk;
}

// ── 652/1204 同款 ROUND_START tick —— 窗内每回合必暴 ──
EffectResult j55_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[self] = true;
    return EffectResult::kOk;
}

// ── 702 的双态 AFTER_ACTION 探针 —— 本次攻击暴击 → 回满 ──
EffectResult j55_702_after_action_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (j55_bucket_acting_side(ctx) != self || !ctx->crit_happened[self]) {
        return EffectResult::kOk;   // 只在自己出手流程结束判；没打出致命一击不回
    }
    (void)g_core->heal_amount(ctx, self, max_hp_of(ctx, self));
    return EffectResult::kOk;
}

// ── 160 —— {0}回合内若对手MISS则下回合自身必定致命一击 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_160_miss_next_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 160, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 160, args,
                     "160 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     160, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 160, args, "需要 回合 > 0");
    // miss 武装：对手侧使用技能失败（emit {actor=使用方}）；攻击技过滤=对手当回合
    // 所选槽位的技能非属性技（1320 同款）。动作只置旗——事件派发中不注册效果。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;
            }
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;   // "攻击MISS"：属性技的无效不算
            }
            c->plugin_storage[k160ArmBase + c->roundCount] = 1;
        },
        WatcherScope::TEAM);
    // 消费 tick：窗 rounds+1 —— 末个观察回合的武装在下回合（观察窗外一回合）
    // 的 ROUND_START 也要吃到。
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 160, rounds + 1,
                          {owner}, &j55_160_round_start_tick);
    return EffectResult::kOk;
}

// ── 652 —— 消除对手能力上升状态，消除成功下{0}回合必定暴击 ──
EffectResult effect_652_purge_then_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 652, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 652, args,
                     "652 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     652, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 652, args, "需要 回合 > 0");
    if (!g_core || !g_core->clear_stat_boosts
        || g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛（STAT_CLEAR 免票挡下=失败）
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 652, rounds,
                                {owner}, &j55_must_crit_tick);
    return EffectResult::kOk;
}

// ── 702 —— 下{0}回合若打出致命一击则必定恢复全部体力 ──
EffectResult effect_702_crit_heal_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 702, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 702, args,
                     "702 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 702, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 702, args, "需要 回合 > 0");
    // 双态 AFTER_ACTION 探针（1483 同构，duration=窗长）：窗内每次自己行动结束
    // 都判 crit_happened——多回合多暴击各回各的（"若打出…则恢复"逐次成立）。
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, self, 702, rounds,
                                {self}, &j55_702_after_action_probe);
    register_next_rounds_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, self, 702, rounds,
                                {self}, &j55_702_after_action_probe);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(160, effect_160_miss_next_crit)
SKILL_EFFECT(652, effect_652_purge_then_crit)
SKILL_EFFECT(702, effect_702_crit_heal_full)
