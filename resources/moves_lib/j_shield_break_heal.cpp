// J 组 J47 · 盾破回血族 —— 1 宏/19 载体（2026-10-02，worktree ../agents/j47）
//
// 家族语义（effect_info 模板即权威）：
//   1753 [2] 获得{0}点护盾，护盾消失时恢复自身最大体力的1/{1}
//
// 实现口径：
//   - shield_bank_.add(priority=0, {0}点, source=1753)（2005/1013 直系）。
//   - "护盾消失时恢复"= EVENT_SHIELD_BROKEN watcher（emit {actor=破盾攻击方,
//     target=盾主}——battleContext.cpp:330；filter ev.target==self）→ heal_max_hp_fraction
//     (1/{1})。⚠️ 事件无盾 source 归属 → **一次性** storage 旗（获盾置位、首次盾破回血后
//     清除；本族载体单盾单回，多盾场景的逐盾归属记档待事件扩展）。
//
// 载体切片（unity 库 json_each 精确匹配 19；side_effect.arg_count 切分全载体自洽，
// 脚本核算 2026-10-02；样例 25493 [400,3]、25638 [400,3]）：
//
// 动工前置三查（2026-10-02）：①内核 grep 1753 无硬编码 ②custom_effect_overrides 无声明
// ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1330_shield_break_heal.cpp（号段 1330-1339）。

#include "common.h"

namespace {

constexpr int k1753ArmedBase = 990175300;   // +owner：回血一次性旗

// ── 1753 —— 获得{0}点护盾，护盾消失时恢复自身最大体力的1/{1} ──
// int_args = [owner, target, shield, denom]（args_num=2）。
EffectResult effect_1753_shield_break_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1753, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1753, args,
                     "1753 布局=[owner,target,盾点,分母]");
    const int self = args.int_args[0];
    const int shield = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1753, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(shield > 0 && denom > 0, 1753, args, "需要 盾点/分母 > 0");
    ctx->getPet(self).shield_bank_.add(/*priority=*/0, shield, /*source_id=*/1753,
                                       /*duration=*/0);
    // 一次性回血旗：获盾置位，首次盾破（任意来源）→ 回 max_hp 1/{1} → 清。
    ctx->plugin_storage[k1753ArmedBase + self] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->heal_amount) {
                return;
            }
            const int key = k1753ArmedBase + self;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 旗未置/已消耗
            }
            c->plugin_storage.erase(key);
            (void)heal_max_hp_fraction(c, self, denom);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1753, effect_1753_shield_break_heal)
