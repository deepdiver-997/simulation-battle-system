// J 组 J98 · 切换回血族 —— 1702×15（2026-10-03，worktree ../agents/j98）
//
// 家族语义（effect_info 官方模板即权威）：
//   1702（arg_count=1）"自身主动切换下场后令己方下只精灵出战时恢复{0}点体力"
//
// 实现口径（K3 330 / J31 934 事件面直系）：
//   - 单 watcher：EVENT_SWAP（**仅主动切换 emit**——死亡换宠不发，内核
//     perform_switch 注释明示"死亡换宠语义上是强制替换"；ev.actor=切换方）→
//     heal_amount(自身, {0})——SWAP 派发时 perform_switch 已完成、新宠就位，
//     时点即"出战时"。（初版双 watcher ENTER+SWAP 因入队序 ENTER 先于 SWAP 而
//     反序失效，本次实证后简化。）
//   - 模板无窗口字样（args 仅点数）→ 永久行为（施放后整场生效）。
//
// 载体切片（unity 库 json_each 精确匹配 15；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-03）：
//   1702 [点数]：{300}×10/{200}×4/{100}
//     （场景载体 牵风引霞 36703 [1779✗,1702]|[1,3,300]——1779 未实现哑）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1702 无硬编码 ②effect_meta
// 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1600（EVENT_SWAP 仅主动切换口径，crawford）、perform_switch（ENTER_STAGE
//   统一 emit，battleFsm.cpp:626）、K3 330（死亡继承事件面）。
// 场景回归：test/scenario/scenario_1880_switch_heal.cpp（1880 号段，一文件一宏）。

#include "common.h"

namespace {

// ── 1702 —— 自身主动切换下场后令己方下只精灵出战时恢复{0}点体力 ──
// int_args = [owner, target, points]（args_num=1）。
EffectResult effect_1702_switch_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1702, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1702, args,
                     "1702 布局=[owner,target,点数]");
    const int owner = args.int_args[0];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1702, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 1702, args, "需要 点数 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    // 单 watcher：EVENT_SWAP（仅主动切换 emit；死亡换宠不发——内核口径）。
    // 事件派发序：perform_switch（新宠已就位）→ ENTER_STAGE 入队 → SWAP 入队，
    // SWAP 后投递 → 此时 heal_amount 落在**已登场的下只精灵**上，时点即"出战时"。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, points](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            (void)g_core->heal_amount(c, owner, points);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1702, effect_1702_switch_heal)
