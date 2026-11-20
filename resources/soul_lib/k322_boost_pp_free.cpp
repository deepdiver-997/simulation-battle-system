// K322 强化态免耗 PP 族 —— effect 353（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 353，2026-10-21）：
//   353 初夏 2733 args[1,3]（kind=[]）：「自身处于能力提升时 使用技能不消耗PP」
//     → **零语义参数**（args=[1,3] 找不到对应文本子句：tips 只有"能力提升"门 +
//       "使用技能不消耗PP"效果，无数值）→ **args 全不消费**（记档，工单行同口径
//       "参数废位按 tips"）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   本单**不读任何行参**（按 tips 字面定值）——与 K304 1738 / K309 802 / K311 323
//   零参先例同款。
//
//   门 = 自身**处于能力提升状态**（任一能力槽 > 0，K213 316 / K255 320 直系判据）。
//   效果 = ws.skill_pp_cost_multiplier[owner] = 0（**PP 消耗倍率槽**：内核 PP 消耗点
//     AFTER_ACTION 的 consume_selected_skill_pp 读它，≤0 时**不扣 PP**——
//     battleFsm.cpp:451-452）。
//   时点 = BATTLE_FIRST/SECOND_SKILL_EFFECT（**必须在此**）：PP 消耗点在
//     AFTER_ACTION（battleFsm.cpp:457 扣 pp），而 SKILL_EFFECT 在其之前 → 本次使用
//     免耗。412「体力小于1/N则每次攻击不消耗PP」（batch_c_skills.cpp）同款时点。
//     ⚠️ "使用技能"不限攻击技——故**不加攻击技类型门**（412 的文案是"每次攻击"、
//       它取"本技能每次使用"落；本行字面就是"使用技能"，故全部技能都免耗）。
//
//   ⚠️ **每回合 reset → 必须每回合重写**（本单最大的坑）：ws.skill_pp_cost_multiplier
//     在**回合初 reset 回 1**（battleWorkspace.h:149 `skill_pp_cost_multiplier[i] = 1`）。
//     故"某回合开了免耗、下一回合门不成立"时，下一回合的倍率会**自动回到 1**
//     （不会残留）。但反过来——本单在 SKILL_EFFECT **写 0** 是"本次使用免耗"，
//     下一回合自动回 1，**不需要**额外的清零逻辑。
//     ⚠️ **但要注意窗口残留的另一种形态**：若本单的免耗写在某回合的 SKILL_EFFECT，
//         而该回合**没有出手**（NONE/被控）→ 槽仍是 1（没写过）→ 无残留。反之若写
//         了 0 而该回合之后又用别的技能……同一回合内多次使用，第二次仍免耗
//         （官方"使用技能不消耗PP"字面如此）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 353 无魂印行为硬编码；
//    ②effect_icon kind=[]、tips 为权威文（dump 原文已核；effect_info 无 353
//    技能模板）；③registry soul 集合无 353、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：batch_c_skills 412（**PP 免耗：ws.skill_pp_cost_multiplier=0 + SKILL_EFFECT
//   时点，直系**）、k213_boost_paralyze 316（能力提升门判据）、k255 320（同款门）。
// 场景回归：test/scenario/scenario_4780_boost_pp_free_353.cpp（号段 4780-4789）。

#include "common.h"

namespace {

constexpr int kMarkId = 353;

// 自身处于能力提升状态 = 任一能力槽 > 0（K213 316 直系判据）。
bool self_has_boost(const BattleContext* ctx, int owner) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

// SKILL_EFFECT：处于能力提升状态 → 本次使用不消耗 PP（倍率槽写 0）。
EffectResult k322_353_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (!self_has_boost(ctx, owner)) {
        return EffectResult::kOk;   // 门闭：不处于能力提升状态
    }
    // PP 消耗倍率槽写 0 → 内核 PP 消耗点（AFTER_ACTION）读到 ≤0 → 不扣 PP。
    ctx->ws.skill_pp_cost_multiplier[owner] = 0;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（353 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k322_353_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k322_353_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
