#include <utils/sbs_rand.h>
// J 组 J273 · 异常门固伤/异常窗族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1143 [3] "对手处于异常状态时附加{0}点固定伤害，不处于异常状态则下{1}回合
//   使用技能随机附加烧伤、冻伤、冰封、焚烬中的{2}种异常状态"
//
// 实现口径（门 = has_any_active_abnormal_status(对手)，575/2009 同判据）：
//   - 门开：fixed_damage(对手, {0})（粉伤原语；护罩/免粉照常参与），**不注册窗**。
//   - 门闭：AFTER_ACTION 双侧 tick（j_window_roll_anomaly 771 同型，rounds={1}
//     InRounds 含当回合）+ executed_skill_this_round 门（"使用技能"=成功使用，
//     被控/NONE 不算，§四点五）→ 池 {烧伤2, 冻伤5, 冰封15, 焚烬22} 做部分
//     Fisher-Yates 不重复抽 {2} 种（超过池长钳到 4）→ apply_anomaly_checked
//     (对手, code, 100)（申报制 100%=不消耗掷点）。
//   - 审计槽：991143000+owner = 1 门开 / 0 门闭；991143100+owner = 窗内 tick 次数；
//     991143200+owner = 最近一次抽取 bitmask（池下标位）。
//   - {0}/{1}/{2} 在位即消费（显式 0：固定伤害 0 是合法空附加；{2}=0 抽取 0 种）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1143 [fixed, rounds, count]：25376 雪灵焱甲 [300,1,2]、28290 异灵之牙 [300,2,2]、
//     32564 流星光斩 [200,1,1]、32746 极龙破音嚎 [300,1,2]、34791 天使之序灭 [300,1,1]、
//     36166 峰谷聚合 [300,2,2]、36695 司天镇国诀 [300,1,2]、36969 术傀异咒破 [350,2,2]、
//     37430 日月轮·重天 [300,1,2]（纯链，场景载体）（9/9 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1143 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1143) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6060_anomaly_gate_fixed_window.cpp（号段 6060-6069）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kGateBase = 991143000;   // +owner：1=门开（立即固伤）/0=门闭（注册窗）
constexpr int kTickBase = 991143100;   // +owner：窗内 tick 次数
constexpr int kMaskBase = 991143200;   // +owner：最近一次抽取 bitmask（池下标位）

// 官方模板列出的四种异常（池序 = bitmask 位序）。
constexpr int kPool[4] = {
    static_cast<int>(AbnormalStatusId::Burn),        // 2 烧伤
    static_cast<int>(AbnormalStatusId::Frostbite),   // 5 冻伤
    static_cast<int>(AbnormalStatusId::Icebound),    // 15 冰封
    static_cast<int>(AbnormalStatusId::Incinerate),  // 22 焚烬
};

// ── 窗 tick —— 成功使用技能 → 随机抽 {count} 种异常附加给对手 ──
// args=[owner, opp, count]
EffectResult j273_1143_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // "使用技能"门（被控/NONE 不算）
    }
    int idx[4] = {0, 1, 2, 3};
    const int n = std::min(std::max(count, 0), 4);
    int mask = 0;
    for (int i = 0; i < n; ++i) {
        const int j = i + (4 - i > 1 ? sbs_rand() % (4 - i) : 0);
        std::swap(idx[i], idx[j]);
        const int code = kPool[idx[i]];
        mask |= 1 << idx[i];
        (void)g_core->apply_anomaly_checked(ctx, opp, code, random_anomaly_duration(),
                                            self, 100, ChanceSource::Skill);
    }
    const int prev_ticks = any_int_or(ctx->plugin_storage[kTickBase + self]);
    ctx->plugin_storage[kTickBase + self] = prev_ticks + 1;
    ctx->plugin_storage[kMaskBase + self] = mask;
    return EffectResult::kOk;
}

// ── 1143 —— 异常门：附加{0}固伤 / 否则窗内随机附加{2}种异常 ──
// int_args = [owner, target, fixed, rounds, count]（args_num=3）。
EffectResult effect_1143_anomaly_gate_fixed_window(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1143, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1143, args,
                     "1143 布局=[owner,target,固伤,回合,种数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    const int rounds = args.int_args[3];
    const int count = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1143, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed >= 0, 1143, args, "固伤不能为负");
    EFFECT_ARG_GUARD(rounds > 0, 1143, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(count >= 0, 1143, args, "种数不能为负");

    if (ctx->has_any_active_abnormal_status(opp)) {
        ctx->plugin_storage[kGateBase + self] = 1;
        if (fixed > 0 && g_core && g_core->fixed_damage) {
            (void)g_core->fixed_damage(ctx, opp, fixed);
        }
        return EffectResult::kOk;   // 门开：不注册窗
    }

    ctx->plugin_storage[kGateBase + self] = 0;
    const std::vector<int> payload{self, opp, count};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1143, rounds, payload, &j273_1143_window_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1143, effect_1143_anomaly_gate_fixed_window)
