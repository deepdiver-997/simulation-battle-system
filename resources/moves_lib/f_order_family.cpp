// F 组 F5 · 先后手条件族：186 / 1344 / 1472（2026-09-29，feat/f5）
//
// 效果与载体（看板 F5 行，DB 切片 side_effect.arg_count 优先）：
//   186  "后出手时，{1}%使自身{0}提升{2}个等级"（args_num=3）
//        蓄力冲天 13053 [4,50,1] —— 4399（伊力沙 1310）：后出手时，50%令自身速度等级+1
//        奋力反击 13057 [0,100,1] —— 4399（阿里斯 1313）：后出手时，自身攻击等级+1
//   1344 "全属性+{0}，后出手时自身强化效果翻倍"（args_num=1）
//        狂沙凝聚 26114 [1]（斯瓦尼根 3607）—— 4399：必中；全属性+1，后出手时自身强化效果翻倍；…
//        亡星天降 26146 [1]（埃拉辛姆 3618）—— 4399 缺文，effect_info 模板即权威
//   1472 "自身处于能力提升状态时造成的伤害提升{0}%，先出手时效果翻倍"（args_num=1）
//        雷破苍穹 33364 [50]（暗霆·伊修兰迪 3879）、冰川裂隙闪 33746 [50]（冠绝冰龙 3952）
//        —— 4399 两者均：自身处于能力提升状态时造成的伤害提升50%，先出手时效果翻倍
//
// 判据与先例：
//   - 先/后手 = acting_side_is_first(ctx)（common.h：currentState 在 FIRST_ACTION 区间即
//     先手；SKILL_EFFECT 时点先手权已定——1395/919/1178 同判据；"后出手" = 取反）。
//   - 186 单槽提升走 stat_change 真原语（自身增益**不查免弱**；本体/视图一并同步——
//     手抄 clamp 只改 pet.levels 会让速度不参与先手权比较，effect 4 高速移动同款）。
//     {0} 是**能力等级槽序**（0攻 1特攻 2防 3特防 4速 5命中），4399 佐证：13053 的
//     {0}=4 → "速度等级+1"、13057 的 {0}=0 → "攻击等级+1"。
//   - 1344 全属性走 all_stats_up（common.h 助手：六槽含命中，1293 先例）；"后出手时自身
//     强化效果翻倍" = 后手时本效果的 全属性+{0} 再写一次（合计 +2×{0}）。
//     ⚠️ 口径记档：官方文本未给"强化效果翻倍"更宽的作用域定义，按 4399 上下文
//     （紧跟"全属性+1"）自限为**本效果的强化翻倍**；若实测是"一切强化来源翻倍"需内核级
//     增幅钩子，本实现不覆盖，记台账。
//   - 1472 "处于能力提升状态" = 本体 ability_levels 六槽任一正等级（962/1158/1178 同判据，
//     含命中、负等级不算）；增伤 = ws.damage_add_pct += {0}（通用增伤·加算通道，1178
//     同款）；"先出手时效果翻倍" = 先手再 += {0} 一次（1395 双写同款）。
//     AMP 结算式 damage.final*(100+pct)/100 在 raw_attack_damage 台账**之后** →
//     场景按 loss == raw×(100+Σpct)/100 断言（420 暴击 ×2 同款）。
//
// 链上非本单效果（不实现、不干扰）：927/707（回合回血族，effect_batch8/round_window 已接）、
//   776（下N回合威力翻倍，round_window 已接）、693（下N回合增伤，round_window 已接）、
//   915（消除回合类→冻伤窗，e_break_round 已接）、1350（未实现，链上静默跳过）。
//
// 场景回归：test/scenario/scenario_650_order_family.cpp。

#include "common.h"

namespace {

// "自身处于一种能力提升状态"计数（962/1158/1178 同判据：本体正等级、**含命中**共 6 槽，
// 用户 2026-09-18 口径；d5_D6.cpp 的 count_boost_kinds_3404 同式，插件不跨 .so 复制）。
int count_boost_kinds_f5(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 186 —— 后出手时，{1}%使自身{0}提升{2}个等级 ──
// Args: [owner(0), target(1), slot{0}, percent{1}, levels{2}]（int_count=5）。
// 先手短路在掷骰**之前**（条件不成立不耗 rand；percent=100 时 roll_percent 同样零消耗）。
EffectResult effect_186_second_mover_stat_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 186, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 186, args, "需要 [owner,target,slot,percent,levels]");
    EFFECT_ARG_GUARD(g_core && g_core->stat_change != nullptr, 186, args,
                     "需要 stat_change 原语");
    const int owner = args.int_args[0];
    const int slot = args.int_args[2];
    const int percent = args.int_args[3];
    const int levels = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 186, args, "owner 越界");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 186, args,
                     "等级槽越界（0攻 1特攻 2防 3特防 4速 5命中）");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先出手：条件不成立（不掷骰）
    }
    if (!roll_percent(ctx->rng(), percent)) {
        return EffectResult::kOk;   // 概率未触发
    }
    (void)g_core->stat_change(ctx, owner, slot, levels);
    return EffectResult::kOk;
}

// ── 1344 —— 全属性+{0}，后出手时自身强化效果翻倍 ──
// Args: [owner(0), target(1), up{0}]（int_count=3）。
// 先手：all_stats_up(+{0}) 一次；后手：再 +{0} 一次（合计 2×{0}）。
EffectResult effect_1344_all_stats_up_second_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1344, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1344, args, "需要 [owner,target,up]");
    const int owner = args.int_args[0];
    const int up = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1344, args, "owner 越界");
    if (up <= 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, up);
    if (!acting_side_is_first(ctx)) {
        all_stats_up(ctx, owner, up);   // 后手：强化翻倍
    }
    return EffectResult::kOk;
}

// ── 1472 —— 自身处于能力提升状态时造成的伤害提升{0}%，先出手时效果翻倍 ──
// Args: [owner(0), target(1), pct{0}]（int_count=3）。
// 无提升 → 不增伤；有提升 → +{0}%；先手 → 再 +{0}%（翻倍）。SKILL_EFFECT 直写
// ws.damage_add_pct（通用增伤加算通道；ws 每回合 reset，本效果只管本次攻击，无窗口）。
EffectResult effect_1472_boost_cond_damage_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1472, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1472, args, "需要 [owner,target,pct]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1472, args, "owner 越界");
    if (pct <= 0 || count_boost_kinds_f5(ctx, owner) <= 0) {
        return EffectResult::kOk;   // 无提升状态：不增伤
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    if (acting_side_is_first(ctx)) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);   // 先手：效果翻倍
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(186, effect_186_second_mover_stat_up);
SKILL_EFFECT(1344, effect_1344_all_stats_up_second_double);
SKILL_EFFECT(1472, effect_1472_boost_cond_damage_up);
