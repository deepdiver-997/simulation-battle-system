// J 组 J288 · 护盾/护罩转固伤族 —— 2 宏/14 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1662 [0] 消耗自身所有护盾值并附加等量固定伤害，若对手体力未变化则恢复自身等量体力值
//   1537 [1] 消耗自身所有护罩并造成等量固定伤害，若当前护罩值低于{0}则造成的固定伤害翻倍
//
// 实现口径：
//   - 两宏都是"资源清空 → 等量固伤"的一步式技能效果（SKILL_EFFECT 时点、默认桶）：
//     先读账再 consume（consume 之后 total 读不到），被拿空的每条 emit
//     EVENT_SHIELD_BROKEN（"以此法消耗的护盾视为被击破"，K2 2263/2318 与
//     j43_shield_punish 1017 同口径）。
//   - 1662 = 护盾（shield_bank_，只响应红伤）→ fixed_damage(对手, 消耗值)；
//     **结算前后对手 hp 相等** = 这次固伤没落地（免粉/护罩吃光/对手 hp 已 0）→
//     heal_amount(自身, 消耗值)（"恢复自身等量体力值"；吃封回血，实际恢复量入审计槽）。
//     注意："体力未变化"判的是**本次固伤**前后，不是整条技能前后。
//   - 1537 = 护罩（hood_bank_，只响应粉伤）→ fixed_damage；"当前护罩值低于{0}"
//     = 消耗前总额 **严格小于** {0} → 本次固伤 ×2；{0}=0 是显式关闸档（永不翻倍，
//     在位即消费，不以 `>0` 拒绝参数）。
//   - 落点统一 fixed_damage（FixedDamage 原语，粉伤通道；1104/J43 同款）。
//
// plugin_storage 审计槽（精确断言用；grep 全库无撞）：
//   991662000 + owner = 本次消耗护盾值
//   991662100 + owner = 本次实际恢复量（被 clamp/封回血后的 hp 差）
//   991537000 + owner = 本次消耗护罩值
//   991537100 + owner = 本次固伤值（含翻倍）
//
// 载体切片（side_effect.arg_count 顺序切分，脚本核算 2026-10-22）：
//   1662 []：28939 钢铁雄心 [48,47,2050,1655,1662]（属性技，兄弟为免疫/PP 类，无红伤）、
//     34868 霞辉灵璧石 [1224,1662,1663]、37329 奥瑞亚之息 [1500,1662]…（7/7 零参）
//   1537 [阈值]：28116 残躯不腐 [191,426,1394,1537]|[4,3,100,3,2,350]（属性技，阈值 350）、
//     33902 幻星流沙阵 [679,1537]|[2,250]、36817 天荡灵绝震 [1281,1537,1702]|[300,200]…（7/7）
//
// 动工前置三查（2026-10-22）：①内核 grep 1662/1537 无硬编码（仅注释同款引用）
//   ②effect_info 模板唯一、custom_effect_programs/overrides 零行 ③全分支 moves_lib/
//   soul_lib 并集无 SKILL_EFFECT 注册——真缺口。
// 先例：j43_shield_punish.cpp 1017（主动 consume + 破盾事件）、j_clear_shields_pink.cpp
//   1828（shield/hood 双账）、j_hp_ratio_pink_drain.cpp（fixed_damage 数值落点）。
// 场景回归：test/scenario/scenario_6210_shield_burst_heal.cpp（号段 6210-6219）。

#include "common.h"

namespace {

constexpr int k1662ConsumedAuditBase = 991662000;  // +owner：消耗护盾值
constexpr int k1662HealAuditBase = 991662100;      // +owner：实际恢复量
constexpr int k1537ConsumedAuditBase = 991537000;  // +owner：消耗护罩值
constexpr int k1537DamageAuditBase = 991537100;    // +owner：本次固伤值

// ── 1662 —— 消耗自身所有护盾值并附加等量固定伤害；对手体力未变化则恢复自身等量 ──
// int_args = [owner, target]（args_num=0；bind_participants 仍带两侧）。
EffectResult effect_1662_shield_burst_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1662, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1662, args,
                     "1662 布局=[owner,target]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1662, args,
                     "需要 owner/target ∈ {0,1}");
    int broken = 0;
    const int total = ctx->getPet(self).shield_bank_.total();
    const int consumed = total > 0 ? ctx->getPet(self).shield_bank_.consume(total, &broken) : 0;
    for (int i = 0; i < broken; ++i) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, -1, self});
    }
    ctx->plugin_storage[k1662ConsumedAuditBase + self] = consumed;
    ctx->plugin_storage[k1662HealAuditBase + self] = 0;
    if (consumed <= 0) {
        return EffectResult::kOk;   // 无盾可消耗 → 整条无事
    }
    const int foe_before = ctx->getPet(opp).hp;
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, consumed);
    }
    if (ctx->getPet(opp).hp == foe_before && g_core && g_core->heal_amount) {
        const int self_before = ctx->getPet(self).hp;
        (void)g_core->heal_amount(ctx, self, consumed);
        ctx->plugin_storage[k1662HealAuditBase + self] = ctx->getPet(self).hp - self_before;
    }
    return EffectResult::kOk;
}

// ── 1537 —— 消耗自身所有护罩并造成等量固定伤害；消耗前护罩 < {0} 则固伤翻倍 ──
// int_args = [owner, target, 阈值]（args_num=1）。
EffectResult effect_1537_hood_burst_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1537, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1537, args,
                     "1537 布局=[owner,target,阈值]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1537, args,
                     "需要 owner/target ∈ {0,1}");
    // 红线7：阈值在位即消费（0 = 显式"永不翻倍"档，不拒绝）。
    EFFECT_ARG_GUARD(threshold >= 0, 1537, args, "需要 阈值 >= 0");
    int broken = 0;
    const int total = ctx->getPet(self).hood_bank_.total();
    const int consumed = total > 0 ? ctx->getPet(self).hood_bank_.consume(total, &broken) : 0;
    for (int i = 0; i < broken; ++i) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, -1, self});
    }
    const int damage = consumed * (consumed < threshold ? 2 : 1);
    ctx->plugin_storage[k1537ConsumedAuditBase + self] = consumed;
    ctx->plugin_storage[k1537DamageAuditBase + self] = damage;
    if (damage <= 0) {
        return EffectResult::kOk;   // 无护罩可消耗 → 不产生 0 伤
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, damage);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1662, effect_1662_shield_burst_heal)
SKILL_EFFECT(1537, effect_1537_hood_burst_double)
