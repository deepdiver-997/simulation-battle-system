#include <utils/sbs_rand.h>
// J 组 J198 · 击败→下只随机PP归零族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   1663 [1] 当回合击败对手则令对手下只登场精灵随机{0}个技能PP值归零
//
// 实现口径：
//   - 两段式 TEAM watcher（跨死亡换宠稳定；1882 死亡旗→下只登场直系）：
//     ① EVENT_DEATH：actor==self（最后一刀归因）且 target==opp（对手在场宠阵亡）
//        → 置 pending=plugin_storage[k1663PendingBase+opp]={0}；
//     ② EVENT_ENTER_STAGE：**actor=登场方**（battleFsm perform_switch 的 emit 口径，
//        target=另一方）→ actor==opp 时消费 pending，随机不重复抽 {0} 个技能槽
//        走 pp_zero_slot 原语（f_misc_family 716 同款；直写 pp 会绕过 EVENT_PP_REDUCED）。
//   - 审计槽 plugin_storage[k1663AuditBase+opp] = 本次实际清零槽数（无随机断言用）。
//   - pending 在登场时清零，只作用于"下只"；下次击败再武装。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1663 [清零槽数]：36932 极霆威压 纯链 [2]（物 140，场景载体）、
//        37371 炼·邃密 [3]、38301 骑枪掷投 [2]、35880 影僧苦行录 [2]…
//        （9/9 自洽；{0} 全为 2 或 3）
//
// 动工前置三查（2026-10-22）：①内核 grep 1663 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(1663)——真缺口。
// 场景回归：test/scenario/scenario_5210_defeat_next_pp_zero.cpp（号段 5210-5219）。

#include "common.h"

#include <primitives/battle_primitives.h>

#include <numeric>
#include <vector>

namespace {

constexpr int k1663PendingBase = 990166300;   // +side：待生效清零槽数
constexpr int k1663AuditBase = 990166301;     // +side：本次实际清零槽数

// ── 1663 —— 当回合击败对手则令对手下只登场精灵随机{0}个技能PP值归零 ──
// int_args = [owner, target, 槽数]（args_num=1）。
EffectResult effect_1663_defeat_next_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1663, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1663, args,
                     "1663 布局=[owner,target,槽数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1663, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count > 0, 1663, args, "需要 槽数 > 0");

    // ① 击败探针：只认"自身造成的"对手阵亡（EVENT_DEATH actor=最后一刀来源）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, opp, count](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp || ev.actor != self) {
                return;   // 非自身击败/非对手侧阵亡 → 不武装
            }
            c->plugin_storage[k1663PendingBase + opp] = count;
        },
        WatcherScope::TEAM);

    // ② 下只登场：actor=登场方（不是 target！battleFsm 的 EVENT_ENTER_STAGE 口径）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;   // 只认对手方登场
            }
            auto it = c->plugin_storage.find(k1663PendingBase + opp);
            if (it == c->plugin_storage.end()) {
                return;
            }
            const int pending = any_int_or(it->second);
            if (pending <= 0) {
                return;
            }
            c->plugin_storage[k1663PendingBase + opp] = 0;   // 只作用于"下只"
            if (!g_core || !g_core->pp_zero_slot) {
                return;
            }
            const int slots = static_cast<int>(c->getPet(opp).skills.size());
            if (slots <= 0) {
                return;
            }
            std::vector<int> idx(static_cast<std::size_t>(slots));
            std::iota(idx.begin(), idx.end(), 0);
            const int n = std::min(pending, slots);
            int landed = 0;
            for (int i = 0; i < n; ++i) {
                const int j = i + (slots - i > 1 ? sbs_rand() % (slots - i) : 0);
                std::swap(idx[static_cast<std::size_t>(i)],
                          idx[static_cast<std::size_t>(j)]);
                if (g_core->pp_zero_slot(c, opp, idx[static_cast<std::size_t>(i)], self)
                    == PpReduceResult::SUCCESS) {
                    ++landed;
                }
            }
            c->plugin_storage[k1663AuditBase + opp] = landed;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1663, effect_1663_defeat_next_pp_zero)
