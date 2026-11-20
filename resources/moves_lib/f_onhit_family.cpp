// F6 · 受击响应族 —— 817×34 / 897×32 / 850×29 / 68×33（2026-09-29，worktree ../agents/f6）
//
// 家族语义："挨打之后的响应"——四条模板各有独立通道，全部纯插件可达、不改内核：
//
// 817（args_num=1）"{0}回合内受到攻击则对手下回合受到的伤害翻倍"：
//   - 受击 watcher（EVENT_TAKE_DAMAGE，E12/1188 同款；"受攻击"= damage_kind==NORMAL，
//     粉伤 tick/真伤/属性直伤不算——E12 口径）；
//   - 触发 → 对**攻击者**授 NextRounds 1 回合窗口（"下回合"= NextRounds 起点，1010 同款），
//     ROUND_START tick 把对手的**受伤**翻倍：`ws.damage_reduce_add[attacker]` 首空槽写
//     **-100**（默认 REDUCE_PCT 回调读 → applyDamageReduction 的 add 钳底恰为 -100
//     = ×2 整倍；同回合多次受击多窗叠加也在钳底封顶，不会超翻倍——"翻倍"即上限）。
//   - ⚠️ 口径记档（与协调者指令的工具提示有偏差，按官方语义落）：指令写的是
//     "对攻击者授 damage_add_pct += 100（1010 tick 同款）"——但 `damage_add_pct[X]` 是
//     **X 造成伤害**的 AMP 通道（bucket_owner==attackerId 才生效），写攻击者会把官方语义
//     反转成"对手下回合**造成**的伤害翻倍"（自残 debuff）。官方模板（effect_info）、看板行
//     （"受击→对手下回合受伤翻倍"）、全部 34 载体的 info 文案（"受到攻击则对手受到的伤害
//     翻倍"，如嗜血之心 24684/神秘玩偶 26548）三处一致为**对手（=攻击者）受到的伤害翻倍**
//     → 按 receive-side 通道落地（damage_reduce_add 是减伤槽、负值即受伤放大，
//     Calculation 的 ±100 钳位天然就是为该用法留的），**窗口 owner 按指令挂在攻击者身上**
//     （破甲标记随攻击者走：换宠/死亡即失）。
//   - 实例：重力反转 24362 [3]、嗜血之心 24684 [3]（全库 34 载体）。
// 897（args_num=2）"未击败对手则令自身下{0}次受到的攻击伤害额外减少{1}%"：
//   - "未击败"在伤害落地后才可知 → AFTER_ACTION 桥（1218/1307 判据：对手在场 hp>0）。
//     与 864/1171 的**常驻**探针（每回合重判）不同：897 官方文本无"每回合"字样，
//     判定归属**本次技能使用** → 探针 duration=1（只在用技当回合存活）+
//     bucket_acting_side 过滤（FIRST/SECOND 双侧注册、只有行动方那条跑）；
//   - 消费 = 次数型减伤：register_damage_effect(REDUCE_PCT, self, MITIGATE) 回调
//     （1984「受到的攻击伤害额外减少{1}%」同款公式 d.final -= d.final*pct/100；
//     只管红伤、非属性直伤——1984 同判据），shared_ptr 计数按次消费：
//     final<=0（被免/被挡空）不消费——没"受到"就不算一次。
//   - 实例：气临九渊怒 19324 [1,50]（全库 32+ 载体）。
// 850（args_num=2）"{0}回合内每回合有{1}%的概率免疫当回合受到的攻击伤害"：
//   - "每回合"窗口（InRounds，126/448 同族）+ 概率授票（roll_percent → grant_immune）：
//     命中 → grant_immune(DAMAGE, duration=1, counts=0)——票的 window_expired 只有终门
//     （register+duration 即过期）→ 恰好只覆盖当回合（1034 记档同款口径）；
//   - 首跳口径（433 先例）：ROUND_START 桶在用技回合已跑过，窗口注册只能覆盖下一回合起
//     → 用技当回合**立即掷一次**，剩余 {0}-1 回合走 NextRounds 窗口的 ROUND_START tick。
//   - 实例：梦生归元 24575 [3,50]（全库 29 载体，{1} 全为 50——场景用统计断言）。
// 68（args_num=1）"{0}回合内受到致死攻击时则余下1点体力"：
//   - ★ 不死族**纯插件可达**（调研结论，推翻"需内核 hp-pin 原语"的预设）：死亡漏斗的
//     **死亡拦截器**（DeathInterceptor，battle_primitives.h / spirit_lifecycle.h）正是
//     "残留体力免死"的原生挂点——`sees_hp_consume=false` 档：官方《赛学选修4—免死》
//     "残留体力效果面对消耗体力效果不能免死"→ HP_CONSUME/自爆族不拦（挡伤失效也不在
//     拦截器压制面，battleContext.h:1793）；拦下时把 hp 写回 >0（写 1 = "余下1点"），
//     defeat_pet 复核后 INTERCEPTED、回合照常继续（battleFsm 死亡漏斗注释）。
//   - 窗口 = {0} 回合 InRounds（round_effect_start_round 折算，后手 N=1 不顺延），
//     拦截器 fn 内做回合闭环（561 堕世同款"过期自然哑火"；register_death_interceptor
//     对同 source 幂等重注册，复用即重新武装）；slot 用武装时快照——换宠即失保护。
//   - 取态记档：只拦 DefeatCause::DAMAGE（"致死**攻击**"=伤害致死；秒杀族自有
//     INSTANT_KILL 票体系不混装；EFFECT 直写归零漏斗外无凭可拦）。actor 不限来源
//     （反伤致死也保）；漏斗不带伤害种类标记，粉伤/真伤致死同拦（取宽，记档）。
//   - 实例：不灭战魂 14813 [1]、星辰守护 20303 [1]（全库 33 载体，{0} 全为 1）。
//
// 前置三查（2026-09-29）：①内核无 817/897/850/68 处理（grep skills.cpp/
// battle_primitives.cpp/battleFsm.cpp 无命中；effect_register_state 无这四个 case）
// ②effect_meta 无牵连分类 ③全库无既有实现（SKILL_EFFECT 零注册）。
//
// 场景回归：test/scenario/scenario_660_onhit_family.cpp。

