// K309 回合类消除族 —— effect 802（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 802，2026-10-21）：
//   802 冥王·裂空破 3157 args[0,0]（kind=[8]）：「回合开始时，若对手不处于异常状态，
//     则当回合自身攻击技能消除对手回合类效果」
//     → **零参行**（args 全 0，无数值子句）→ 行参不消费（记档）。
//     ⚠️ 参数自证（同族行交叉锚定）：750 args[2,2,1]「先出手时攻击+2，速度+2；后手
//       时消除对手回合类效果」——**db[2]=1**（消除子句的占位）；614 args[1,1,1,1]
//       「每次使用攻击技能消除对手回合类效果…」首个 1 = "每次使用攻击技能"标记。
//       本行两子句（回合开始的门 + 攻击技的消除）都不带数值 → 零参自洽。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   零参行 → 不读行参（K224 257 / K304 1738 零参先例同款）。
//   ⚠️ 走 db_at 而非 a.row 的理由同族统一（K257 起的夜跑口径）：a.row 在**真实对局**
//      里是空的（bind_soulmark_args 丢弃 owned_int_args 前两位）→ 存量用 a.row_or 的
//      文件全靠 fallback 硬编码兜底。本单零参，两种读法等价，但**不消费任何下标**。
//
//   ① **条件判定**（ROUND_START，"回合开始时，若对手不处于异常状态"）：
//      门 = 对手**不处于任何异常状态**（`has_any_active_abnormal_status(opp)` 为假）。
//      记"本回合可消除"旗（plugin_storage，+owner 域）。**每回合重判**（"回合开始时"
//        是逐回合条件——对手这回合中了异常，下回合就不成立了）。
//   ② **消除**（SKILL_EFFECT，"当回合自身攻击技能消除对手回合类效果"）：
//      门 = ①本回合旗为真 ②自己是出手方 ③本回合**成功执行过技能**且是**攻击技**。
//      效果 = break_round_effects(对手)（"断回合"原语：查免断 + 发 EVENT_BREAK +
//        清回合类盔威封属，core_api.h:60）。
//      ⚠️ 判据用 §四点五 私副本（executed_skill_this_round / executed_skill_is_attack，
//         就地复刻 moves_lib/common.h——soul 侧不链那个头；先例：k165 / k306 765 /
//         crawford_4644）。
//      ⚠️ **不是"每次攻击都消除"**：官方是"当回合自身攻击技能"——同一回合多次攻击
//        只消除一次（旗在首次消费后清）。故首次消除后清旗。
//      ⚠️ 旗在 ROUND_START 重写（每回合重判），不是"一次开永久有效"。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 802 无魂印行为硬编码；
//    ②effect_icon kind=[8]、tips 为权威文（dump 原文已核；effect_info 无 802
//    技能模板）；③registry soul 集合无 802、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k238_fullhp_hit_fear_break 883（**break_round_effects 断回合，直系**）、
//   k193_cond_freeze（条件 + 断回合）、k255 320 / k306 765（§四点五 判据私副本）。
// 场景回归：test/scenario/scenario_4650_round_clear_802.cpp（号段 4650-4659）。

#include "common.h"

#include <entities/skills.h>

namespace {

constexpr int kMarkId = 802;
constexpr int kFlagBase = 990802000;   // +owner：本回合"可消除对手回合类效果"旗

// ── 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h，就地复刻）─────
bool executed_skill_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_skill_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// ① ROUND_START：对手不处于异常状态 → 置"本回合可消除"旗（每回合重判）。
EffectResult k309_802_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (ctx->has_any_active_abnormal_status(opp)) {
        // 对手处于异常状态 → 本回合不成立（清旗）。
        ctx->plugin_storage.erase(kFlagBase + owner);
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kFlagBase + owner] = 1;
    return EffectResult::kOk;
}

// ② SKILL_EFFECT：自己用攻击技 → 消除对手回合类效果（旗消费一次）。
EffectResult k309_802_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kFlagBase + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
        return EffectResult::kOk;   // 本回合门不成立
    }
    if (!executed_skill_this_round(ctx, owner) || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // 本回合没成功用攻击技
    }
    // 消费旗：官方"当回合自身攻击技能"——同一回合只消除一次。
    ctx->plugin_storage.erase(it);
    (void)g_core->break_round_effects(ctx, ctx->opponent(owner));
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（802 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k309_802_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k309_802_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k309_802_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
