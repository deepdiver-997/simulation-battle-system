// J 组 J286 · 对手攻击未命中异常窗族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1466 [3] "{0}回合内若对手攻击技能未命中则{1}%令对手{2}"
//   args 顺序 = [回合, 概率, 异常码]（8/8 载体脚本核算自洽）。
//
// 实现口径（先例：j_defense_windows.cpp 1023 的 EVENT_SKILL_INVALID miss watcher；
// j_miss_drain_pierce.cpp 1320 / j41 2300 同通道）：
//   - 窗 = EVENT_SKILL_INVALID watcher，duration_rounds={0}（注册起算，含当回合）。
//   - emit 载荷：actor=技能使用方、target=对面（skills.cpp:1220）→ 只认 ev.actor==opp。
//   - "攻击技能"过滤：roundChoice[opp][1] 取本次技能下标，type==Attribute 的无效不算
//     （1023 同款口径；属性技 miss/被封属也发 EVENT_SKILL_INVALID，不过滤会误触发）。
//   - {1}% 每次未命中独立掷（roll_percent），命中后 apply_anomaly_checked(100%,
//     ChanceSource::Skill)（概率已在窗外层掷过）；{1}=0 显式关闸（红线 7：不在位不守卫）。
//   - 审计槽 991466200+self = 累计异常落地次数；从未触发时槽位缺席（场景按缺席断言）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算，8/8 自洽）：
//   1466 [回合,概率,异常]：26712 绯红之影 [3,100,22]、26845 恐惧笛音 [3,100,6]、
//     26925 剧毒踪迹 [3,100,1]、27098 幻指变奏 [3,80,8]、27209 怒灵咆哮 [3,100,6]
//     （场景载体；链上 1283/576/783 全为自身增益/护体，不扰对手异常断言）、
//     27478 天外幽曲 [3,100,8]、28332 一笑流芳 [4,100,8]、34632 悠扬入梦曲 [3,100,8]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1466 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1466) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6190_miss_anomaly_window.cpp（号段 6190-6199）。

#include "common.h"

namespace {

constexpr int kAuditBase = 991466200;   // +self：累计异常落地次数

bool anomaly_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
        case ApplyAnomalyResult::CONVERTED:
            return true;
        default:
            return false;
    }
}

// ── 1466 —— {0}回合内若对手攻击技能未命中则{1}%令对手{2} ──
// int_args = [owner, target, 回合, 概率, 异常码]（args_num=3）。
EffectResult effect_1466_miss_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1466, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1466, args,
                     "1466 布局=[owner,target,回合,概率,异常]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1466,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1466, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1466, args,
                     "异常码越界（≤43）");

    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            // "攻击技能"过滤：本次技能下标取 roundChoice（1023 同端口径）；属性技无效不算。
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;
            }
            if (!roll_skill_percent(*c, 1466, 0, self, opp, chance)) {
                return;   // {1}% 未中（每次未命中独立掷）
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, /*chance=*/100,
                ChanceSource::Skill);
            if (anomaly_landed(r)) {
                const auto it = c->plugin_storage.find(kAuditBase + self);
                const int before = it == c->plugin_storage.end()
                                       ? 0
                                       : any_int_or(it->second, 0);
                c->plugin_storage[kAuditBase + self] = before + 1;
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1466, effect_1466_miss_anomaly_window)
