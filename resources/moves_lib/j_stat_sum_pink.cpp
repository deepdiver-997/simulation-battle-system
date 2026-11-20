// J 组 J293 · 属性总和/速度段粉伤族 —— 2 宏/14 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   876 [1] "附加自身防御、特防和速度总和{0}%的百分比伤害"
//   812 [1] "自身速度能力每提升1段，则回合结束时减少对手1/{0}最大体力"
//
// 实现口径：
//   - 876 = SKILL_EFFECT 即时直结（1163 面板粉伤同形，j_panel_pink.cpp）：自身
//     DEF+SDEF+SPEED **视图值**总和 × {0}% → deal_pink_damage(PERCENT_VALUE) 打对手。
//     视图值 = ws.getTempAbilityValue（含能力等级，676/1163 同口径；官方若按面板算
//     改这一行即可，记档）。乘区走 checked_pct_mul（全库 int 乘区统一入口）。
//     "附加"= 技能本体落地后追加一段粉伤；SKILL_EFFECT 时点与 1163 一致。
//     行参在位即消费：显式 0 档 = 关闸（不粉伤），不回退默认值。
//   - 812 = 用技后挂一条**常驻** BATTLE_ROUND_END 条目（duration=-1，1995 形；
//     source_id_=812 同源去重 → 连续使用刷新覆盖、不叠加；scope 默认 ON_STAGE →
//     换下场随 epoch 作废 = "本次上场有效"）。每回合末读**本体速度等级**
//     `ability_levels[owner][4]`（槽 4=速度），k=max(0,等级)；
//     削减 pct = k × 100/{0}（k 段合计，一次调 reduce_max_hp_pct，floor=1）；
//     k=0/负等级 → 不削。百分比取整口径按原语（上取整），与 725 同。
//     行参在位即消费：{0}≤0 = 关闸（不注册）。
//
// plugin_storage 审计槽（精确断言用；grep 全库无撞）：
//   998760000 + owner = 876 本次粉伤值
//   998760100 + owner = 876 本次三属性视图总和
//   998120000 + owner = 812 本次 tick 的速度等级（钳 0 前原值另存见 998120300）
//   998120100 + owner = 812 本次 tick 申报 pct
//   998120200 + owner = 812 累计 tick 次数
//   998120300 + owner = 812 本次 tick 读到的原始速度等级（可为负，便于断言负等级门）
//
// 载体切片（side_effect.arg_count 顺序切分，脚本核算 2026-10-22，7/7 + 7/7 自洽）：
//   876 [pct]：19169 神怒破灭断 [20] / 29429 岩壁建构 [30] / 32705 凌风长鸣 [20] /
//     33269 雨水如丝 [20] / 33412 自然莲印 [20]（纯链，场景载体）/
//     34018 殿下漂漂拳 [25] / 35357 绝影十字斩 [30]。
//   812 [分母]：17950 王·雷鳞机甲 [6] / 28702 冥腐之沙华 [4] / 30809 王·擎天之光 [6] /
//     33119 战狮破劫 [8] / 34211 飞矢流影 [6]（纯链，场景载体）/
//     34889 豪气冲天撞 [6] / 36900 鹰姿飒爽 [10]。
//
// 动工前置三查（2026-10-22）：①内核 grep 876/812 无行为硬编码
//   ②effect_info 模板唯一、custom_effect_overrides/programs 零行
//   ③全库 moves_lib/soul_lib 并集（含 kbatch 与各 agents worktree）无 SKILL_EFFECT
//   注册——真缺口。（soul_lib/k248 的 812 是 SOUL_PROGRAM 魂印空间，id_collision 按
//   类别判定，不与技能效果空间互撞。）
//
// 场景回归：test/scenario/scenario_6260_stat_sum_pink.cpp（号段 6260-6269）。

#include "common.h"

#include <numerical-calculation/calculation.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int k876PinkAuditBase = 998760000;   // +owner：本次粉伤值
constexpr int k876SumAuditBase = 998760100;    // +owner：本次三属性视图总和
constexpr int k812StageAuditBase = 998120000;  // +owner：本次 tick 有效速度段（钳 0 后）
constexpr int k812PctAuditBase = 998120100;    // +owner：本次 tick 申报 pct
constexpr int k812TickAuditBase = 998120200;   // +owner：累计 tick 次数
constexpr int k812RawStageBase = 998120300;    // +owner：本次 tick 原始速度等级（可负）
constexpr int kAbilitySlotSpeed = 4;           // 能力等级槽 4 = 速度（0攻1特攻2防3特防4速5命中）

// ── 876 —— 附加自身防御、特防和速度总和{0}%的百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_876_stat_sum_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 876, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 876, args,
                     "876 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     876, args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 关闸（不开默认值回退）。
    ctx->plugin_storage[k876PinkAuditBase + owner] = 0;
    if (pct <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // "防御、特防和速度总和"：视图槽（含能力等级，676/1163 同口径）。
    const int sum = ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE)
        + ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE)
        + ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPEED);
    ctx->plugin_storage[k876SumAuditBase + owner] = sum;
    const int pink = checked_pct_mul(sum, pct - 100, "effect_876");
    ctx->plugin_storage[k876PinkAuditBase + owner] = pink;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 812 的 BATTLE_ROUND_END tick —— payload={owner, target, denom} ──
EffectResult j293_812_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->reduce_max_hp_pct) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target || denom <= 0) {
        return EffectResult::kOk;
    }
    // "自身速度能力每提升1段"：本体等级（on-stage 作用域，换宠清空——与条目 scope 一致）。
    const int raw_stage = ctx->ability_levels[owner][kAbilitySlotSpeed];
    ctx->plugin_storage[k812RawStageBase + owner] = raw_stage;
    const int stage = raw_stage > 0 ? raw_stage : 0;
    ctx->plugin_storage[k812StageAuditBase + owner] = stage;
    ctx->plugin_storage[k812PctAuditBase + owner] = 0;
    const auto it = ctx->plugin_storage.find(k812TickAuditBase + owner);
    ctx->plugin_storage[k812TickAuditBase + owner] =
        (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    if (stage <= 0) {
        return EffectResult::kOk;   // 无提升段 → 不削
    }
    const int pct = stage * 100 / denom;
    ctx->plugin_storage[k812PctAuditBase + owner] = pct;
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->reduce_max_hp_pct(ctx, target, ctx->on_stage[target], pct, /*floor=*/1);
    return EffectResult::kOk;
}

// ── 812 —— 自身速度能力每提升1段，则回合结束时减少对手1/{0}最大体力 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_812_speed_stage_maxhp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 812, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 812, args,
                     "812 布局=[owner,target,denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     812, args, "需要 owner/target ∈ {0,1}");
    if (denom <= 0) {
        return EffectResult::kOk;   // 行参在位即消费：显式 0/负档 = 关闸（不注册）
    }
    // 常驻 ROUND_END（1995 形）：duration=-1；source_id 同源去重（重复使用刷新不叠加）；
    // scope 默认 ON_STAGE —— 换下场随 epoch 作废（"本次上场有效"）。
    Effect e(812, 0, owner, /*left_round=*/-1,
             EffectArgs(std::vector<int>{owner, target, denom}), &j293_812_round_end_tick);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 812;
    ctx->register_skill_effect(State::BATTLE_ROUND_END, owner, std::move(ce));
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(876, effect_876_stat_sum_pink)
SKILL_EFFECT(812, effect_812_speed_stage_maxhp)
