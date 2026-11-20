// J 组 J187 · 主动切换护盾族 —— 1 宏/9 载体（2026-10-22，worktree ../agents/j187）
//
// 家族语义（effect_info 模板即权威）：
//   1937 [1] 自身主动切换下场后令己方下只精灵出战时获得{0}点护盾
//
// 实现口径（J98 1702 切换回血直系镜像）：
//   - SKILL_EFFECT 时点注册 EVENT_SWAP watcher（仅"主动切换"emit；死亡换宠不发——
//     内核 perform_switch 注释明示"死亡换宠语义上是强制替换"）。
//   - SWAP 派发时 perform_switch 已完成、新宠就位 → shield_bank_.add(0,{0}) 落在
//     **己方当前在场（下只）精灵**上，时点即"出战时"。
//   - 模板无窗口字样（args 仅点数）→ 永久行为（施放后整场生效；每次主动切换都触发，
//     1702 同口径）。技能每次使用各注册一条 watcher；同回合多次使用会叠加待触发条目
//     （官方无"仅一次"措辞，记档为永久型语义）。
//   - 护盾槽 = shield_bank_（红伤吸收），与护罩 hood_bank_ 独立。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1937 [盾点]：28881 织梦 [240]（链 [1911,1937]|[240,240]）、28358 龙缘之息、
//        29177 绝不宽容、37106 繁馨一隅…（9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1937 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5020_switch_shield.cpp（号段 5020-5029）。

#include "common.h"

namespace {

// ── 1937 —— 自身主动切换下场后令己方下只精灵出战时获得{0}点护盾 ──
// int_args = [owner, target, 盾点]（args_num=1）。
EffectResult effect_1937_switch_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1937, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1937, args,
                     "1937 布局=[owner,target,盾点]");
    const int owner = args.int_args[0];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1937, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 1937, args, "需要 盾点 > 0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_SWAP, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, points](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner) {
                return;
            }
            // SWAP 时 perform_switch 已完成：getPet(owner) = 己方下只出战精灵。
            c->getPet(owner).shield_bank_.add(/*priority=*/0, points,
                                              /*source_id=*/1937, /*duration=*/0);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1937, effect_1937_switch_shield)
