// J 组 J230 · 受击转化护盾族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1768 [2] 将自身下{0}次受到的攻击伤害转化为等量护盾，最多获得{1}点
//
// 实现口径（直系先例 j_taken_convert_reflect.cpp 1314）：
//   - EVENT_TAKE_DAMAGE watcher（TEAM 作用域、非 once）+ shared_ptr 计次 {0}。
//   - 只认 "攻击伤害" = DamageKind::NORMAL 红伤、amount>0（粉/真/属性伤害不转化）。
//   - 每次落地：获得量 = min(本次伤害, {1} - 已累计获得)；写 shield_bank_.add
//     （priority 0、source_id=1768、duration 0；护盾槽记账走一等公民 ShieldBank）。
//   - "最多获得{1}点" = **累计获得上限**（护盾被消耗不返还额度）；额度用尽即停止，
//     计次同步清零。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 10/10 自洽）：1768 [counts,cap] 全部 counts=1：
//   27848 圣洁岩盾 [1,300]（场景载体；链兄弟 191 自免异常/738 属性技窗，无受击转化面）/
//   27903 [1,300] / 28572 [1,300] / 28633 [1,300] / 28862 [1,300] / 28923 [1,300] /
//   29073 [1,300] / 29235 [1,300] / 36543 [1,300] / 37188 [1,400]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1768 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5610_taken_to_shield.cpp（号段 5610-5619）。

#include "common.h"

#include <algorithm>
#include <memory>

namespace {

// ── 1768 —— 下{0}次受到的攻击伤害转化为等量护盾（累计上限{1}）──
// int_args = [owner, target, counts, cap]（args_num=2）。
EffectResult effect_1768_taken_attack_to_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1768, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1768, args,
                     "1768 布局=[owner,target,counts,cap]");
    const int self = args.int_args[0];
    const int counts = args.int_args[2];
    const int cap = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1768, args, "owner 越界");
    EFFECT_ARG_GUARD(counts > 0 && cap > 0, 1768, args, "需要 counts/cap > 0");
    auto remaining = std::make_shared<int>(counts);
    auto gained = std::make_shared<int>(0);   // 累计获得（护盾被消耗也不返还额度）
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, cap, remaining, gained](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || *remaining <= 0) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "受到的攻击伤害" = 红伤；粉/真/属性伤害不转化
            }
            const int room = cap - *gained;
            if (room <= 0) {
                *remaining = 0;
                return;
            }
            const int add = std::min(ev.amount, room);
            --*remaining;
            *gained += add;
            c->getPet(self).shield_bank_.add(/*priority=*/0, add, /*source_id=*/1768,
                                             /*duration=*/0);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1768, effect_1768_taken_attack_to_shield)
