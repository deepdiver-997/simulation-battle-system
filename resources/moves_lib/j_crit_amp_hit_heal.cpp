// J 组 J95 · 暴击增幅/受伤回满反噬族 —— 1840×14 / 1073×14（2026-10-03，worktree ../agents/j95）
//
// 家族语义（effect_info 官方模板即权威）：
//   1840（arg_count=1）"若打出致命一击则造成的攻击伤害提升{0}%"
//   1073（arg_count=2）"{0}回合内受到的伤害大于{1}则主动恢复自身全部体力并造成等同
//        于恢复量的固定伤害"
//
// 实现口径：
//   - 1840 = SKILL_EFFECT 时点读 crit_happened[自身]（暴击在 query_usage ①.5 掷定、
//     SKILL_EFFECT 已可读——J87/J18 同旗同时点）→ 命中则 ws.damage_add_pct 直写
//     （effect_set_damage_amp，1103/920 通用加法通道）落本次攻击 snapshot。
//   - 1073 = EVENT_TAKE_DAMAGE watcher（535/817 通道：TEAM scope、InRounds {0} 含
//     当回合）+ 一次性武装旗（触发阈值：红伤 NORMAL、amount 严格 > {1}）→
//     heal_max_hp_fraction(1) 回满（实际恢复量 = 前后 hp 差，封回血/钳满天然生效）
//     → fixed_damage(对手, 实际恢复量)（"等同于恢复量"——415 DETECT 形变体）。
//
// 载体切片（unity 库 json_each 精确匹配 14/14=28；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1840 [pct]：{50}×9/{100}×4/{150}
//     （短链载体 镇土铁碎阵 37043 [1840,1323✓]|[50,50] 物150——1323 已实现）
//   1073 [回合,阈值]：{3,300}×10/{4,300}×3/{2,200}
//     （短链载体 惊天战意 25842 [191✓,47✓,1073]|[4,4,3,300] 属——兄弟均实现）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1840/1073 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1103/1439（amp 直写，J51/J17）、crit_happened（J18/J87 时点）、
//   535/153（TAKE_DAMAGE watcher，J24）、415（受伤回满+等量固伤变体，J49）、
//   25842 惊天战意的 191/47 兄弟均实现良性。
// 场景回归：test/scenario/scenario_1840_crit_amp_hit_heal.cpp（1840 号段，一文件两宏）。

#include "common.h"

namespace {

constexpr int k1073ArmBase = 990107300;   // +owner：受伤回满一次性旗

// ── 1840 —— 若打出致命一击则造成的攻击伤害提升{0}% ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_1840_crit_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1840, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1840, args,
                     "1840 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1840, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 1840, args, "需要 pct > 0");
    if (!ctx->crit_happened[owner]) {
        return EffectResult::kOk;   // "若打出致命一击"门（①.5 已掷，此处读真值）
    }
    EffectArgs tick_args(std::vector<int>{owner, args.int_args[1], pct});
    return effect_set_damage_amp(ctx, tick_args);   // 通用加法 amp 落本次 snapshot
}

// ── 1073 —— {0}回合内受到的伤害大于{1}则回满并造成等量固伤 ──
// int_args = [owner, target, rounds, threshold]（args_num=2）。
EffectResult effect_1073_hit_heal_recoil(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1073, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1073, args,
                     "1073 布局=[owner,target,回合,阈值]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1073, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 1073, args, "需要 回合/阈值 > 0");
    if (!g_core || !g_core->heal_amount || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1073ArmBase + owner] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp, threshold](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || !g_core || !g_core->heal_amount
                || !g_core->fixed_damage) {
                return;
            }
            const int key = k1073ArmBase + owner;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 已触发（一次性）
            }
            // "受到的伤害大于{1}"：红伤、严格大于（535/817 判据族）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= threshold) {
                return;
            }
            c->plugin_storage.erase(key);
            const int before = c->getPet(owner).hp;
            (void)heal_max_hp_fraction(c, owner, 1);   // 回满（实际恢复量受钳满/封回血）
            const int healed = c->getPet(owner).hp - before;
            if (healed > 0 && c->getPet(opp).hp > 0) {
                (void)g_core->fixed_damage(c, opp, healed);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1840, effect_1840_crit_amp)
SKILL_EFFECT(1073, effect_1073_hit_heal_recoil)
