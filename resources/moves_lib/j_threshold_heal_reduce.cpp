// J 组 J25 · 阈值回血/减伤窗族 —— 3 宏/53 载体（2026-10-02，worktree ../agents/j25）
//
// 家族语义（effect_info 模板即权威）：
//   507 [1] 下回合若受到的伤害大于{0}，则恢复自身全部体力
//   624 [2] 消除对手能力上升状态，消除成功{0}回合内受到伤害减少{1}点
//   134 [2] 若造成的伤害低于{0}，则所有技能的PP值恢复{1}点
//
// 实现口径：
//   - 507 = J15 775 直系（TAKE_DAMAGE 量判：NORMAL 红伤、ev.amount 严格 >{0} →
//     heal_max_hp_fraction 回满），一次性 **NextRounds(1)** 窗——watcher 锚点用
//     round_effect_start_round(owner,1,NextRounds) 显式折算（先/后手都=R+1），
//     窗 [R+1,R+1]。多次受击每次判（回满后 clamp 自然幂等）。
//   - 624 = J2 消强门（clear_stat_boosts>0）+ REDUCE_FLAT MITIGATE 点数减伤窗
//     （463/969 同形，**InRounds** {0} 含当回合；shared_ptr<Win> storage 刷新）。
//   - 134 = J15 729 的 DETECT 相位对称判（攻方桶读 final **严格 <** {0}）→
//     pp_restore(owner, {1}, only_empty=false) 全技能回 PP（2533/2076 同原语）。
//     恒久（模板无窗口）。
//
// 载体切片（unity 库 json_each 精确匹配 16+17+20=53；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   507  [阈值]：22484 灵魂之音 [200]（+四枚 effect4 自升伴技）、23893 防守状态 [300]
//   624  [rounds,点数]：16676 耀光斩杀 [2,200]（纯载体）、33958 极寒风雷拳 [3,300]
//   134  [阈值,点数]：36680 野性伏击 [300,10]（PP+10 测试载体）、12545 光束反冲 [100,1]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1100_threshold_heal_reduce.cpp（号段 1100-1109）。

#include "common.h"

#include <memory>

namespace {

// ── 507 —— 下回合若受到的伤害大于{0}，则恢复自身全部体力 ──
// int_args = [owner, target, threshold]（args_num=1）。
EffectResult effect_507_next_hit_heal_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 507, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 507, args,
                     "507 布局=[owner,target,阈值]");
    const int self = args.int_args[0];
    const int threshold = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     507, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0, 507, args, "需要 阈值 > 0");
    // "下回合"：NextRounds(1) 单回合窗（先/后手都折算到 R+1）。
    // ⚠️ 事件中心的 watcher 窗口**不防未来锚点**（current < register 也算激活，
    //    expiry 只判 current-register >= duration）→ 锚当前回合、时长+1 存活到窗末，
    //    lambda 内自卡 roundCount == start（J25 实测坑，记档）。
    const int start = ctx->round_effect_start_round(self, /*rounds=*/1,
                                                    EffectWindowKind::NextRounds);
    const int duration = (start - ctx->roundCount) + 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, duration, /*once=*/false,
        [self, threshold, start](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->roundCount != start) {
                return;   // 只在"下回合"生效
            }
            if (ev.target != self || !g_core || !g_core->heal_amount) {
                return;
            }
            // "受到的伤害" = 红伤实扣量，严格大于阈值（775 同判据）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= threshold) {
                return;
            }
            (void)heal_max_hp_fraction(c, self, 1);   // 恢复自身全部体力
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 624 —— 消除对手能力上升状态，消除成功{0}回合内受到伤害减少{1}点 ──
// int_args = [owner, target, rounds, reduce]（args_num=2）。463/969 同形（InRounds）。
EffectResult effect_624_purge_then_flat_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 624, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 624, args,
                     "624 布局=[owner,target,回合,点数]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int reduce = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1 && self != target,
                     624, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && reduce > 0, 624, args, "需要 回合/点数 > 0");
    if (!g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    // J2 消强门：清掉个数 >0 才触发。
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;
    }
    using Win = std::pair<int, int>;   // [start, expire)
    const int key = 624000 + self;
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    auto win = std::make_shared<Win>(start, start + rounds);
    if (ctx->plugin_storage.count(key)) {
        *std::any_cast<std::shared_ptr<Win>>(ctx->plugin_storage[key]) = *win;
        return EffectResult::kOk;   // 重复使用=刷新窗口，不叠加（463 同形）
    }
    ctx->plugin_storage[key] = win;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, reduce, win](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            const int rc = c->roundCount;
            if (rc < win->first || rc >= win->second) {
                return;   // 窗口外
            }
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                return;   // 只减"所受的攻击伤害"（红伤，463 家族口径）
            }
            damage.final = std::max(0, damage.final - reduce);
        });
    return EffectResult::kOk;
}

// ── 134 —— 若造成的伤害低于{0}，则所有技能的PP值恢复{1}点 ──
// int_args = [owner, target, threshold, points]（args_num=2）。恒久。
EffectResult effect_134_low_damage_pp_restore(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 134, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 134, args,
                     "134 布局=[owner,target,阈值,点数]");
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     134, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && points > 0, 134, args, "需要 阈值/点数 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, threshold, points](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final >= threshold) {
                return;   // "造成的伤害低于{0}"（严格小于）
            }
            if (g_core && g_core->pp_restore) {
                (void)g_core->pp_restore(c, owner, points, /*only_empty=*/false);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(507, effect_507_next_hit_heal_full)
SKILL_EFFECT(624, effect_624_purge_then_flat_reduce)
SKILL_EFFECT(134, effect_134_low_damage_pp_restore)
