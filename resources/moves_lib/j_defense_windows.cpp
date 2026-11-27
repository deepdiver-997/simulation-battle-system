// J 组 J62 · 防御窗族 —— 1023×17 / 914×18（2026-10-03，worktree ../agents/j62）
//
// 家族语义（effect_info 官方模板即权威）：
//   1023（arg_count=3）"{0}回合内{1}%闪避对手攻击，若对手MISS则恢复自身最大体力的1/{2}"
//   914（arg_count=3）"若对手处于{0}状态则{1}回合内{2}%令对手使用的技能无效"
//
// 实现口径：
//   - 1023 = ws.dodge_rate 窗（568 直系：当回合立即写 + ROUND_START tick 逐回合重写，
//     ws 每回合 memset；dodge 在 skills.cpp 命中 roll 消费 = hit_chance 减 dodge×100）
//     + MISS watcher（160/1320 同款：EVENT_SKILL_INVALID、ev.actor==对手、属性技的
//     无效不算——"对手攻击"字面过滤）→ heal_max_hp_fraction(1/{2})，逐次 MISS 逐次回。
//   - 914 = 状态门（has_active_abnormal_status({0})）+ roll_percent({2}) 一次掷 →
//     grant_skill_invalid SEAL_ALL {1} 回合（龙威形：counts=0 窗口票、penetrable=false）。
//     ⚠️ 口径记档：{2}% 按"授予时一次掷"实现（SEAL 票无逐次使用掷点，票面授予是唯一
//     既有通道；"使用的技能无效"= 窗内对手用啥都无效，SEAL_ALL 语义即此）。
//
// 载体切片（unity 库 json_each 精确匹配 17/18=35；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1023 [回合,闪避%,分母]：{3,70,3}×6/{3,50,3}×3/{3,80,3}×3/{3,100,3}×3/
//     {2,100,2}/{3,40,3}（纯链载体 无息之隐 28344 [3,100,3] 属——100% 档确定性）
//   914 [状态,回合,概率]：{2,2,50}×5/{2,1,50}×3/{2,2,100}×3/…（场景载体 灼光绮丽
//     28421 [2,2,100] 属——100% 档；链上 191/886 已实现且无害、1393 未实现哑）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1023/914 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：568/1069（ws.dodge_rate 闪避窗，J28）、160/1320（EVENT_SKILL_INVALID miss
// watcher，J55/J42）、155/heal_max_hp_fraction（回 max/denom）、1110（grant_skill_invalid）。
// 场景回归：test/scenario/scenario_1480_defense_windows.cpp（1480 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 1023 的 ROUND_START tick —— 窗内每回合重写闪避率（ws memset，568 同款）──
EffectResult j62_1023_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int dodge_pct = args.int_args[1];
    if (self < 0 || self > 1 || dodge_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = dodge_pct / 100.0f;
    return EffectResult::kOk;
}

// ── 1023 —— {0}回合内{1}%闪避对手攻击，若对手MISS则恢复自身最大体力的1/{2} ──
// int_args = [owner, target, rounds, dodge_pct, heal_denom]（args_num=3）。
EffectResult effect_1023_dodge_heal_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1023, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1023, args,
                     "1023 布局=[owner,target,回合,闪避%,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int dodge_pct = args.int_args[3];
    const int heal_denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1023, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && dodge_pct > 0 && heal_denom > 0, 1023, args,
                     "需要 回合/闪避%/分母 > 0");
    // 闪避窗：当回合立即生效（426 形）+ ROUND_START 逐回合重写（ws memset）。
    ctx->ws.dodge_rate[self] = dodge_pct / 100.0f;
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1023, rounds,
                          {self, dodge_pct}, &j62_1023_dodge_tick);
    // MISS→回血 watcher：对手攻击技命中失败（闪避/落空都走 miss 出口）→ 回 max/1/{2}。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, heal_denom](BattleContext* c, const BattleEvent& ev) {
            fprintf(stderr, "[PROBE-W] fired actor=%d opp=%d\n", ev.actor, opp);
            if (!c || ev.actor != opp || !g_core || !g_core->heal_amount) {
                return;
            }
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;   // "对手攻击"MISS：属性技的无效不算
            }
            (void)heal_max_hp_fraction(c, self, heal_denom);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 914 —— 若对手处于{0}状态则{1}回合内{2}%令对手使用的技能无效 ──
// int_args = [owner, target, status, rounds, pct]（args_num=3）。
EffectResult effect_914_status_seal_all(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 914, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 914, args,
                     "914 布局=[owner,target,状态码,回合,概率]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int status = args.int_args[2];
    const int rounds = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     914, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 914, args, "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(status), 914, args,
                     "需要 状态码 ∈ [0,43]");
    // 异常槽查询走 CoreApi（has_active_abnormal_status 非 inline，插件不可直调——击杀线教训 #4）。
    if (!g_core || !g_core->has_active_anomaly
        || !g_core->has_active_anomaly(ctx, opp, status)) {
        return EffectResult::kOk;   // 状态门：对手不处于{0}状态
    }
    if (!roll_percent(ctx->rng(), pct)) {
        return EffectResult::kOk;   // {2}% 一次掷（授予时；口径记档见文件头）
    }
    ctx->grant_skill_invalid(owner, opp, /*source_slot=*/-1, /*effect_id=*/914,
                             SealKind::SEAL_ALL, /*counts=*/0, /*rounds=*/rounds,
                             /*penetrable=*/false, EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1023, effect_1023_dodge_heal_window)
SKILL_EFFECT(914, effect_914_status_seal_all)
