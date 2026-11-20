// J 组 J277 · 自爆传功下只族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   59 [2] "消耗自身全部体力(体力降到0)，使下一只出战精灵的{0}和{1}能力提升1个等级"
//   {0}/{1} = 官方码序能力下标（0攻 1防 2特攻 3特防 4速 5命中）。
//
// 实现口径：
//   - 自爆 = diwang_3512_skills.cpp `self_destruct_hp_consume` 直系：hp=0 →
//     defeat_pet(HP_CONSUME)（穿"残留免死"拦截器，先例 2213/1212；actor=对手——
//     与帝王自爆同口径）。
//   - 传功 = EVENT_ENTER_STAGE watcher（TEAM；actor==self 表示本方登场，1319/698
//     同款）→ 首只登场宠按 kStatMap 把 {0}/{1} 各 +1（官方码序→引擎槽，J48/J81 同表），
//     施加后置审计并自删（"下一只"一次语义）。
//   - 顺序：**先注册 watcher 再自爆**——否则登场事件先于监听器存在会漏接。
//   - 审计槽 991059000+owner = 1（已传功）/0（未传功）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算，全属性技载体）：
//   59 [stat1, stat2]：20175 灵魂附体 [2,3]、20264 能量灌注 [1,2]、
//     20330 眼球的掌控 [1,3]、20522 绝处逢生 [0,1]、20626 献身大海 [0,2]、
//     20670 机械之魂 [1,3]、20720 掩护 [1,3]、20749 传承 [4,5]、
//     22558 永恒战魂 [0,4]（9/9 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 59 无行为硬编码（仅注释序号命中）
//   ②effect_info 模板唯一、custom_effect_overrides/programs 零行 ③全库
//   moves_lib/soul_lib 并集（含 kbatch 与各 agents worktree）无 SKILL_EFFECT(59)
//   注册——真缺口。
//
// 场景回归：test/scenario/scenario_6100_self_destruct_relay_boost.cpp（号段 6100-6109）。

#include "common.h"

namespace {

constexpr int kAuditBase = 991059000;   // +owner：1=已传功 / 0=未传功

// 官方码序（0攻 1防 2特攻 3特防 4速 5命中）→ 引擎能力槽（J48/J81 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

// ── 59 —— 消耗自身全部体力，使下一只出战精灵 {0}/{1} 各 +1 ──
// int_args = [owner, target, stat1, stat2]（args_num=2）。
EffectResult effect_59_self_destruct_relay_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 59, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 59, args,
                     "59 布局=[owner,target,stat1,stat2]");
    const int self = args.int_args[0];
    const int stat1 = args.int_args[2];
    const int stat2 = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 59, args, "owner 越界");
    EFFECT_ARG_GUARD(stat1 >= 0 && stat1 < 6 && stat2 >= 0 && stat2 < 6, 59, args,
                     "能力下标须在 [0,5]");
    if (!g_core || !g_core->defeat_pet) {
        return EffectResult::kOk;
    }

    ctx->plugin_storage[kAuditBase + self] = 0;
    // ① 先挂"下一只本方登场"监听（TEAM；自爆换宠后仍存活）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, stat1, stat2](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || !g_core || !g_core->stat_change) {
                return;   // 只认本方登场
            }
            const int prev = any_int_or(c->plugin_storage[kAuditBase + self]);
            if (prev != 0) {
                return;   // 已传功（"下一只"一次）
            }
            (void)g_core->stat_change(c, self, kStatMap[stat1], 1);
            (void)g_core->stat_change(c, self, kStatMap[stat2], 1);
            c->plugin_storage[kAuditBase + self] = 1;
        },
        WatcherScope::TEAM);

    // ② 自爆：体力清 0 → HP_CONSUME 死亡漏斗（穿残留免死；actor=对手）。
    const int slot = ctx->on_stage[self];
    ElfPet& me = ctx->seerRobot[self].elfPets[slot];
    if (me.hp <= 0) {
        return EffectResult::kOk;   // 已死（反射/反伤等）：自爆无从谈起
    }
    me.hp = 0;
    (void)g_core->defeat_pet(ctx, self, slot, 1 - self, DefeatCause::HP_CONSUME);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(59, effect_59_self_destruct_relay_boost)
