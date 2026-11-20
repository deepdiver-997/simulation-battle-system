// J 组 J108 · 技能无效反制/正先制观测族 —— 1794×14 / 557×15（2026-10-03，worktree ../agents/j108）
//
// 家族语义（effect_info 官方模板即权威）：
//   1794（arg_count=3）"技能无效时，恢复自身最大体力的1/2且下1次受到的攻击伤害额外
//        减少50%"（切片 [2,1,50]×14 → {0}=窗口回合、{1}=回血分子（/2）、{2}=减伤%）
//   557（arg_count=3）"{0}回合内对手使用正先制的技能时{1}%令对手{2}"
//
// 实现口径：
//   - 1794 = EVENT_SKILL_INVALID watcher（ev.actor==自身——被打断/miss 都算，
//     J42 1320 同通道；窗口 InRounds {0}）+ 一次性武装旗：首次触发 →
//     heal_amount(max×{1}/2) + register_damage_effect(REDUCE_PCT, MITIGATE)
//     counts=1 共享指针票（1265 直系：下次受到的攻击红伤 -{2}%，消费即耗）。
//   - 557 = InRounds {0} 的 ROUND_START tick（J18 1204 窗）：读对手 roundChoice
//     槽位技能 priority > 0（Skills::priority 静态面板，2273 同读法）→
//     apply_anomaly_checked（{1}% 概率申报，anomaly={2}，random_duration）。
//     ⚠️ 每回合 ROUND_START 判一次（对手当回合选定技能已可知）。
//
// 载体切片（unity 库 json_each 精确匹配 14/15=29；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1794 [回合,回血分子,减伤%]：{2,1,50}×14 全同参
//   557 [回合,概率,异常码]：{3,100,6}×7/{3,100,X}/{2,100,6}/…（纯链 爆裂斩 17521 [3,100,6]）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1794/557 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1320（EVENT_SKILL_INVALID watcher，J42）、1265（REDUCE_PCT counts 票，
//   J20）、1204（InRounds ROUND_START tick 窗，J18）、2273（Skills::priority 读法）。
// 场景回归：test/scenario/scenario_2020_invalid_react_priobserve.cpp（2020 号段，
//   一文件两宏）。

#include "common.h"

#include <memory>

namespace {

constexpr int k1794ArmBase = 990179400;   // +owner：技能无效反制一次性旗

// ── 1794 —— 技能无效时回 1/2 + 下次受击 -50%（窗口一次性）──
// int_args = [owner, target, rounds, heal_num, reduce_pct]（args_num=3）。
EffectResult effect_1794_invalid_react(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1794, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1794, args,
                     "1794 布局=[owner,target,回合,回血分子,减伤%]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int heal_num = args.int_args[3];
    const int reduce_pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1794, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && heal_num > 0 && reduce_pct > 0, 1794, args,
                     "需要 回合/分子/减伤 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1794ArmBase + owner] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, heal_num, reduce_pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            const int key = k1794ArmBase + owner;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 已触发（窗口一次性）
            }
            c->plugin_storage.erase(it);
            (void)g_core->heal_amount(c, owner, max_hp_of(c, owner) * heal_num / 2);
            // 下 1 次受到的攻击伤害 -reduce_pct%（1265 counts 票直系）。
            auto counts = std::make_shared<int>(1);
            c->register_damage_effect(
                DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
                [owner, reduce_pct, counts](BattleContext* cc, int bucket_owner) {
                    if (!cc || bucket_owner != owner || *counts <= 0) {
                        return;
                    }
                    DamageSnapshot& d = cc->resolvedDamage;
                    if (d.defenderId != owner || !d.isRed || d.isDirect || d.final <= 0) {
                        return;
                    }
                    --*counts;
                    d.final -= d.final * reduce_pct / 100;
                });
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 557 的 ROUND_START tick —— 对手选定技能正先制 → 申报制异常 ──
EffectResult j108_557_pri_observe_tick(BattleContext* c, const EffectArgs& args) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !args.int_args
        || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    // 对手当回合选定技能（roundChoice 选择期已写定）的静态先制 > 0 = "正先制"。
    const int idx = c->roundChoice[opp][1];
    const auto& skills = c->getPet(opp).skills;
    if (idx < 0 || idx >= static_cast<int>(skills.size())
        || skills[static_cast<std::size_t>(idx)].priority <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 557 —— {0}回合内对手使用正先制的技能时{1}%令对手{2} ──
// int_args = [owner, target, rounds, chance, anomaly]（args_num=3）。
EffectResult effect_557_pri_observe_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 557, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 557, args,
                     "557 布局=[owner,target,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     557, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0, 557, args, "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 557, args,
                     "需要 异常码 ∈ [0,43]");
    // InRounds {0} ROUND_START tick（1204 窗形态；每次 ROUND_START 判当回合选定技）。
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 557, rounds,
                          {owner, opp, rounds, chance, anomaly},
                          &j108_557_pri_observe_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1794, effect_1794_invalid_react)
SKILL_EFFECT(557, effect_557_pri_observe_anomaly)
