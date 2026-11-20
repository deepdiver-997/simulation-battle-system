// J 组 J190 · 强化翻倍/镜像族 —— 2 宏/18 载体（2026-10-22，worktree ../agents/j190）
//
// 家族语义（effect_info 模板即权威）：
//   1177 [0] 自身处于能力提升状态则使自身能力提升状态翻倍
//   1856 [1] 全属性+{0}，若强化前自身处于能力下降状态则额外获得等同的能力提升状态
//
// 实现口径：
//   - 1177 = 门（任一槽正等级）→ 逐**正槽**再 stat_change(+当前等级)
//     （J90 622 的"强化效果翻倍"取法：+2→+4，AT_CAP 自然封顶；负/零槽不动）。
//   - 1856 = ①强化前逐槽快照负等级；②六槽各 +{0}（全属性，含命中槽 5）；
//     ③快照中**负槽**再补 abs(负等级)（"额外获得等同的能力提升状态"，逐槽对等）。
//     例：-2 的槽 → -2+{0}+2 = +{0}；无负槽时不补。
//   - stat_change 是本体/视图同步真原语（不吃免弱；正增益正常落地）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1177 0 参：34794 极寒冲击（纯链）、35273 色香味俱全、35841 园艺插花术…（9/9）
//   1856 [增量]：28083 垂宇龙吟 [1]、28386 无米之炊 [1]、29211 浩域森罗 [1]…（9/9 全 1）
//
// 动工前置三查（2026-10-22）：①内核 grep 1177/1856 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5120_boost_double_mirror.cpp（号段 5120-5129）。

#include "common.h"

namespace {

constexpr int kAbilitySlots = 6;   // 0攻 1特攻 2防 3特防 4速 5命中
constexpr int kAbilityCap = 6;     // 能力等级 ±6 钳制（stat_change 不做部分钳位）

// ── 1177 —— 自身处于能力提升状态则使自身能力提升状态翻倍 ──
// int_args = [owner, target]（0 参效果，装载器注入）。
EffectResult effect_1177_double_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1177, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1177, args,
                     "1177 布局=[owner,target]（0 参效果）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1177, args, "owner 越界");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    bool any_positive = false;
    for (int slot = 0; slot < kAbilitySlots; ++slot) {
        if (ctx->ability_levels[self][slot] > 0) {
            any_positive = true;
            break;
        }
    }
    if (!any_positive) {
        return EffectResult::kOk;   // 门：自身处于能力提升状态
    }
    // 逐正槽 +当前等级（翻倍）；先读值再加，避免同槽自读自写歧义。
    // ⚠️ stat_change 对越界增量返回 AT_CAP 且**整段不落地**（不做部分钳位）——
    //    翻倍必须自己把增量钳到 +6 余量内（+5 → +1 → 6，而非 +5 → AT_CAP 留在 5）。
    for (int slot = 0; slot < kAbilitySlots; ++slot) {
        const int level = ctx->ability_levels[self][slot];
        const int room = kAbilityCap - level;
        if (level > 0 && room > 0) {
            (void)g_core->stat_change(ctx, self, slot, std::min(level, room));
        }
    }
    return EffectResult::kOk;
}

// ── 1856 —— 全属性+{0}，若强化前自身处于能力下降状态则额外获得等同的能力提升状态 ──
// int_args = [owner, target, 增量]（args_num=1）。
EffectResult effect_1856_all_boost_mirror_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1856, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1856, args,
                     "1856 布局=[owner,target,增量]");
    const int self = args.int_args[0];
    const int delta = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1856, args, "owner 越界");
    EFFECT_ARG_GUARD(delta > 0, 1856, args, "需要 增量 > 0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    int before[kAbilitySlots];
    bool any_negative = false;
    for (int slot = 0; slot < kAbilitySlots; ++slot) {
        before[slot] = ctx->ability_levels[self][slot];
        if (before[slot] < 0) {
            any_negative = true;
        }
    }
    for (int slot = 0; slot < kAbilitySlots; ++slot) {
        (void)g_core->stat_change(ctx, self, slot, delta);
    }
    if (any_negative) {
        for (int slot = 0; slot < kAbilitySlots; ++slot) {
            if (before[slot] < 0) {
                const int room = kAbilityCap - ctx->ability_levels[self][slot];
                const int extra = std::min(-before[slot], room);
                if (extra > 0) {
                    (void)g_core->stat_change(ctx, self, slot, extra);
                }
            }
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1177, effect_1177_double_boosts)
SKILL_EFFECT(1856, effect_1856_all_boost_mirror_drop)
