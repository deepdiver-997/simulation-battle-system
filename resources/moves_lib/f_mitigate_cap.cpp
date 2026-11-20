// F2 · 减伤封顶族 —— 766×36 / 811×34 / 1034×32（2026-09-29，worktree ../agents/f2）
//
// 家族形状：数值削减/免伤类防御效果，全部走既有范式、不改内核——
// CAP 阶段内核早已备好（damage_pipeline.h kOrder），只是"阶段在、生产者未做"，
// 本文件是 CAP 的**首个插件生产者**（FLOOR 的内核默认条目见 battleContext.cpp
// install_default_damage_floor；CAP 此前零注册）。
//
// 766（args_num=2）"消除对手能力提升状态，消除成功则{0}回合内对手造成的攻击伤害不超过{1}点"：
//   - 消强门槛（clear_stat_boosts 返回清掉个数 >0，868/1010/794 先例）；
//   - "攻击伤害不超过{1}点" = **伤害锁定（CAP）**：register_damage_effect(CAP, owner,
//     DamageEffectCategory::MITIGATE) 把 final 钳到 {1}——管线序里 CAP 在 FLOOR 之后、
//     BLOCK 之前（"最后再拦一次"），MITIGATE 类别 → 不可被"挡伤失效"无视（官方口径）；
//   - "{0}回合内"含当回合（470 同款口径）→ 回调里判 roundCount < r0+rounds；
//   - ★ **697 凭证消费**：官方 697「无视伤害限制效果」可穿锁伤——回调查
//     `ws.attack_credential[attacker].ignore_damage_limit`（query_usage 每次攻击现物化，
//     skills.cpp materialize_attack_credential）。该凭证此前"写入完备却零消费"
//     （damage_pipeline.h 头注记档的死凭证），本实现是它的第一个消费者，正式转活。
//   - 载体：移山填海 17357 [766]|[3,200]、风灵破 30591 [766,593]|[3,200,0,4,35]
//    （全库共 36 载体，切片统一 [rounds, cap]）。
// 811（args_num=0）"使自身下次受到的伤害减少n点，n等于本回合自身造成的伤害"：
//   - n 的累计口径：**EVENT_TAKE_DAMAGE watcher 本回合累计**（ev.actor==self &&
//     ev.target==opp，amount=实际扣血；红/粉/真/属性四条 emit 路径都发 → 字面
//     "造成的伤害"全计）。为什么不用 resolvedDamage 快照：FSM 状态序是
//     SKILL_EFFECT → ATTACK_DAMAGE（battleFsm handle_*SkillEffect 先于 handle_*AttackDamage），
//     811 效果运行时**自己的技能伤害还没打出**，快照恒 0 → 快照口径结构性不成立；
//     也不用"对手名下落血累计"——事件是"入队+晚 drain"（每个 State 桶跑完后 drain），
//     同回合反打时 drain 已发生过（自身 ATTACK_DAMAGE 桶后）→ 时序恰好闭合。
//   - watcher duration=1（只在技能使用当回合存活，event_center 派发按
//     current-register>=duration 撤）→ n 天然冻结在"本回合"，下回合起不再累计。
//   - 消费：register_damage_effect(REDUCE_FLAT, self, MITIGATE) 一次性回调——
//     "下次"=一次性别忘消费（n=0 也消费，字面口径）；按点数扣减 d.final（下限 0）。
//     只挂红伤管线（粉伤管线不注册，记档：此类模板官方语境均为攻击伤害）。
//   - 载体：王·逆流加农炮 17946 [760,610,811]|[1]、王·叱咤风云 19294 [797,811]|[2]、
//     X·圣盾 32332 [811]|[]（全库 34+ 载体；811 无参，owner/opp 由装载器注入）。
// 1034（args_num=1）"消除对手回合类效果，消除成功则免疫下{0}回合受到的攻击伤害"：
//   - DAMAGE 票回合窗（794 直系同款：break_round_effects SUCCESS 门槛 +
//     grant_immune(DAMAGE, duration={0}, counts=0)——crawford 46 是 counts 型先例，
//     本条是窗口型；沧岚 4 滴抑制门自动覆盖票据）；
//   - ⚠️ 口径记档："下{0}回合"按 794 同款 **含当回合**落（窗口 [R, R+{0}-1]）——
//     IMMUNE 票的 window_expired 只有终门没有起算门（rule_center.h:1738，
//     current-register>=duration 即过期、无"起点前不生效"判定），915/979 的
//     NextRounds 起点语义在票模型上表达不了；协调者指令"794 直系同款"，照办记档。
//   - 载体：破碎铁拳冲 31285 [1034,132]|[1]、翔空天意决 19936 [1034,192]|[1,33]
//    （全库 32+ 载体）。
//
// 前置三查（2026-09-29）：①内核无 766/811/1034 硬编码（grep skills.cpp/
// battle_primitives.cpp/battleFsm.cpp 无命中）②effect_meta 无穿透分类牵连
// ③全库无既有实现（SKILL_EFFECT 零注册）。
//
// 场景回归：test/scenario/scenario_620_mitigate_cap.cpp（含 697 穿 CAP 集成）。

