// J 组 J254 · 能力段数固伤 + PP 门威力族 —— 2 宏/16 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1678（args_num=1）"附加自身防御和特防能力提升总段数x{0}的固定伤害"
//   1857（args_num=2）"技能威力提升{0}%，对手任意1项技能PP值小于{1}点则提升效果翻倍"
//
// 实现口径：
//   - 1678 = SKILL_EFFECT 读本体能力等级：只数**正**段（槽 2 防御 / 槽 3 特防），
//     负段不算（"能力提升总段数"字面）。amount = (max(0,def)+max(0,spdef)) × {0}
//     → fixed_damage(对手)（J252 固定伤害原语口径，吃护罩/固定抗性/事件）。
//     审计槽 991678100+owner = 本局固定伤害点数（0 = 无正段）。
//   - 1857 = SKILL_EFFECT 写 ws.skill_power_view（102 变威力通道，J253 638 同通道）：
//     有效 pct = 对手任一技能**当前 pp < {1}** ? {0}×2 : {0}；
//     view += view × 有效 pct / 100（view>0 才写，845 同守卫）。
//     {0} 在位即消费（模板为"提升{0}%"，无显式 0 关闸语义；0 时审计仍落 0）。
//     审计槽 991857100+owner = (门过?1000:0) + 有效 pct。
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算）：
//   1678 [系数]：36271 倾覆之势 [100]（纯链）、35408 铁甲冲顶 [30]（纯链）、
//     36211 龙言之息 [30]（纯链）、37270 一锯两段 [50]（纯链）、34962 春沐群芳醉 [40]、
//     35065 蔽日浮云烟 [50]、37855 风尘沙爆 [50]、37983 龙化虾镰击 [70]。
//   1857 [pct,阈值]：36500 迅镰击 [50,2]、36121 夜逐 [50,2]、36130 孤胆映青釭 [50,2]、
//     36690 激流葬命斩 [25,2]、37109 净眸清灵语 [50,2]、37409 帆啸 [50,2]、
//     38156 算式入侵 [60,2]、38251 灵足烈贯 [80,3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1678/1857 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含各 worktree）
//   无 SKILL_EFFECT(1678/1857) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5850_boost_level_power.cpp（号段 5850-5859）。

#include "common.h"

#include <algorithm>

namespace {

constexpr int k1678AuditBase = 991678100;   // +owner：本局固定伤害点数
constexpr int k1857AuditBase = 991857100;   // +owner：(门过?1000:0)+有效 pct

// ── 1678 —— 附加自身防御和特防能力提升总段数x{0}的固定伤害 ──
// int_args = [owner, target, 系数]（args_num=1）。
EffectResult effect_1678_def_spdef_boost_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1678, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1678, args,
                     "1678 布局=[owner,target,系数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int coef = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1678, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(coef > 0, 1678, args, "需要 系数 > 0");

    // 只数正段：槽 2=防御、槽 3=特防（能力等级槽序见常识速查 §三）。
    const int def_levels = std::max(0, ctx->ability_levels[owner][2]);
    const int spdef_levels = std::max(0, ctx->ability_levels[owner][3]);
    const int amount = (def_levels + spdef_levels) * coef;
    ctx->plugin_storage[k1678AuditBase + owner] = amount;
    if (amount > 0 && g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

// ── 1857 —— 技能威力提升{0}%，对手任意1项技能PP值小于{1}点则提升效果翻倍 ──
// int_args = [owner, target, pct, 阈值]（args_num=2）。
EffectResult effect_1857_pp_gate_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1857, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1857, args,
                     "1857 布局=[owner,target,pct,阈值]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1857, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0, 1857, args, "需要 阈值 > 0");

    // 对手任意 1 项技能当前 PP 小于阈值 → 门过（严格小于）。
    bool low_pp = false;
    for (const Skills& s : ctx->getPet(target).skills) {
        if (s.pp >= 0 && s.pp < threshold) {
            low_pp = true;
            break;
        }
    }
    const int effective_pct = low_pp ? pct * 2 : pct;
    ctx->plugin_storage[k1857AuditBase + owner] = (low_pp ? 1000 : 0) + effective_pct;
    const int view = ctx->ws.skill_power_view[owner];
    if (view > 0 && effective_pct != 0) {
        ctx->ws.skill_power_view[owner] = view + view * effective_pct / 100;   // 102 通道
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1678, effect_1678_def_spdef_boost_fixed)
SKILL_EFFECT(1857, effect_1857_pp_gate_power)