#include "common.h"

#include <memory>

namespace {

// AFTER_ACTION 桶的当前行动方（batch_b_skills.cpp 同款小工具，插件各自持有一份；
// preemptive_right 推，与 FSM 同源）。
int bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 817 共用：把"攻击者下回合受伤翻倍"写进对手减伤槽的负值（REDUCE_PCT 默认回调读）──
EffectResult effect_817_vulnerable_mark_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int marked = args.int_args[0];   // 被标记方（=当时的攻击者）
    if (marked < 0 || marked > 1) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_reduce_add[marked];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = -100;   // applyDamageReduction add 钳底 -100 = ×2（"翻倍"上限）
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 817 —— {0}回合内受到攻击则对手下回合受到的伤害翻倍 ──
EffectResult effect_817_onhit_vulnerable_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 817, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 817, args,
                     "需要 [owner,target,rounds]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 817, args, "需要 rounds>0");
    // 受击 watcher：{0} 回合窗口（InRounds 含当回合，462/1188 同款注册法），TEAM 作用域。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;
            }
            // "受攻击" = 红伤（E12 damage_kind 口径；tick/真伤/属性直伤不触发）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            const int actor = ev.actor;
            if (actor < 0 || actor > 1 || actor == self) {
                return;   // 自伤不构成"受到攻击"（不给自己发破甲标记）
            }
            // 对攻击者授 NextRounds 1 回合窗口（"下回合"= NextRounds 起点）；
            // 标记 owner=攻击者——换宠/死亡窗口随破甲标记失效。
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, actor, 817,
                                        /*rounds=*/1, {actor, 1 - actor},
                                        &effect_817_vulnerable_mark_tick);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 897 共用：次数型"受到的攻击伤害额外减少{1}%"（REDUCE_PCT 回调，1984 同公式）──
// 计数放 shared_ptr（std::function 要求可拷贝，811 先例）。
EffectResult effect_897_arm_counted_reduce(BattleContext* ctx, int self, int times, int pct) {
    auto counts = std::make_shared<int>(times);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, pct, counts](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || *counts <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            // 只管"自己受到的攻击红伤"（非属性直伤，1984 同判据）。
            if (d.defenderId != self || !d.isRed || d.isDirect || d.final <= 0) {
                return;
            }
            --*counts;   // 按次消费（final<=0 的空担不消费——没"受到"就不算一次）
            d.final -= d.final * pct / 100;
        });
    return EffectResult::kOk;
}

