// J 组 J59 · 被消响应族 —— 804×16 / 998×16（2026-10-03，worktree ../agents/j59）
//
// 家族语义（effect_info 官方模板即权威），两条都是"窗口内自强被消/被吸 → 反击"：
//   804（arg_count=2）"{0}回合内自身能力提升状态被消除或吸取时，令对手全属性-{1}"
//   998（arg_count=2）"{0}回合内自身能力提升状态被消除或吸取时，{1}回合内对手属性技能命中效果失效"
//
// 实现口径：
//   - 共门 = EVENT_STAT_BOOSTS_LOST watcher（E13 广播 TEAM scope，1109 J16 直系注册法，
//     InRounds {0} 回合含当回合）。⚠️ 与 1109 的差异：1109 模板只"被消除"（kind=0），
//     本族模板是"被消除**或吸取**"→ kind=0/1 都吃（工单裁定），只过滤 ev.target==自身。
//   - 804 后果 = 全属性 stat_drop 循环（1234 J1 同款原语逐槽、吃免弱）。
//   - 998 后果 = grant_skill_invalid SEAL_ATTRIBUTE 窗（1110 bailuo 同款授予、
//     counts=0/rounds={1} 回合型；"命中效果失效"= SEAL 语义，RuleCenter 记账
//     register_round 起算）。
//
// 载体切片（unity 库 json_each 精确匹配 16/16=32；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   804 [rounds,降幅]：{3,1}×12/{2,1}×2/{3,2}×1/{4,1}×1（纯链载体 无息破碎 37052 [3,1] 物140）
//   998 [rounds,封属回合]：{3,2}×15/{4,2}×1（纯链载体 超能法界 26797 [3,2] 属）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 804/998 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1109（STAT_BOOSTS_LOST watcher，J16）、1234（全属性 stat_drop 循环，J1）、
//   1110（grant_skill_invalid SEAL_ATTRIBUTE，bailuo）。
// 场景回归：test/scenario/scenario_1450_boost_lost_response.cpp（1450 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 804 —— {0}回合内自强被消/被吸 → 对手全属性-{1} ──
// int_args = [owner, target, rounds, drop]（args_num=2）。
EffectResult effect_804_boost_lost_all_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 804, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 804, args,
                     "804 布局=[owner,target,回合,降幅]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int drop = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     804, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && drop > 0, 804, args, "需要 回合/降幅 > 0");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // "被消除**或吸取**"：kind=0/1 都吃（1109 只吃 kind=0，此处刻意宽于它）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, drop](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->stat_drop) {
                return;
            }
            for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
                (void)g_core->stat_drop(c, opp, slot, drop, self);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 998 —— {0}回合内自强被消/被吸 → {1}回合对手属性技命中失效 ──
EffectResult effect_998_boost_lost_seal_attr(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 998, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 998, args,
                     "998 布局=[owner,target,回合,封属回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int seal_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     998, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && seal_rounds > 0, 998, args, "需要 回合/封属回合 > 0");
    // 回合型封属窗（counts=0 窗口票；1110 counts 形同款授予、RuleCenter 记账）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, seal_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;
            }
            c->grant_skill_invalid(self, opp, /*source_slot=*/-1, /*effect_id=*/998,
                                   SealKind::SEAL_ATTRIBUTE, /*counts=*/0,
                                   /*rounds=*/seal_rounds, /*penetrable=*/false,
                                   EffectScope::ON_STAGE);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(804, effect_804_boost_lost_all_down)
SKILL_EFFECT(998, effect_998_boost_lost_seal_attr)
