// J121 · 阈值门族二 —— 473×12 + 503×13（2026-10-03，worktree ../agents/j121）
//
// 家族语义（effect_info 官方模板 + 载体切片 + 4399 文本交叉验证）：
//
// 473（args_num=3）"若造成的伤害不足{0}，则自身{1}等级+{2}"：
//   - DETECT 攻方桶恒久回调（J15 1088 阈值形直系，模板无窗口）；门 = 红伤 final
//     **严格 <** {0}（"不足"；属性技无伤害相位自然不触发）；
//   - {1} 是**官方码序**（0攻/1防/2特攻/3特防/4速——速查 §三 属性排列双序高频坑）
//     → 引擎槽映射 {0,2,1,3,4}：16825 炽烈炫光破 [300,2,1]=自身**特攻**+1、
//     15329 紫金狂战击 [300,4,1]=**速**+1 双载体 4399 文本交叉验证；
//   - 加成走 stat_change 真原语（自身增益通道，不吃免弱/同步视图）。
// 503（args_num=2）"若造成的伤害不足{0}，则回合结束时对手损失{1}点固定体力"：
//   - 同门；触发即登记**当回合** BATTLE_ROUND_END 一次性 tick（J14 1024 兜底同款
//     register_round_effect，伪 id 990050301）→ fixed_damage(防御方, {1})；
//   - ⚠️ 记档：「损失体力」非攻击伤害，内核无 lose 原语——fixed_damage 近似
//     （走护罩/固定抗性通道，勘误留待原语落地）；逐刀各自登记（register_effect
//     不去重），多次不足=多次损失，与逐刀触发语义一致。
//
// 动工前置三查（2026-10-03）：①内核 grep 473/503 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect.arg_count
//   473=3/503=2 与 effect_info.args_num 一致；25 载体 [阈值,槽码,级]/[阈值,点数]
//   自洽零剩余（脚本核算 2026-10-03）。
// 先例：J15 729/1088（DETECT 攻方桶阈值门）、J14 1024（ROUND_END 一次性节点）、
//   J2 1170（固伤原语）。
// 场景回归：test/scenario/scenario_2150_threshold_gate2.cpp（2150 号段）。

#include "common.h"

namespace {

// 503 的 ROUND_END 一次性节点伪 id（桶键，10^9 段外——J14 990102401 同款段式）。
constexpr int kJ121Node503Tick = 990050301;

// 官方码序（0攻/1防/2特攻/3特防/4速）→ 引擎能力槽（0攻/1特攻/2防/3特防/4速）。
// ⚠️ 防与特攻互换是本映射的全部意义（速查 §三 双序坑）；越界码由 guard 拦。
constexpr int kOfficialCodeToSlot[5] = {0, 2, 1, 3, 4};

// 503 tick：payload = [owner, defender, amount] → 对手损失 amount 点固定体力。
EffectResult j121_503_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int defender = args.int_args[1];
    const int amount = args.int_args[2];
    if (defender < 0 || defender > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->fixed_damage(ctx, defender, amount);
    return EffectResult::kOk;
}

}  // namespace

// ── 473 —— 若造成的伤害不足{0}，则自身{1}等级+{2} ──
// int_args = [owner, target, threshold, code, levels]（args_num=3）。
EffectResult effect_473_insufficient_self_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 473, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 473, args,
                     "473 布局=[owner,target,阈值,槽码,等级]");
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int code = args.int_args[3];
    const int levels = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     473, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && levels != 0, 473, args, "需要 阈值>0 且 等级≠0");
    EFFECT_ARG_GUARD(code >= 0 && code <= 4, 473, args,
                     "槽码 " + std::to_string(code) + " 越界（官方码序 0..4）");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, threshold, code, levels](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->stat_change) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final >= threshold) {
                return;   // "伤害不足{0}"：红伤且严格小于（属性技无红伤自然不触发）
            }
            (void)g_core->stat_change(c, owner, kOfficialCodeToSlot[code], levels);
        });
    return EffectResult::kOk;
}

// ── 503 —— 若造成的伤害不足{0}，则回合结束时对手损失{1}点固定体力 ──
// int_args = [owner, target, threshold, amount]（args_num=2）。
EffectResult effect_503_insufficient_round_end_loss(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 503, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 503, args,
                     "503 布局=[owner,target,阈值,点数]");
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     503, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && amount > 0, 503, args, "需要 阈值/点数 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, threshold, amount](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final >= threshold) {
                return;   // "伤害不足{0}"：红伤且严格小于
            }
            register_round_effect(c, State::BATTLE_ROUND_END, owner, kJ121Node503Tick,
                                  /*rounds=*/1, {owner, d.defenderId, amount},
                                  &j121_503_round_end_tick);
        });
    return EffectResult::kOk;
}

SKILL_EFFECT(473, effect_473_insufficient_self_up)
SKILL_EFFECT(503, effect_503_insufficient_round_end_loss)
