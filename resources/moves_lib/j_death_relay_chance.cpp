// J81 · 概率施加/死亡接棒族 —— 1424×15 / 1319×16（2026-10-03，worktree ../agents/j81）
//
// 家族语义（effect_info 官方模板 + 载体切片 + 4399 文本核对）：
//
// 1424（args_num=2）"{0}%令对手{1}，自身处于能力提升状态时概率翻倍"：
//   - 申报制异常（J3/J14 口径）；翻倍判据 = **自身**任一正槽（1274 镜像：它判对手
//     负槽）；掷点前求值、封顶 100；
//   - 审计槽（F9 782 口径）：本次申报的有效概率写 plugin_storage（990142400+owner），
//     场景可对翻倍梯度做无随机断言。
//   - 实例：灰烬 36467 [1424]|[50,2]（物5 纯链 50% 烧伤）。
// 1319（args_num=6）"自身下次被击败后下一只出场的精灵{0}"：
//   - {0} 吞尾 = **语义序六槽向量**（4399 黑死魔能"自身被击败后下一只出场的精灵
//     攻击+2、防御+2" ↔ 切片 [2,2,0,0,0,0]；广寒天威尾 [2,0,2,0,2,0]="攻+2特攻+2速+2"
//     同构——kStatMap 语义序→引擎槽）；
//   - 接棒 = 两段 watcher（K3 330 死亡继承事件面直系）：用技时挂 EVENT_DEATH
//     （once=true="下次"；ev.target==self）把向量写 plugin_storage 暂存；
//     EVENT_ENTER_STAGE（本方，ev.actor==owner）见暂存即对登场宠逐槽 stat_change
//     （正值提升，不吃免弱——common.h effect 4 口径）并清暂存（一次性）。
//   - 实例：黑死魔能 36577 [1319]|[2,2,0,0,0,0]（特攻140 纯链）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1424/1319 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1424=2/1319=6 与 effect_info.args_num 一致；31 载体切片自洽（1319 尾位载体六槽
//   向量与 4399 文本逐一吻合）。
// 存储键段（grep 全库无撞）：审计 990142400+owner；接棒暂存 990131900+owner*10+slot。
// 先例：J14 1274（翻倍判据/审计槽）、K3 330（k_thorn_reflect.cpp 死亡继承两段
//   watcher）、J48（kStatMap）。
// 场景回归：test/scenario/scenario_1700_death_relay_chance.cpp（1700 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <algorithm>

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（J48 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

constexpr int kAudit1424Base = 990142400;   // +owner：1424 有效概率审计槽
constexpr int kRelay1319Base = 990131900;   // +owner*10+slot：1319 接棒向量暂存
constexpr int kRelay1319Flag = 990131999;   // 暂存有效旗（避免逐槽 find）

// 自身任一正槽（J14/J76 助手同判据；匿名空间不跨 TU，就地重写）。
bool j81_has_any_stat_up(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 1424 —— {0}%令对手{1}，自身处于能力提升状态时概率翻倍 ──
// int_args = [owner, target, chance, anomaly]（args_num=2）。
EffectResult effect_1424_chance_double_on_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1424, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1424, args,
                     "1424 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1424, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1424, args,
                     "{1} 须为合法异常码");
    EFFECT_ARG_GUARD(chance > 0, 1424, args, "需要 概率 > 0");
    // 翻倍门槛在掷点前求值（自身任一正槽）；封顶 100。审计槽供场景无随机断言。
    const int eff = j81_has_any_stat_up(ctx, owner) ? std::min(2 * chance, 100) : chance;
    ctx->plugin_storage[kAudit1424Base + owner] = eff;
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, target, args.int_args[3],
                                            random_anomaly_duration(), owner, eff,
                                            ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 1319 —— 自身下次被击败后下一只出场的精灵{0}（语义序六槽向量）──
// int_args = [owner, target, 六槽×6]（args_num=6）。
EffectResult effect_1319_defeat_relay_sixslot(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1319, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 1319, args,
                     "1319 布局=[owner,target,六槽×6]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1319, args, "owner 越界");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    // 段一：自身"下次"被击败 → 暂存向量（once=true 自摘除，"下次"语义）。
    const std::vector<int> vec(args.int_args + 2, args.int_args + 8);
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [owner, vec](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner) {
                return;
            }
            for (int i = 0; i < 6; ++i) {
                wctx->plugin_storage[kRelay1319Base + owner * 10 + i] = vec[i];
            }
            wctx->plugin_storage[kRelay1319Flag + owner] = 1;
        },
        WatcherScope::TEAM);
    // 段二：本方下一只登场 → 见暂存即施加并清（一次性接棒）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || !g_core || !g_core->stat_change) {
                return;
            }
            if (!wctx->plugin_storage.count(kRelay1319Flag + owner)) {
                return;   // 没有接棒暂存（未击败过）→ 不施加
            }
            for (int i = 0; i < 6; ++i) {
                const int delta = any_int_or(
                    wctx->plugin_storage[kRelay1319Base + owner * 10 + i]);
                if (delta != 0) {
                    (void)g_core->stat_change(wctx, owner, kStatMap[i], delta);
                }
            }
            wctx->plugin_storage.erase(kRelay1319Flag + owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

SKILL_EFFECT(1424, effect_1424_chance_double_on_boosts)
SKILL_EFFECT(1319, effect_1319_defeat_relay_sixslot)
