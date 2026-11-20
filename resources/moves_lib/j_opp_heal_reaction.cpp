// J 组 J282 · 对手回血反应固伤窗族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1071 [3] "{0}回合内若对手恢复体力（药剂恢复除外），则{1}回合内自身攻击附加{2}点固定伤害"
//
// 实现口径（两段窗口，均用事件面）：
//   ① 外窗（{0} 回合）= EVENT_HEAL_RESTORED watcher（TEAM，duration={0}）：
//      `ev.target == 对手` 即命中（事件只在实际恢复 > 0 时发，"封回血吞掉"不算；
//      **药剂恢复是 robot.use_medicine 直写本体、不走 heal_impl，天然不发事件**——
//      官方"药剂恢复除外"由事件面直接满足，不需要额外过滤）。
//   ② 内窗（{1} 回合）= 每次外窗触发时注册 EVENT_TAKE_DAMAGE watcher
//      （TEAM，duration={1}）：`ev.actor == 自身 && ev.target == 对手 &&
//      damage_kind == NORMAL`（只认攻击红伤）→ `fixed_damage(对手, {2})` 附加固定伤害。
//      ⚠️ 固定伤害事件 kind=FIXED → 不会自触发（防递归）。
//      ⚠️ 多次回血 = 多条内窗并存（每条各附加 {2} 点）——按官方字面"每次恢复体力则…"。
//   - 审计槽：991071000+owner = 外窗触发次数；991071100+owner = 内窗附加次数。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 10/10 自洽）：
//   1071 [rounds, inner_rounds, fixed]：25193 魂牵光影 [4,2,350]、26129 黄金羽翼 [3,3,300]、
//     26321 火焰跃动 [3,2,300]（场景载体）、26354 婆罗魔咒 [4,2,400]、26505 钢铁之翼
//     [3,3,300]、26560 陵墓守护 [3,2,300]、26817 粉色回忆 [3,3,300]、26880 海绵蛋糕
//     [3,2,300]、27235 凤仙信风 [3,3,300]、36502 回旋镰刃 [3,2,250]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1071 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与
//   各 agents worktree）无 SKILL_EFFECT(1071) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6150_opp_heal_reaction.cpp（号段 6150-6159）。

#include "common.h"

namespace {

constexpr int kTriggerAuditBase = 991071000;   // +owner：外窗触发次数
constexpr int kAppendAuditBase = 991071100;    // +owner：内窗附加次数

// ── 1071 —— {0}回合内对手回血 → {1}回合内自身攻击附加{2}点固定伤害 ──
// int_args = [owner, target, rounds, inner_rounds, fixed]（args_num=3）。
EffectResult effect_1071_opp_heal_reaction(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1071, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1071, args,
                     "1071 布局=[owner,target,外窗回合,内窗回合,固定伤害]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int inner_rounds = args.int_args[3];
    const int fixed = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1071, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && inner_rounds > 0, 1071, args, "窗回合须 > 0");
    EFFECT_ARG_GUARD(fixed >= 0, 1071, args, "固定伤害须 >= 0");
    if (fixed == 0) {
        return EffectResult::kOk;   // 显式 0 档在位即消费：登记但不产生附加
    }

    ctx->plugin_storage[kTriggerAuditBase + self] = 0;
    ctx->plugin_storage[kAppendAuditBase + self] = 0;
    ctx->event_center_.register_watcher(
        EventType::EVENT_HEAL_RESTORED, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, inner_rounds, fixed](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp) {
                return;   // 只认"对手恢复体力"
            }
            const auto it = c->plugin_storage.find(kTriggerAuditBase + self);
            c->plugin_storage[kTriggerAuditBase + self] =
                (it == c->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
            c->event_center_.register_watcher(
                EventType::EVENT_TAKE_DAMAGE, self, c->roundCount, inner_rounds,
                /*once=*/false,
                [self, opp, fixed](BattleContext* c2, const BattleEvent& e2) {
                    if (!c2 || e2.actor != self || e2.target != opp) {
                        return;
                    }
                    if (e2.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                        return;   // 只认自身攻击红伤；固定伤害事件 kind=FIXED 不会自触发
                    }
                    const auto i2 = c2->plugin_storage.find(kAppendAuditBase + self);
                    c2->plugin_storage[kAppendAuditBase + self] =
                        (i2 == c2->plugin_storage.end() ? 0 : any_int_or(i2->second)) + 1;
                    if (g_core && g_core->fixed_damage) {
                        (void)g_core->fixed_damage(c2, opp, fixed);
                    }
                },
                WatcherScope::TEAM);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1071, effect_1071_opp_heal_reaction)
