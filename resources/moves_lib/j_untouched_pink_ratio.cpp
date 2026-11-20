// J 组 J263 · 未受击门比例粉伤族二 —— 2 宏/16 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1801 [1] 若上回合自身未受到攻击伤害则附加对手最大体力1/{0}的百分比伤害
//   1631 [2] {0}回合内每回合若自身未受到攻击伤害则回合结束后附加对手最大体力
//            1/{1}的百分比伤害（自身体力为0时也可触发）
//
// 实现口径（先例：j_untouched_gate.cpp 819/1708 受击旗；j_cond_pink_amp.cpp 1046
// 比例粉伤；j7 粉伤族 PERCENT_VALUE 通道）：
//   - 受击旗 = EVENT_TAKE_DAMAGE 自侧 TEAM watcher，**仅 NORMAL 红伤**置旗
//     （"攻击伤害"字面；粉/真/属性伤不置旗），键 = roundCount*2+side。
//     watcher 一次武装、全战场长（plugin_storage 守卫防重复注册）。
//   - 1801 时点 = SKILL_EFFECT：读**上一回合**旗（roundCount-1）；无上一回合（首回合）
//     不触发。⚠️ 首施放盲区（同 j153 记档）：watcher 自首次施放才武装，施放前发生的
//     受击不可见——首回合施放天然读不到上一回合，场景避开该面。
//   - 1631 = 持续 {0} 回合的 BATTLE_OLD_ROUND_END_1 tick：每回合末读**当回合**旗；
//     未受击 → deal_pink_damage(对手, 对手maxHP/{1}, PERCENT_VALUE)。
//     tick 不读自身当前体力，故"自身体力为0时也可触发"在 tick 仍执行的场合自然满足
//     （自身已倒下时桶是否执行属 FSM 面，本单不单独验证，记档）。
//   - 分母在位即消费：{0}/{1} <= 0 视为坏行参（EFFECT_ARG_GUARD 报错），不做
//     ">0 才生效"的静默关闸。
//   - 审计槽（场景精确断言，9xxxx 私有段）：
//       991801200 + owner = 1801 本次粉伤值（0=门闭/未触发）
//       991631200 + owner = 1631 最近一次触发回合（-1=本回合未触发）
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1801 [分母]：28805 六手协奏 [2]（纯链）、29287 守卫片像 [2]、36666 不灭之咒 [2]、
//     35846 繁星赞美诗 [2]、36757 觉律 [3]、36771 浊心异灵咒 [2]、37340 星罗阑珊谱 [2]、
//     38239 溯渊生灭 [2]。
//   1631 [回合,分母]：27384 守护之侍 [3,2]、27580 邪鳞鬼面 [3,3]、27617 树妖之灵 [3,2]、
//     27763 流光溢彩 [3,2]、27920 虎魄圣威 [3,2]、27999 剥茧抽丝 [3,2]、28120 蔓月诡瞳
//     [3,2]、36757 觉律 [3,3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1801/1631 无行为硬编码 ②effect_info 模板
//   唯一、custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集
//   （含 kbatch 与各 agents worktree）无 SKILL_EFFECT(1801/1631) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5940_untouched_pink_ratio.cpp（号段 5940-5949）。

#include "common.h"

namespace {

constexpr int kAttackFlagBase = 9918011;   // +roundCount*2+side：本回合吃过 NORMAL 红伤
constexpr int kArmedBase = 9918019;        // +side：watcher 已武装
constexpr int kAudit1801 = 991801200;      // +owner：1801 本次粉伤值
constexpr int kAudit1631 = 991631200;      // +owner：1631 最近触发回合

// 武装"受击旗"watcher（仅 NORMAL 红伤；TEAM scope 跨回合存活）。
void arm_attack_flag_watcher(BattleContext* ctx, int self) {
    if (ctx->plugin_storage.find(kArmedBase + self) != ctx->plugin_storage.end()) {
        return;
    }
    ctx->plugin_storage[kArmedBase + self] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, /*start=*/0, /*rounds=*/9999, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;
            }
            c->plugin_storage[kAttackFlagBase + c->roundCount * 2 + self] = 1;
        },
        WatcherScope::TEAM);
}

bool took_attack_damage(const BattleContext* ctx, int side, int round) {
    const auto it = ctx->plugin_storage.find(kAttackFlagBase + round * 2 + side);
    return it != ctx->plugin_storage.end() && any_int_or(it->second) != 0;
}

// ── 1801 —— 上回合未受攻击伤害 → 附加对手最大体力1/{0} 粉伤 ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1801_last_round_untouched_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1801, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1801, args,
                     "1801 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1801, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1801, args, "需要 分母 > 0");
    arm_attack_flag_watcher(ctx, owner);
    const int last_round = ctx->roundCount - 1;
    if (last_round < 0) {
        return EffectResult::kOk;   // 首回合没有"上回合"
    }
    if (took_attack_damage(ctx, owner, last_round)) {
        ctx->plugin_storage[kAudit1801 + owner] = 0;
        return EffectResult::kOk;   // 门：上回合受过攻击伤害
    }
    const int pink = max_hp_of(ctx, target) / denom;
    if (pink <= 0 || ctx->getPet(target).hp <= 0) {
        ctx->plugin_storage[kAudit1801 + owner] = 0;
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kAudit1801 + owner] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1631 tick —— 回合末：本回合未受攻击伤害 → 附加对手最大体力1/{1} 粉伤 ──
// int_args = [owner, target, 分母]（窗口由 register_round_effect 承载）。
EffectResult j263_1631_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target
        || denom <= 0) {
        return EffectResult::kOk;
    }
    if (took_attack_damage(ctx, owner, ctx->roundCount)) {
        ctx->plugin_storage[kAudit1631 + owner] = -1;
        return EffectResult::kOk;   // 门：本回合已受攻击伤害
    }
    const int pink = max_hp_of(ctx, target) / denom;
    if (pink <= 0 || ctx->getPet(target).hp <= 0) {
        ctx->plugin_storage[kAudit1631 + owner] = -1;
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kAudit1631 + owner] = ctx->roundCount;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1631 —— {0}回合内每回合未受攻击伤害 → 回合末比例粉伤 ──
// int_args = [owner, target, 回合, 分母]（args_num=2）。
EffectResult effect_1631_window_untouched_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1631, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1631, args,
                     "1631 布局=[owner,target,回合,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1631, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 1631, args, "需要 回合/分母 > 0");
    arm_attack_flag_watcher(ctx, owner);
    ctx->plugin_storage[kAudit1631 + owner] = -1;
    register_round_effect(ctx, State::BATTLE_OLD_ROUND_END_1, owner, 1631, rounds,
                          {owner, target, denom}, &j263_1631_round_end);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1801, effect_1801_last_round_untouched_pink)
SKILL_EFFECT(1631, effect_1631_window_untouched_pink)
