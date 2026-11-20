// K248 对手提升门攻击反转族 —— effect 812（1 宏 / 2 行）
//（虞姬 3178；author k-prod-6 2026-10-13 立单，号段 3980-3989）
//
// 行清单（tools/soul_effects_dump.py 812，2026-10-13）：
//   row508 args[1,0]「回合开始时，若对手处于能力提升状态，则当回合自身攻击反转对手
//          能力提升状态」
//   row509 args[0,0]「回合开始时，若对手处于能力提升状态，则当回合自身攻击消除对手
//          能力提升状态」（虞姬魂印进化活动）
//   ⚠️ 行参即动词判别位（**修正立单行"双参不消费"的解码**）：db[0]=1 → 反转
//   （stat_boost_reversal）、db[0]=0 → 消除（clear_stat_boosts）；db[1] 恒 0 记档。
//   两行同 pet（3178 虞姬）绑定哪行由精灵魂印配置决定，实现按 db[0] 双动词覆盖。
//   ⚠️ 立单行宠物名笔误：3178 = 虞姬（monsters 表实证），非"班纳尔克"。
//
// 实现（双节点，K236 同形）：
//   · BATTLE_ROUND_START：onstage 且对手在场宠处于能力提升状态（六槽判据）→
//     门旗账本（plugin_storage 记当轮 roundCount）；
//   · EVENT_SKILL_USED 窗（own + 攻击技，K399 同窗——"自身攻击"=使用攻击技）：
//     门旗当轮有效 → 动词消费：db[0]? stat_boost_reversal(opp)（正→等负，查免弱）
//     : clear_stat_boosts(opp)（返回 0=无可消/被免则无后续——本宏无补偿子句）。
//     同回合多次攻击幂等（第二次无可反转/消除，原语自查）。
// boss 无效标注：tips 无尾注 → 不标（默认 boss 有效）。
// ⚠️ 三查记录（k-prod-5 2026-10-13）：①内核无 812 硬编码；②effect_info 812 模板
//    不同文（错位行，按行 tips 落地）；③soul_lib/registry 无 812 注册——真缺口。
// 先例：K236（ROUND_START 门→账本→消费）、k399（SKILL_USED 窗）、chaos_sorensen
//   （六槽判据/消除原语）、core_api:99（stat_boost_reversal）。
// 场景回归：test/scenario/scenario_3980_boostgate_reversal.cpp（号段 3980-3989）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 812;
constexpr int kGateBase = 990760600;   // +owner：门旗账本（值=通过的 roundCount）
constexpr int kCombatStatCount = 6;

bool side_has_any_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < kCombatStatCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

int k248_read_mode(BattleContext* ctx, int owner) {
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return 0;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    return db.size() > 0 ? db[0] : 0;   // 1=反转 / 0=消除（行判别位）
}

// ── ROUND_START：对手提升门 → 账本 ─────────────────────────────────────────

EffectResult k248_812_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (side_has_any_boost(ctx, ctx->opponent(owner))) {
        ctx->plugin_storage[kGateBase + owner] = ctx->roundCount;
    }
    return EffectResult::kOk;
}

// ── 自身攻击技使用 → 门旗当轮有效则消费动词 ─────────────────────────────────

EffectResult k248_812_arm(BattleContext* ctx, const SoulNodeArgs& na) {
    if (!ctx || na.owner < 0 || na.owner > 1 || !g_core
        || !g_core->stat_boost_reversal || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = na.owner;
    const int mode = k248_read_mode(ctx, owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, mode](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || ev.actor != owner || ev.used_attribute) {
                return;   // 仅自己的攻击技
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            const auto gate = wctx->plugin_storage.find(kGateBase + owner);
            if (gate == wctx->plugin_storage.end()
                || any_int_or(gate->second) != wctx->roundCount) {
                return;   // 门闭（或上轮残留旗）
            }
            if (mode != 0) {
                (void)g_core->stat_boost_reversal(wctx, wctx->opponent(owner));
            } else {
                (void)g_core->clear_stat_boosts(wctx, wctx->opponent(owner));
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（812 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k248_812_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k248_812_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE)});
