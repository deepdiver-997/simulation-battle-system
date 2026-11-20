#include <utils/sbs_rand.h>
// J 组 J175 · 威力随机三胎+未受击门 —— 61×12 / 819×11（2026-10-04，worktree ../agents/j175）
//
// 家族语义（effect_info 官方模板即权威）：
//   61  [0] 威力随机，随机范围50~150
//   819 [2] 出手时本回合若未受到攻击伤害，则附加自身{0}值{1}%的百分比伤害，并恢复等量体力
//
// 实现口径：
//   - 61 = 118/70 第三胎（J159/J173 同通道）：skill_power_view = 50 + sbs_rand()%101
//     （50..150 绝对置，常数写死文案）。
//   - 819 = EVENT_TAKE_DAMAGE watcher（duration=1 本回合，ev.target==self 且
//     damage_kind==NORMAL——"受到攻击伤害"=红伤，粉/真不算）置旗（roundCount 键）；
//     SKILL_EFFECT 读旗：未受击 → deal_pink(自身{0}值面板×{1}%, PERCENT_VALUE) +
//     按 dealt heal（J87 1659 口径）。⚠️ {0} = NumericalPropertyIndex 直读
//     （调和之钥 36905 [4]=速度值 / 恶兆烈风 36952 [0]=攻击值 4399 对定；
//     ⚠️ 风暴离子电流 17953 切片 [2,50] 与 4399"攻击力"冲突——按官方码落 DEFENSE，
//     记档 4399 措辞疑误）。
//
// 载体切片（unity 库 json_each 精确匹配 12+11=23；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   61  []（零参）纯链×12（峰回路转 10484 物 / 未来射线 10747 特，全 pri0）
//   819 [属性码,pct]：{50} 全 50；{0}/{2}/{4} 分散——纯链×2（调和之钥 36905 [4,50] /
//     恶兆烈风 36952 [0,50] cat2 pri0）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 2 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：118/70（rand 威力绝对写，J159/J173）、J42 1320（TAKE_DAMAGE watcher 置旗
//   +roundCount 键）、1697（deal_pink PERCENT_VALUE）、1659/1144（按 dealt heal）。
// 场景回归：test/scenario/scenario_2770_randpower_untouched.cpp（2770 号段）。

#include "common.h"

namespace {

constexpr int k819SnapBase = 990819000;      // +owner：ROUND_START 体力快照（-1=未快照）

// ── 61 —— 威力随机，随机范围50~150（零参）──
EffectResult effect_61_random_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 61, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 61, args,
                     "61 布局=[owner,target]（on_selected 绑定）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 61, args, "需要 owner ∈ {0,1}");
    ctx->ws.skill_power_view[owner] = 50 + sbs_rand() % 101;   // 50..150 绝对置
    return EffectResult::kOk;
}

// ── 819 tick：回合开始快照体力（供"本回合是否受伤"比对）──
EffectResult j159_819_snap_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    c->plugin_storage[k819SnapBase + owner] = c->getPet(owner).hp;
    return EffectResult::kOk;
}

// ── 819 —— 未受攻击伤害 → 附加自身{0}值{1}%粉伤 + 等量回吸 ──
// int_args = [owner, target, stat_idx, pct]（args_num=2）。
// 受击门 = ROUND_START 快照比对（先手出手时快照==现值→未受击；后手且已被打→不等）。
// 记档近似：hp 差比对含治疗/非攻击伤害（官方字面仅"攻击伤害"）。
EffectResult effect_819_untouched_pink_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 819, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 819, args,
                     "819 布局=[owner,target,属性码,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int stat_idx = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     819, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(stat_idx >= 0 && stat_idx <= 4, 819, args,
                     "需要 属性码 ∈ [0,4]（5=体力）");
    EFFECT_ARG_GUARD(pct > 0, 819, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    // 常驻快照 tick（一次武装；696 形 duration=-1 TEAM——快照是 side 级）。
    const int arm_key = k819SnapBase + 100 + owner;
    if (ctx->plugin_storage.find(arm_key) == ctx->plugin_storage.end()) {
        ctx->plugin_storage[arm_key] = 1;
        Effect e(819, 0, owner, -1, EffectArgs(std::vector<int>{owner}), &j159_819_snap_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, owner,
                                                     /*duration=*/-1, ctx->roundCount);
        ce->source_id_ = 819;
        ce->scope_ = EffectScope::TEAM;
        ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    }
    const auto it = ctx->plugin_storage.find(k819SnapBase + owner);
    if (it != ctx->plugin_storage.end()) {
        const int snap = any_int_or(it->second);
        if (snap >= 0 && snap != ctx->getPet(owner).hp) {
            return EffectResult::kOk;   // 本回合已受伤（快照比对）
        }
    }
    const int base = ctx->ws.battle_attrs[owner][static_cast<NumericalPropertyIndex>(stat_idx)];
    const int pink = base * pct / 100;
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);   // 恢复等量体力（1659 口径）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(61, effect_61_random_power)
SKILL_EFFECT(819, effect_819_untouched_pink_drain)
