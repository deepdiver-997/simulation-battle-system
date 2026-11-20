// J 组 J294 · 攻击面板粉伤窗族 —— 1105×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1105（args_num=2）"{0}回合内每回合攻击附加自身攻击和特攻中最高一项{1}%的百分比伤害"
//
// 实现口径（同形先例逐字照抄）：
//   - J30 676 / J106 1186 直系（j_panel_pink.cpp）：EVENT_SKILL_USED watcher +
//     NextRounds 起点自卡窗 [start, start+{0})；ev.actor==owner + !used_attribute
//     （"使用攻击技能"——使用即触发，miss 也算）→ 粉伤打对手。
//   - 与 1186 的唯一差异 = 面板取"攻击/特攻**最高一项**"：
//     max(getTempAbilityValue(PHYSICAL_ATTACK), getTempAbilityValue(SPECIAL_ATTACK))。
//     视图槽（含能力等级，2501/676/1186 同口径）。
//   - 窗型取 NextRounds：与 1186 同模板族先例一致——施放当回合的动作就是"开窗"，
//     当次 EVENT_SKILL_USED 已在效果注册前发出（skills.cpp execute 起点），窗从下回合
//     起算；场景 T6270.a 显式钉死"施放当次不附加"。
//   - pct 行参在位即消费：显式 0 = 关闸（不粉伤），不走默认值、不报错（红线 7）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽、零剩余）：
//   m25316 斗破苍穹 [48,843,693,1105] → 1105=[3,80] / m26363 爆裂形态 → [3,80] /
//   m26792 未来·冥想 → [3,50] / m28375 烂漫星幕 → [3,70] /
//   m28740 星光·冥想 → [3,50] / m28994 暗之锐鸣 → [3,60] /
//   m36417 天龙碎骨断 → [2,30] / m37920 终末神决 → [3,30]。
//   测试载体：m25316 斗破苍穹（属性技；链上 48=免疫异常窗 / 843=先制+2 /
//   693=攻击伤害额外+50%——均不产生粉伤，粉伤断言面干净）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1105 无行为硬编码（仅 boss_invalid_effects
//   元数据表命中，PvE 标记不参与引擎派发）②effect_info 仅本条模板 ③moves_lib/soul_lib
//   并集（jbatch/kbatch 各 worktree）无注册——真缺口。
//   审计槽段：991105100+owner（面板最高项）/991105200+owner（本次粉伤）/991105300+owner
//   （本次 pct），grep 全库无撞。
//
// 场景回归：test/scenario/scenario_6270_attack_panel_pink.cpp（号段 6270-6279）。

#include "common.h"

namespace {

constexpr int k1105PanelBase = 991105100;   // +owner：攻击/特攻视图最高项
constexpr int k1105PinkBase = 991105200;    // +owner：本次粉伤
constexpr int k1105PctBase = 991105300;     // +owner：本次 pct

void j294_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 1105 的 EVENT_SKILL_USED watcher 体（676/1186 原形：NextRounds 自卡窗）──
void j294_on_skill_used(BattleContext* c, const BattleEvent& ev, int owner, int pct,
                        int start, int expire) {
    if (!c || c->roundCount < start || c->roundCount >= expire) {
        return;   // "N回合内"窗（NextRounds，起点折算后显式自卡）
    }
    if (ev.actor != owner || ev.used_attribute || ev.target < 0 || ev.target > 1) {
        return;   // 条件：自身使用攻击技能（属性技不算）
    }
    if (!g_core || !g_core->deal_pink_damage) {
        return;
    }
    // "攻击和特攻中最高一项"：视图槽（含能力等级，2501/676/1186 同口径）。
    const int pa = c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::PHYSICAL_ATTACK);
    const int sa = c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_ATTACK);
    const int panel = std::max(pa, sa);
    // pct 显式 0 = 关闸（在位即消费）：仍记审计槽，粉伤为 0。
    const int pink = panel * pct / 100;
    j294_set_storage(c, k1105PanelBase + owner, panel);
    j294_set_storage(c, k1105PinkBase + owner, pink);
    j294_set_storage(c, k1105PctBase + owner, pct);
    if (pink > 0) {
        (void)g_core->deal_pink_damage(c, ev.target, pink, DamageKind::PERCENT_VALUE, owner);
    }
}

}  // namespace

// ── 1105 —— {0}回合内每回合攻击附加自身攻击和特攻中最高一项{1}%的百分比伤害 ──
// int_args = [owner, target, rounds, pct]（args_num=2）。
EffectResult effect_1105_attack_panel_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1105, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1105, args,
                     "1105 布局=[owner,target,回合,pct]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1105, args, "需要 owner/target ∈ {0,1}");
    // pct 在位即消费：显式 0 合法（关闸），负值才是布局错误。
    EFFECT_ARG_GUARD(rounds > 0 && pct >= 0, 1105, args, "需要 回合>0 且 pct≥0");
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    const int expire = start + rounds;
    // 事件中心不防未来锚点（J25 记档）→ 锚当前回合、时长跨到窗末，lambda 自卡窗口。
    const int duration = (start - ctx->roundCount) + rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, duration, /*once=*/false,
        [owner, pct, start, expire](BattleContext* c, const BattleEvent& ev) {
            j294_on_skill_used(c, ev, owner, pct, start, expire);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

SKILL_EFFECT(1105, effect_1105_attack_panel_pink_window)
