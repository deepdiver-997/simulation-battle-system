#include <utils/sbs_rand.h>
// J 组 J87 · 盾破反噬/暴击吸取族 —— 1387×15 / 1659×15（2026-10-03，worktree ../agents/j87）
//
// 家族语义（effect_info 官方模板即权威）：
//   1387（arg_count=2）"获得{0}点护盾，护盾消失时附加{1}点固定伤害"
//   1659（arg_count=4）"随机附加给对手{0}-{1}点固定伤害，若打出致命一击则效果转变为
//        吸取对手{2}-{3}点体力"
//
// 实现口径：
//   - 1387 = 1753 镜像（J47 直系，回满换固伤）：shield_bank_.add(0,{0}) 获盾 +
//     一次性武装旗；EVENT_SHIELD_BROKEN watcher（ev.target==自身、任意来源盾破）
//     首破 → fixed_damage(opp,{1}) → 清旗。watcher 永久窗（duration 0），
//     旗清后静默（再获盾/再破不再触发——模板"护盾消失时"单次语义，1753 同口径）。
//   - 1659 = SKILL_EFFECT 直通：amount = rand 区间 [{0},{1}]（含端点，
//     rng.next()%(hi-lo+1)+lo）；暴击分叉读 crit_happened[自身]（query_usage ①.5
//     掷定、SKILL_EFFECT 时点已可读——J18/J55 同旗）：
//     未暴 → fixed_damage(amount)（吃护盾的红伤系固伤）；
//     暴 → 吸取（canglan drain 形：fixed_damage 落点 + 按实际扣血 heal_amount 回），
//     点数用同一段 rand 区间 [{2},{3}]。
//
// 载体切片（unity 库 json_each 精确匹配 15/15=30；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1387 [盾点,固伤]：{300,300}×5/{250,250}×4/{400,300}/…（纯链载体 悲剧终幕 37171 [300,300]）
//   1659 [固低,固高,吸低,吸高]：{200,300,300,400}×3/{150,300,150,300}×2/…
//     （无纯链——场景载体动工挑最短链筛哑兄弟）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1387/1659 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1753（盾破 watcher+一次性旗，J47）、沧岚 581（吸取=固伤落点+dealt 回），
//   J18/J55（crit_happened 旗时点）。
// 场景回归：test/scenario/scenario_1770_shield_break_fixed.cpp（1770 号段，一文件两宏）。

#include "common.h"

namespace {

constexpr int k1387ArmedBase = 990138700;   // +owner：盾破固伤一次性旗

// ── 1387 —— 获得{0}点护盾，护盾消失时附加{1}点固定伤害 ──
// int_args = [owner, target, shield, fixed]（args_num=2）。
EffectResult effect_1387_shield_break_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1387, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1387, args,
                     "1387 布局=[owner,target,盾点,固伤]");
    const int self = args.int_args[0];
    const int shield = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1387, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(shield > 0 && fixed > 0, 1387, args, "需要 盾点/固伤 > 0");
    ctx->getPet(self).shield_bank_.add(/*priority=*/0, shield, /*source_id=*/1387,
                                       /*duration=*/0);
    ctx->plugin_storage[k1387ArmedBase + self] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, fixed](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->fixed_damage) {
                return;
            }
            const int key = k1387ArmedBase + self;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 旗未置/已消耗
            }
            c->plugin_storage.erase(key);
            const int opp = 1 - self;
            if (c->getPet(opp).hp > 0) {
                (void)g_core->fixed_damage(c, opp, fixed);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1659 —— 随机{0}-{1}固伤；打出致命一击则转为吸取{2}-{3}点体力 ──
// int_args = [owner, target, fix_lo, fix_hi, drain_lo, drain_hi]（args_num=4）。
EffectResult effect_1659_rand_fixed_or_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1659, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1659, args,
                     "1659 布局=[owner,target,固低,固高,吸低,吸高]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int fix_lo = args.int_args[2];
    const int fix_hi = args.int_args[3];
    const int drain_lo = args.int_args[4];
    const int drain_hi = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1659, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fix_lo > 0 && fix_hi >= fix_lo && drain_lo > 0 && drain_hi >= drain_lo,
                     1659, args, "需要 区间合法（低>0、高≥低）");
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int amount =
        fix_lo + (fix_hi > fix_lo ? ctx->rng().next() % (fix_hi - fix_lo + 1) : 0);
    if (ctx->crit_happened[owner]) {
        // "转变为吸取"：同法落点 + 按实际扣血回（沧岚 581 drain 形）。
        // 点数：吸低-吸高再掷一段（模板独立区间）。
        const int drain =
            drain_lo + (drain_hi > drain_lo ? ctx->rng().next() % (drain_hi - drain_lo + 1) : 0);
        const int before = ctx->getPet(opp).hp;
        (void)g_core->fixed_damage(ctx, opp, drain);
        const int dealt = before - ctx->getPet(opp).hp;
        if (dealt > 0) {
            (void)g_core->heal_amount(ctx, owner, dealt);
        }
        return EffectResult::kOk;
    }
    (void)g_core->fixed_damage(ctx, opp, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1387, effect_1387_shield_break_fixed)
SKILL_EFFECT(1659, effect_1659_rand_fixed_or_drain)