#include "common.h"

#include <memory>

namespace {

// ── 766 —— 消除对手能力提升状态，消除成功则{0}回合内对手造成的攻击伤害不超过{1}点 ──
EffectResult effect_766_break_then_cap(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int cap = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛（消强族同款，868/1010 先例）
    }
    const int r0 = ctx->roundCount;
    // CAP 阶段首个插件生产者（此前零注册，见文件头）。ON_STAGE 绑定：换宠即作废
    //（条目由 pipeline_valid_id epoch 惰性失效，816 同款选型）。
    ctx->register_damage_effect(
        DamagePhase::CAP, owner, DamageEffectCategory::MITIGATE,
        [owner, opp, rounds, cap, r0](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            // 只拦"被消强对手打出的"红伤（isRed 含攻击红伤与属性直伤——官方
            // "锁伤会生效"于属性伤害，deal_attribute_damage 的 CAP 阶段子集同口径）。
            if (d.attackerId != opp || !d.isRed || d.final <= 0) {
                return;
            }
            if (c->roundCount >= r0 + rounds) {
                return;   // "{0}回合内"含当回合（[r0, r0+rounds-1]）
            }
            // ★ 697「无视伤害限制效果」凭证可穿锁伤——该凭证此前写入完备却零消费
            //（damage_pipeline.h 记档的死凭证），本行是它的第一个消费者。
            // 属性伤害路径由 deal_attribute_damage 的 pierce_limit 在调用侧已踢掉 CAP，
            // 这里再查一道是攻击路径的判定点（双保险，语义同源）。
            if (c->ws.attack_credential[d.attackerId].ignore_damage_limit) {
                return;
            }
            if (d.final > cap) {
                d.final = cap;   // 锁伤只钳 final（末位再拦一次；816 归零式整段清是 BLOCK 的事）
            }
        });
    return EffectResult::kOk;
}

// ── 811 —— 使自身下次受到的伤害减少n点，n等于本回合自身造成的伤害 ──
// 累计口径见文件头（watcher-only；快照口径因 SKILL_EFFECT 先于 ATTACK_DAMAGE 不成立）。
EffectResult effect_811_next_hit_flat_reduce(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // watcher 与 REDUCE_FLAT 回调共享的累计状态（std::function 要求可拷贝 → shared_ptr）。
    auto acc = std::make_shared<int>(0);
    auto consumed = std::make_shared<bool>(false);
    // ① 本回合"自身造成的伤害"累计：只在技能使用当回合存活（duration=1）→ n 冻结。
    //    ⚠️ 事件是"入队+晚 drain"（每个 State 桶后 drain）——自身 ATTACK_DAMAGE 桶跑完
    //    即投递，同回合对手反打前 n 已就位（时序闭合，见文件头）。
    //    actor==self 过滤：异常 tick 会把 actor 记成对面（tick 方视角），
    //    不过滤会把"对手身上的异常扣血"误计成自己造成（保守口径记档）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/false,
        [self, opp, acc](BattleContext* /*c*/, const BattleEvent& ev) {
            if (ev.actor != self || ev.target != opp || ev.amount <= 0) {
                return;
            }
            *acc += ev.amount;
        },
        WatcherScope::TEAM);
    // ② "下次受到的伤害减少n点"：REDUCE_FLAT（点数减伤）阶段一次性回调。
    //    打盔时管线整段不跑 → 天然不消费（与"打盔不消耗点数减伤"官方口径一致）。
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, acc, consumed](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || *consumed) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;
            }
            *consumed = true;   // "下次"=一次性（n=0 也消费，字面口径）
            if (*acc > 0) {
                d.final = std::max(0, d.final - *acc);
            }
        });
    return EffectResult::kOk;
}

// ── 1034 —— 消除对手回合类效果，消除成功则免疫下{0}回合受到的攻击伤害 ──
EffectResult effect_1034_break_then_immune_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 对手本来就没有回合类效果 → 子句不触发
    }
    // 回合窗口型全挡（794 直系同款，口径记档见文件头）：counts=0（不计数）+
    // duration={0}——沧岚 4 滴抑制门自动覆盖（DAMAGE 票走 BLOCK 阶段同一道门）。
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
        /*duration_rounds=*/rounds, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
        /*source_effect_id=*/1034);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(766, effect_766_break_then_cap)
SKILL_EFFECT(811, effect_811_next_hit_flat_reduce)
SKILL_EFFECT(1034, effect_1034_break_then_immune_window)
