// J 组 J111 · 切换追击/伤害反馈族 —— 1779×15 / 855×13（2026-10-03，worktree ../agents/j111）
//
// 家族语义（effect_info 官方模板即权威）：
//   1779（arg_count=2）"对手下{0}次主动切换或死亡切换精灵时，在场精灵吸取对手下只
//        出场精灵最大体力的1/{1}"（含死亡切换——与 J98 1702 的"仅主动"面区分）
//   855（arg_count=1）"将下次受到的伤害{0}%反馈给对手"
//
// 实现口径：
//   - 1779 = 单 EVENT_ENTER_STAGE watcher（perform_switch 统一漏斗：主动中切与死亡
//     换宠**都 emit**（battleFsm.cpp:626），ev.actor=登场方——恰好覆盖"主动或死亡
//     切换"两语义，无需 SWAP）；ev.actor==对手 → 计数 ≤{0} → drain_hp(对手,
//     1/{1})（"在场精灵"=己方在场位吸取新登场的对手）。计数耗尽即止。
//   - 855 = EVENT_TAKE_DAMAGE watcher（红伤 NORMAL、一次性旗）→
//     fixed_damage(ev.actor, amount×{0}/100)（反噬给攻击者，J95 1073 变体）。
//
// 载体切片（unity 库 json_each 精确匹配 15/13=28；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1779 [次数,分母]：分散（无纯链——场景载体动工挑短链筛哑兄弟）
//   855 [反馈%]：分散（纯链 炽霆灼击 36478）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1779/855 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：J98 1702（ENTER_STAGE/SWAP 事件面+入队序教训）、J95 1073（TAKE_DAMAGE
//   一次性反噬）、drain_hp 原语（J42/J59）。
// 场景回归：test/scenario/scenario_2050_switch_drain_reflect.cpp（2050 号段，一文件两宏）。

#include "common.h"

namespace {

constexpr int k1779CntBase = 990177900;   // +owner：剩余切换计数

// ── 1779 —— 对手下{0}次切换 → 在场精灵吸取对手下只 1/{1} ──
// int_args = [owner, target, count, denom]（args_num=2）。
EffectResult effect_1779_switch_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1779, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1779, args,
                     "1779 布局=[owner,target,次数,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1779, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count > 0 && denom > 0, 1779, args, "需要 次数/分母 > 0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1779CntBase + owner] = count;
    // ENTER_STAGE：主动中切与死亡换宠都 emit（battleFsm:626 统一漏斗）→ 恰好覆盖
    // "主动或死亡切换"。ev.actor==对手 = 对手侧有精灵登场。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, count, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp || !g_core || !g_core->drain_hp) {
                return;
            }
            const int key = k1779CntBase + owner;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) <= 0) {
                return;   // 次数耗尽
            }
            it->second = any_int_or(it->second) - 1;
            const auto dr = g_core->drain_hp(c, owner, opp, denom);
            fprintf(stderr, "[PROBE-1779] drain rc=%d target_hp=%d rc_now=%d\n",
                    static_cast<int>(dr), c->getPet(opp).hp, c->roundCount);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 855 —— 将下次受到的伤害{0}%反馈给对手 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_855_damage_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 855, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 855, args,
                     "855 布局=[owner,target,反馈%]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     855, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 855, args, "需要 反馈% > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    constexpr int k855ArmBase = 990855000;   // +owner：下次受击一次性旗
    ctx->plugin_storage[k855ArmBase + owner] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || !g_core || !g_core->fixed_damage) {
                return;
            }
            constexpr int k855Arm = k855ArmBase;   // （编译期常量别名）
            const int key = k855Arm + owner;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 已触发（一次性）
            }
            // "受到的伤害"= 对手打来的红伤（535/817 判据族）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0
                || ev.actor < 0 || ev.actor > 1 || ev.actor == owner) {
                return;
            }
            c->plugin_storage.erase(it);
            (void)g_core->fixed_damage(c, ev.actor, ev.amount * pct / 100);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1779, effect_1779_switch_drain)
SKILL_EFFECT(855, effect_855_damage_reflect)
