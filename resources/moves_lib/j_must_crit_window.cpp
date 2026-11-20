// J 组 J291 · 必暴窗族 —— 2 宏/14 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   623 [1] "消除自身能力下降状态，消除成功下{0}回合自身直接攻击必定暴击"
//   493 [2] "{0}回合内若对方使用攻击技能则自身下{1}回合必定暴击"
//
// 实现口径：
//   - 必暴通道 = 58 圣光气 / e_must_crit.cpp 同款：NextRounds 窗 + BATTLE_ROUND_START tick
//     每回合重写 `ws.must_crit[owner]=true`（ws 每回合 reset；暴击掷点在 query_usage
//     ①.5 OR 读该旗标，crit_rate==0 也能"必定"）。
//   - 623 门槛 = `clear_stat_drops(自身)` 返回清除条数，>0 即"消除成功"才开窗
//     （清弱化不查任何免疫——原语注释；STAT_DROP_LOCK 例外由原语自己拦）。窗口
//     "下{0}回合" = NextRounds（本回合不算，e_must_crit 475/1180 同款）。
//   - 493 窗 = EVENT_SKILL_USED watcher（E13 469 注册法；676 同款）。条件
//     `ev.actor==对手 && !ev.used_attribute`：事件在 execute 起点发，载荷是**声明类型**
//     （SOP §三点：变型前），"对方使用攻击技能"按声明类型判。
//     触发 → 给自身开 NextRounds({1}) 必暴窗（下{1}回合）。
//     事件中心不防未来锚点（J25 记档）→ 锚当前回合、时长跨到窗末，lambda 内自卡
//     [start, expire)（676 同款）；"N回合内"用 InRounds 折算（后手顺延到下回合起算）。
//
// plugin_storage 审计槽（精确断言用；grep 全库无撞）：
//   996230000 + owner = 623 本次清除的弱化条数（门闭写 0）
//   996230100 + owner = 623 累计开窗次数
//   994930000 + owner = 493 累计触发次数（对方攻击技落在窗内）
//   994930100 + owner = 493 累计"窗内看到对方攻击技"候选次数（= 触发次数）
//
// 载体切片（side_effect.arg_count 顺序切分，脚本核算 2026-10-22，7/7 + 7/7 自洽）：
//   623 [回合]：16288 神光爆裂掌 [1]（纯链，场景载体）/ 17222 噬血怒击 [2] /
//     17445 天沐剑雨 [2] / 17762 暗影诛杀诀 [2] / 28262 铁器之护 [2] /
//     36829 固守绝击 [2] / 38195 断沙斩 [1]。
//   493 [窗回合,必暴回合]：17630 火狮怒爪击 [2,2]（纯链，场景载体）/ 22313 复仇之潮
//     [2,1] / 24165 冥域之罚 [2,2] / 25411 秘法冰魂 [2,2] / 26258 天蛇禁法 [2,2] /
//     26518 幻舞狂歌 [3,2] / 27657 花叶散落 [3,2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 623/493 无行为硬编码
//   ②effect_info 模板唯一、custom_effect_overrides/programs 零行
//   ③全库 moves_lib/soul_lib 并集（含 kbatch 与各 agents worktree）无 SKILL_EFFECT
//   注册——真缺口。
//
// 场景回归：test/scenario/scenario_6240_must_crit_window.cpp（号段 6240-6249）。

#include "common.h"

namespace {

constexpr int k623ClearedAuditBase = 996230000;  // +owner：本次清除弱化条数
constexpr int k623ArmedAuditBase = 996230100;    // +owner：累计开窗次数
constexpr int k493TriggerAuditBase = 994930000;  // +owner：累计触发次数
constexpr int k493SeenAuditBase = 994930100;     // +owner：窗内候选次数（= 触发次数）

// ── 必暴窗口 tick（e_must_crit.cpp 58 同款）：ROUND_START 桶每回合重写 ws.must_crit ──
EffectResult must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// ── 623 —— 消除自身能力下降状态，消除成功下{0}回合自身直接攻击必定暴击 ──
// int_args = [owner, target, 回合]（args_num=1）。
EffectResult effect_623_clear_drops_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 623, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 623, args,
                     "623 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     623, args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：0/负档 = 不开窗（显式关闸，不回退默认值）。
    ctx->plugin_storage[k623ClearedAuditBase + owner] = 0;
    if (rounds <= 0 || !g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int cleared = g_core->clear_stat_drops(ctx, owner);
    ctx->plugin_storage[k623ClearedAuditBase + owner] = cleared;
    if (cleared <= 0) {
        return EffectResult::kOk;   // 门：消除失败（本就没有能力下降）
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 623, rounds,
                                {owner, target}, &must_crit_tick);
    const auto it = ctx->plugin_storage.find(k623ArmedAuditBase + owner);
    ctx->plugin_storage[k623ArmedAuditBase + owner] =
        (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0)) + 1;
    return EffectResult::kOk;
}

// ── 493 —— {0}回合内若对方使用攻击技能则自身下{1}回合必定暴击 ──
// int_args = [owner, target, 窗回合, 必暴回合]（args_num=2）。
EffectResult effect_493_opp_attack_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 493, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 493, args,
                     "493 布局=[owner,target,窗回合,必暴回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int window_rounds = args.int_args[2];
    const int buff_rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 493,
                     args, "需要 owner/target ∈ {0,1}");
    if (window_rounds <= 0 || buff_rounds <= 0) {
        return EffectResult::kOk;   // 行参在位即消费：显式 0 = 关闸档
    }
    const int start = ctx->round_effect_start_round(owner, window_rounds,
                                                    EffectWindowKind::InRounds);
    const int expire = start + window_rounds;
    // 事件中心不防未来锚点（J25 记档）→ 锚当前回合、时长跨到窗末，lambda 自卡窗口。
    const int duration = (start - ctx->roundCount) + window_rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, duration, /*once=*/false,
        [owner, opp, buff_rounds, start, expire](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗外
            }
            if (ev.actor != opp || ev.used_attribute) {
                return;   // 条件：对方使用攻击技能（属性技不算）
            }
            const auto seen = c->plugin_storage.find(k493SeenAuditBase + owner);
            c->plugin_storage[k493SeenAuditBase + owner] =
                (seen == c->plugin_storage.end() ? 0 : any_int_or(seen->second, 0)) + 1;
            // 触发 → 自身下{buff_rounds}回合必暴（NextRounds，本回合不算）。
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, owner, 493, buff_rounds,
                                        {owner, opp}, &must_crit_tick);
            const auto trg = c->plugin_storage.find(k493TriggerAuditBase + owner);
            c->plugin_storage[k493TriggerAuditBase + owner] =
                (trg == c->plugin_storage.end() ? 0 : any_int_or(trg->second, 0)) + 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(623, effect_623_clear_drops_must_crit)
SKILL_EFFECT(493, effect_493_opp_attack_must_crit)
