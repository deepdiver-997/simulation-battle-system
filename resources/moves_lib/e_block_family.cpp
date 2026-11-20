// E12 补缺 · 挡伤票统一化 —— 794 / 1882 / 49 / 2360（2026-09-29，worktree e13 续）
//
// 现状盘点结论（2026-09-29）：挡伤体系内核已完整——DamagePipeline BLOCK 阶段 +
// RuleCenter DAMAGE 票（46 先例，crawford_skills.cpp）+ 沧岚"挡伤失效"抑制门
// （pet.damage_suppress_mask，蚀砚之泪≥4 滴置 BLOCK|DETECT 位，管线 walk 自动跳过
// 被抑制类别——**DAMAGE 票天然吃同一道门**，这正是统一化的意义）。本文件把剩余
// 缺口接进同一条路，不新建并行机制：
//
// 794（args_num=1）"消除对手能力提升，消除成功可以抵挡{0}回合内对手的攻击伤害"：
//   clear_stat_boosts(opp) > 0 门槛 → grant_immune(DAMAGE, duration={0}, counts=0)
//  （回合窗口型全挡——46 的 counts 型是"次数耗尽"，本条是"窗口期内每次攻击都挡"；
//   两者同票同门）。载体：灵龙破 19016 [794]|[2]、雷神双灭斩 17677 [794,789]|[3,1]。
// 1882（args [1,100,32]）"令己方下只出战精灵抵挡下{0}次对手的攻击，且出战时{1}%{2}"：
//   两段式（D17 840 的己方镜像）：EVENT_DEATH(target=自己) 探针置旗 → 己方下只
//   EVENT_ENTER_STAGE 授 counts 型 DAMAGE 票 + 概率对对手施加异常{2}（{2}=32 凝滞；
//   对象按语感"出战时{1}%令对手{2}"，记档）。载体：皎明若玉 36731。
//   ⚠️ 2026-11-06 勘误（场景 8032 首次覆盖授予面时暴露）：ENTER_STAGE watcher 旧判
//   `ev.target != owner` 与载荷约定（actor=登场方，target=对方侧，battleFsm perform_switch
//   emit 点）不符 → 补位登场时恒真、授予半从未触发（待决旗置位后半边死代码）。已改判
//   `ev.actor != owner`（己方下只出战精灵 = actor==owner），8032 对照分支守回归。
// 49（args=[N]）"可以抵挡{0}点伤害"：护体族（护盾/水盾/寒冰护体/固若金汤…全部
//   老护体技）——**数值吸收走护盾通道**（shield_bank_.add），不建并行"数值票"：
//   与现有护盾同池同优先级，官方语义本就是"最老的护盾技能"。
// 2360（王·龙予狂宴段）"免疫下次对手攻击的伤害并附加免疫量的真实伤害"：
//   2360 的注册包装落在 damage_modifier.cpp（helper `arm_immunity_reflect_true`
//   同文件，匿名空间内不可跨 TU——816 侧注册区直接追加）。
//
// 跳过记档：
//   688「抵挡{0}回合内对手的攻击」——moves 表零引用（effect_info 有模板无载体）；
//   2292（战·日月逆行 37910）——"武道衍化轨迹正转/逆转"状态机不存在（grep 全库
//   无武道状态），需先建状态再接，同类跳过先例=D18 四杀鬼囹。
//
// 场景回归：test/scenario/scenario_600_block_family.cpp（水王 4 滴抑制集成）。

#include "common.h"

namespace {

// ── 794 —— 消除对手能力提升，消除成功可以抵挡{0}回合内对手的攻击伤害 ──
EffectResult effect_794_break_then_block_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛（消强族同款，868/1010 先例）
    }
    // 回合窗口型全挡：counts=0（不计数）+ duration={0}——与 46 的 counts 型同票同门
    //（沧岚 4 滴抑制门自动覆盖，见文件头）。
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
        /*duration_rounds=*/rounds, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
        /*source_effect_id=*/794);
    return EffectResult::kOk;
}

// ── 1882 —— 令己方下只出战精灵抵挡下{0}次对手的攻击，且出战时{1}%{2} ──
EffectResult effect_1882_next_pet_block(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (owner < 0 || owner > 1 || counts <= 0 || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // storage 旗标（owner 视角）：自己被击败 → 置旗；己方下只登场 → 授票+异常。
    // bool 旗标独立键段（armed_flag bool 撞 int 坑已两次——tudie_runtime.h 纪律）。
    constexpr int kPendingKeyBase = 990188200;
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            c->plugin_storage[kPendingKeyBase + owner] = 1;
        },
        WatcherScope::TEAM);
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, counts, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            // ⚠️ 载荷约定（battleFsm perform_switch emit 点）：EVENT_ENTER_STAGE 的
            // **actor = 登场方**、target = 对方侧。判"己方下只精灵登场"必须认
            // **actor**——2026-11-06 勘误（场景 8032 首次覆盖本授予面时暴露）：
            // 旧判 `ev.target != owner` 在补位登场时恒真（target 恒为对方侧）→
            // 授予半从未触发过（待决旗置位后半边死代码）。
            if (!c || ev.actor != owner || !g_core) {
                return;
            }
            auto it = c->plugin_storage.find(kPendingKeyBase + owner);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 不是"自己被击败后的登场"
            }
            c->plugin_storage[kPendingKeyBase + owner] = 0;
            // ① 抵挡 {0} 次对手的攻击（counts 型 DAMAGE 票——46 同款）
            c->rule_center_.grant_immune(
                owner, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
                /*duration_rounds=*/0, c->roundCount, /*source_id=*/0, /*soul=*/false,
                EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/counts,
                /*source_effect_id=*/1882);
            // ② 出战时 {1}% 令对手 {2}（{2}=32 凝滞；对象=对手，语感口径记档）
            if (g_core->apply_anomaly && roll_percent(chance)) {
                (void)g_core->apply_anomaly(c, 1 - owner, anomaly,
                                            random_anomaly_duration(), owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 49 —— 可以抵挡{0}点伤害（护体族：数值吸收走护盾通道）──
EffectResult effect_49_shield_value(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int value = args.int_args[2];
    if (owner < 0 || owner > 1 || value <= 0) {
        return EffectResult::kOk;
    }
    // 与现有护盾同池（mistike/burn_trio/qiling 同款 add 调用）；
    // 优先级 0（普通护盾层）、不过期（duration=0，护体无回合限制——耗尽为止）。
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, value, /*source_id=*/49,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(794, effect_794_break_then_block_window)
SKILL_EFFECT(1882, effect_1882_next_pet_block)
SKILL_EFFECT(49, effect_49_shield_value)
