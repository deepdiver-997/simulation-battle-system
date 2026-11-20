// J 组 J68 · PP 门吸取族 —— 1450×16 / 1632×8（2026-10-03，worktree ../agents/j68）
//
// 家族语义（effect_info 官方模板即权威）：
//   1450（arg_count=2）"吸取对手{0}点体力，若对手任意1项技能PP值小于{1}点则吸取效果翻倍"
//   1632（arg_count=3）"吸取对手{0}点体力，若对手任意1项技能PP值小于{1}点则额外吸取{2}点体力"
//
// 实现口径：
//   - 吸取 = drain_points 定点吸取（1022 J4 同款：fixed_damage 落点 + 按实际扣血
//     heal 回吸——护罩/锁血场合按 dealt 回，不凭空回）。两模板均**无吸取成功门**
//     （模板无"吸取成功"字样，直吸；区别于 1022 的消强门）。
//   - PP 门 = 遍历对手技能槽 `s.pp >= 0 && s.pp < {1}` 任一成立（pp=-1 无限 PP
//     不算"小于"——anomaly_channel total_pp / crawford pp>=0 同守卫口径）。
//   - 1450 翻倍 = 再吸取一次（同点数再 drain_points 一遍，964/J54 977 同口径——
//     对手血量充足时等价于 ×2，血量不足时按 dealt 封顶）。
//   - 1632 额外 = 门槛成立再 drain_points({2}) 一遍。
//
// 载体切片（unity 库 json_each 精确匹配 16/8=24；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1450 [点数,门槛]：{250,2}×4/{200,2}×3/{300,2}×3/{200,3}×2/…
//     （最短链载体 灵叶箭矢 33505 [945✗,1450]|[150,2] 特85——945 未实现哑）
//   1632 [点数,门槛,额外]：{300,2,150}×2/{200,2,100}/{250,2,100}/…
//     （最短链载体 圣意屠魔诀 34677 [1632,984✗]|[250,2,100] 特150——984 未实现哑）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1450/1632 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1022（drain_points 通道，J4）、crawford 656（pp>=0 守卫读槽）、
//   964/977（翻倍=再吸取口径）。
// 场景回归：test/scenario/scenario_1550_pp_drain.cpp（1550 号段，一文件两宏）。

#include "common.h"

namespace {

// 定点吸取（1022 同款本文件自备份）：fixed_damage 落点 + 按实际扣血回吸。
void j68_drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount || !ctx) {
        return;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
}

// PP 门：对手任一技能槽当前 PP < threshold（pp=-1 无限 PP 不算"小于"）。
bool j68_any_pp_below(BattleContext* ctx, int opp, int threshold) {
    if (!ctx || opp < 0 || opp > 1 || threshold <= 0) {
        return false;
    }
    for (const Skills& s : ctx->getPet(opp).skills) {
        if (s.pp >= 0 && s.pp < threshold) {
            return true;
        }
    }
    return false;
}

// ── 1450 —— 吸取对手{0}点体力，若对手任意1项技能PP值小于{1}点则吸取效果翻倍 ──
// int_args = [owner, target, points, threshold]（args_num=2）。
EffectResult effect_1450_pp_gate_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1450, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1450, args,
                     "1450 布局=[owner,target,点数,门槛]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    const int threshold = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1450, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0 && threshold > 0, 1450, args, "需要 点数/门槛 > 0");
    j68_drain_points(ctx, owner, opp, points);
    if (j68_any_pp_below(ctx, opp, threshold)) {
        j68_drain_points(ctx, owner, opp, points);   // "翻倍"=再吸取一次（977 口径）
    }
    return EffectResult::kOk;
}

// ── 1632 —— 吸取对手{0}点体力，若对手任意1项技能PP值小于{1}点则额外吸取{2}点体力 ──
// int_args = [owner, target, points, threshold, extra]（args_num=3）。
EffectResult effect_1632_pp_gate_extra_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1632, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1632, args,
                     "1632 布局=[owner,target,点数,门槛,额外]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    const int threshold = args.int_args[3];
    const int extra = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1632, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0 && extra > 0, 1632, args, "需要 点数/额外 > 0");
    j68_drain_points(ctx, owner, opp, points);
    if (j68_any_pp_below(ctx, opp, threshold)) {
        j68_drain_points(ctx, owner, opp, extra);   // "额外吸取{2}点"
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1450, effect_1450_pp_gate_drain)
SKILL_EFFECT(1632, effect_1632_pp_gate_extra_drain)
