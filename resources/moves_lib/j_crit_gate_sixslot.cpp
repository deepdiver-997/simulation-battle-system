// J 组 J48 · 致命门六槽族 —— 1 宏/22 载体（2026-10-02，worktree ../agents/j48）
//
// 家族语义（effect_info 模板即权威）：
//   732 [6] 本回合若没有打出致命一击则{0}——{0} = 语义序六槽变化向量
//   （实载分布：全+1 ×11、攻+2 ×4、特攻+1 ×2、攻+1速+1 ×3、特攻+2速+2 ×1、攻+1特防+1...）
//
// 实现口径：
//   - AFTER_ACTION 双侧注册（桶只跑行动方，52 教训）一次判定：!crit_happened[owner]
//     （731 同源旗：query_usage ①.5 在 ON_SKILL_HIT 掷、每攻击先清——AFTER_ACTION 时
//     反映本回合）→ 语义序 kStatMap{0,2,1,3,4,5} 逐槽变化（J3 1823 同构：
//     负值 stat_drop 查免弱 actor=属主、正值 stat_change、0 跳过）。一次性判定。
//
// 载体切片（unity 库 json_each 精确匹配 22；side_effect.arg_count 切分全载体自洽，
// 脚本核算 2026-10-02；变体分布：[1,1,1,1,1,1]×11、[2,0,0,0,0,0]×4、[1,0,0,0,1,0]×3、
// [0,0,1,0,0,0]×2、[0,0,2,0,2,0]×1、[1,0,0,0,0,0]×1）：
//   测试载体 32430 噬梦曲 [1,1,1,1,1,1]（特150）
//
// 动工前置三查（2026-10-02）：①内核 grep 732 无硬编码 ②custom_effect_overrides 无声明
// ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1340_crit_gate_sixslot.cpp（号段 1340-1349）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（J3 1823 kStatMap 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

// ── 732 的 AFTER_ACTION 判定体 —— payload={owner, target, 六槽×6}。
EffectResult j48_732_crit_gate_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 8) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->crit_happened[owner]) {
        return EffectResult::kOk;   // "本回合打出了致命一击"→ 不触发
    }
    if (!g_core) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        if (delta == 0) {
            continue;
        }
        const int slot = kStatMap[i];
        if (delta < 0) {
            if (g_core->stat_drop) {
                (void)g_core->stat_drop(ctx, target, slot, -delta, owner);   // 对手下降查免弱
            }
        } else if (g_core->stat_change) {
            (void)g_core->stat_change(ctx, owner, slot, delta);              // 自身提升必成
        }
    }
    return EffectResult::kOk;
}

// ── 732 —— 本回合若没有打出致命一击则{0}（语义序六槽变化）──
// int_args = [owner, target, 六槽×6]（args_num=6）。
EffectResult effect_732_no_crit_sixslot(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 732, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 732, args,
                     "732 布局=[owner,target,六槽×6]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     732, args, "需要 owner/target ∈ {0,1}");
    const std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 732, /*rounds=*/1, payload,
                              &j48_732_crit_gate_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(732, effect_732_no_crit_sixslot)