// ── 897 —— 未击败对手则令自身下{0}次受到的攻击伤害额外减少{1}% ──
EffectResult effect_897_no_kill_counted_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 897, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 897, args,
                     "需要 [owner,target,times,pct]");
    const int self = args.int_args[0];
    const int times = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && times > 0 && pct > 0, 897, args,
                     "需要 times>0 && pct>0");
    // AFTER_ACTION 桥：FIRST/SECOND 双侧各挂一条，duration=1（只在用技当回合存活——
    // 判定归属本次技能使用，非常驻重判）；行动方过滤保证只跑自己那条。
    const std::vector<int> payload{self, 1 - self, times, pct};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(897, 0, self, -1, EffectArgs(payload),
                     [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                         if (!c || !a.int_args || a.int_count < 4) {
                             return EffectResult::kOk;
                         }
                         const int me = a.int_args[0];
                         if (bucket_acting_side(c) != me) {
                             return EffectResult::kOk;   // 只在"自己出手流程结束"判
                         }
                         // "未击败"= 伤害落地后对手在场宠还活着（1218/1307 判据）。
                         if (c->getPet(1 - me).hp <= 0) {
                             return EffectResult::kOk;
                         }
                         return effect_897_arm_counted_reduce(c, me, a.int_args[2],
                                                              a.int_args[3]);
                     });
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 897;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 850 共用：掷 {1}% → 免疫**当回合**攻击伤害票（duration=1 恰好只覆盖当回合）──
void effect_850_roll_and_grant(BattleContext* ctx, int self, int pct) {
    if (!roll_percent(pct)) {
        return;
    }
    ctx->rule_center_.grant_immune(
        self, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
        /*duration_rounds=*/1, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
        /*source_effect_id=*/850);
}

EffectResult effect_850_immune_roll_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    effect_850_roll_and_grant(ctx, self, pct);
    return EffectResult::kOk;
}

// ── 850 —— {0}回合内每回合有{1}%的概率免疫当回合受到的攻击伤害 ──
EffectResult effect_850_chance_immune_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 850, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 850, args,
                     "需要 [owner,target,rounds,pct]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && pct > 0, 850, args,
                     "需要 rounds>0 && pct>0");
    // 首跳（433 先例）：ROUND_START 桶本回合已跑过 → 用技当回合立即掷一次；
    // 剩余 rounds-1 跳从下一回合起走 NextRounds 窗口的 ROUND_START tick。
    effect_850_roll_and_grant(ctx, self, pct);
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 850, rounds - 1,
                                    {self, 1 - self, pct}, &effect_850_immune_roll_tick);
    }
    return EffectResult::kOk;
}

// ── 68 —— {0}回合内受到致死攻击时则余下1点体力（死亡拦截器 = 残留体力免死档）──
EffectResult effect_68_lethal_residue_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 68, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 68, args,
                     "需要 [owner,target,rounds]");
    if (!g_core || !g_core->register_death_interceptor) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 68, args, "需要 rounds>0");
    // 窗口 = {0} 回合 InRounds（含当回合；round_effect_start_round 折算后手顺延）。
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    const int slot = ctx->on_stage[self];   // 换宠即失保护（武装时快照）
    DeathInterceptor ic;
    ic.source_effect_id = 68;
    ic.owner = self;
    ic.sees_hp_consume = false;   // 残留体力免死档：消耗体力（自爆族）穿免死，不拦
    ic.fn = [self, slot, start, expire](BattleContext* c, int side, int die_slot, int actor,
                                        DefeatCause cause) -> bool {
        (void)actor;   // 不限来源：反伤致死也保（自伤类走 HP_CONSUME，本档根本不被询问）
        if (!c || side != self || die_slot != slot) {
            return false;
        }
        if (cause != DefeatCause::DAMAGE) {
            return false;   // "致死攻击"=伤害致死；秒杀/消耗体力/直写归零不在本档
        }
        if (c->roundCount < start || c->roundCount >= expire) {
            return false;   // {0} 回合窗口外（过期拦截器自然哑火，561 同款）
        }
        c->seerRobot[side].elfPets[slot].hp = 1;   // "余下1点体力"（契约：写回 >0）
        return true;
    };
    g_core->register_death_interceptor(ctx, ic);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(817, effect_817_onhit_vulnerable_window)
SKILL_EFFECT(897, effect_897_no_kill_counted_reduce)
SKILL_EFFECT(850, effect_850_chance_immune_window)
SKILL_EFFECT(68, effect_68_lethal_residue_window)
