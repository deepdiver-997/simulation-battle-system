// J 组 J279 · 异常施加刷新族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1586 [4] "{0}%令对手{1}，若对手已处于{2}状态则将此状态刷新至{3}回合"
//
// 实现口径（官方字面两分支，互斥）：
//   ① 刷新分支：对手**已处于** {2} 状态 → `set_anomaly_rounds(opp, {2}, {3})`
//      （CoreApi 直写剩余回合，只对已存在异常生效；2399 宿世归泯同款，
//      end_round = roundCount + {3}，与 apply_anomaly 同口径）。不再重复施加。
//   ② 施加分支：对手不处于 {2} → `apply_anomaly_checked(opp, {1}, 持续=2,
//      actor=owner, chance_pct={0}, ChanceSource::Skill)`——现代施加链
//      （次免→弹控→抗性→魂免→转化→落地）+ 概率闸门申报；{0} 由原语内掷，
//      插件不再另掷一次（避免双掷）。
//   - pct 在位即消费：{0}=0 → 永不施加（仍走刷新分支判定），不拿 >0 守卫盖掉 0 档。
//   - 审计槽：991586000+owner = 刷新分支命中次数；991586100+owner = 上次施加结果
//     （ApplyAnomalyResult 枚举值，-1 = 未走施加分支）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   1586 [chance, apply, check, rounds]：27180 燃焰禁地 [100,2,2,4]、28196 幽途诡道
//     [100,10,10,4]、28444 赤炎之盾 [100,2,2,3]、28476 冰结之气 [80,5,5,3]、
//     34772 绝地逆鳞 [100,1,1,3]、36149 蔑川燎原 [100,2,2,5]（纯链，场景载体）、
//     36199 奈落纠缠 [100,1,1,3]、36705 炼神真焱 [100,2,2,5]（纯链）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1586 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与
//   各 agents worktree）无 SKILL_EFFECT(1586) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6120_anomaly_apply_refresh.cpp（号段 6120-6129）。

#include "common.h"

namespace {

constexpr int kRefreshAuditBase = 991586000;   // +owner：刷新分支命中次数
constexpr int kApplyAuditBase = 991586100;     // +owner：上次施加结果（-1=未走施加）

// ── 1586 —— {0}%令对手{1}；已处于{2}则刷新至{3}回合 ──
// int_args = [owner, target, chance, apply_code, check_code, rounds]（args_num=4）。
EffectResult effect_1586_apply_refresh(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1586, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1586, args,
                     "1586 布局=[owner,target,概率,施加码,检查码,刷新回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int apply_code = args.int_args[3];
    const int check_code = args.int_args[4];
    const int rounds = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1586, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1586, args, "概率须在 [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(apply_code)
                         && is_valid_abnormal_status_id(check_code),
                     1586, args, "异常码越界（≤43）");
    EFFECT_ARG_GUARD(rounds > 0, 1586, args, "刷新回合须 > 0");
    // 审计槽每次使用重置，场景可按"本次"断言。
    ctx->plugin_storage[kRefreshAuditBase + owner] = 0;
    ctx->plugin_storage[kApplyAuditBase + owner] = -1;
    if (!g_core) {
        return EffectResult::kOk;
    }

    // ① 已处于 {2} 状态 → 刷新至 {3} 回合（只刷新已存在，不无中生有）。
    if (g_core->has_active_anomaly && g_core->set_anomaly_rounds
        && g_core->has_active_anomaly(ctx, opp, check_code)) {
        const auto it = ctx->plugin_storage.find(kRefreshAuditBase + owner);
        ctx->plugin_storage[kRefreshAuditBase + owner] =
            (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
        (void)g_core->set_anomaly_rounds(ctx, opp, check_code, rounds);
        return EffectResult::kOk;
    }

    // ② 未处于 {2} → 按 {0}% 施加 {1}（概率申报给原语，闸门改写后再掷一次）。
    ctx->plugin_storage[kApplyAuditBase + owner] = -1;
    if (!g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult res =
        g_core->apply_anomaly_checked(ctx, opp, apply_code, random_anomaly_duration(), owner,
                                      chance, ChanceSource::Skill);
    ctx->plugin_storage[kApplyAuditBase + owner] = static_cast<int>(res);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1586, effect_1586_apply_refresh)
