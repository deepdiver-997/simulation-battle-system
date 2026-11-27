// 极渊DS-001 4797 技能组（2026-09-27）
//
// 技能与效果分解（moves 表 mon_id=4797；已有注册直接复用，本文件只写缺的 8 条）：
//   极渊·凋尸 29107（属性，PP5，必中，命中95）: 191 免疫反弹✓ / 2303 ★本文件 /
//       2245 ★本文件 / 843 先制窗口✓
//   极渊·帝丧 29108（属性，PP5，必中，命中95）: 962✓ / 693✓ / 971✓ / 886✓ —— 全复用零新写
//   极渊·堕煞 38018（邪灵物攻100，先3，命中97）: 867 ★本文件 / 1220 消强必暴✓ /
//       2117 异常转化✓（canglan_nutao 泛化实现，[16,23]=流血→诅咒）/ 2321 ★本文件
//   极渊·荡世 38019（机械物攻100，先3，命中97）: 1808 回合类增伤✓ / 1012 ★本文件 /
//       882 变威力✓ / 1679 ★本文件
//   极渊·渎神 38020（邪灵机械物攻300，必中，命中95）: 2322 ★本文件 / 1237 PP归零净化✓ /
//       43 恢复1/N✓ / 1814 ★本文件 / 2320 弱化窗口✓（渎神变体遗留，见文末）
//
// ★ 用户 2026-09-27 点名的两个口径：
//   ① 荡世"自身每处于一种能力提升状态，威力提升20"是**变威力**效果（biligame wiki 悬浮
//      注释："当命中效果对技能威力值进行修改时，会以当前状态重新进行伤害结算覆盖原本的
//      伤害值"）——effect 882 写 `ws.skill_power_view` 自动打 rewritten 标记 → 引擎
//      apply_variable_power_recalc 第二次结算覆盖第一次（battleFsm finish_attack_damage）。
//      1012 吸强在 moves.side_effect 里**排在 882 之前**，吸到的强化当场参与威力计数
//      （B站周测评 cv42524616："先吸取强化后结算伤害……最多提升到 220 威力"；
//      100 基础 + 6 种提升 × 20 = 220 上限与 6 槽能力等级数天然吻合）。
//   ② 1679"造成的伤害高于400则100%令对手流血"的**施加时点在免死检测之后**——游戏内
//      可用荡世高伤击杀"回合开始无异常"的库贝萨（免死条件在回合开始时锁定，当回合内
//      新加的异常不能解锁免死；B站解析 cv17711114 第②条），尸体上仍看得到流血标志。
//      引擎落点：SKILL_EFFECT 只注册延迟器；AFTER_ACTION（裸伤台账已落地、死亡漏斗
//      未跑）读台账分岔——≤400 → 立即吸取 1/3；>400 → 再注册 **OLD_ROUND_END_1** 一次性
//      节点施加流血。OLD_ROUND_END_1 是两条死亡流程后都必经、且必在免死检测之后的统一
//      时点（先手流程致死：FIRST_MOVER_DEATH 漏斗 → 直接跳 OLD_ROUND_END_1；后手致死：
//      SECOND_MOVER_DEATH 漏斗 → OLD_ROUND_END_1；范围即时型免死如顽强在管线内存活钩
//      更早）。免死成功者 hp 已写回 → 流血正常落上——"流血在免死检测之后"由场景 137
//      T2 的存活门结构性地锁死（若提前施加，hp≤0 的存活门会拒收）。
//      已知偏差：目标真死时 apply_anomaly 的存活门（TARGET_DEFEATED）拒收，尸体不留
//      流血标志（游戏尸体会挂标志，纯表现差异）；免死存活、死亡换宠两个可观察路径均
//      与游戏一致（换宠由 target_slot 快照比对挡住，不会错挂到替换宠身上）。
//
// ★ 2322"若自身处于算力限制则本次攻击无法造成伤害"：置 ws.hit_invalid_zero_damage
//   白板归零标记（③层消费点）+ 效果照常结算，不走 ATTACK_NULLIFY 票——完整选型原因
//   见处理器上方注。渎神原文只有"无法造成伤害"子句、没有 1090 族的"且命中效果失效"
//   措辞——挂标记后 1237/43/1814 照常结算（场景 137 T7a 锁）。算力限制标志从
//   pet.on_stage_storage[2414*100+2] 读（键基=魂印注册键 2414）（写入方 soul_lib/jiyuan_ds001_4797.cpp；跨 dylib
//   只信基本类型，bool/int 双读——kongyuan_runtime.h 口径）。
//
// 遗留（不在本批）：渎神 2320 的多段弱化变体（side_effect_arg 切片 [2,-1,0,-1,0,-1,0,2]，
//   {1}~{6} = -1,0×3 = 攻/特攻/速三段 -1）——现行 2320 处理器是单 stat 槽语义
//   （冥府真诰变体，stat=-1 命中参数校验直接 no-op），且该编码不含"槽位映射"信息，
//   攻/特攻/速的对应关系只能靠官方文本硬编码；jinglingwang2 线对 2320 有未修诊断工单
//   （台账 2026-09-26"冥府真诰 2320 参数错位+补偿缺失已诊断未修"），模板族扩展归并到
//   那条线统一做，本批不动共享处理器。
#include "common.h"

namespace {

constexpr int kJiyuanPetId = 4797;
// 算力限制标志（soul_lib/jiyuan_ds001_4797.cpp 的 kRestrictedKey，kJiyuanMarkId*100+2）。
// ⚠️ 键基是魂印**注册键 2414**（effect_icon.effect_id，2026-09-27 对齐批修正）——
//    不是 monster id 4797；魂印侧 kJiyuanMarkId=2414，写 241402，本侧必须同源。
// 两处各持一份字面量：soul 侧常量在其 dylib 匿名命名空间里取不到，跨 dylib 靠键值约定。
constexpr int kSuanliRestrictedKey = 2414 * 100 + 2;

// AFTER_ACTION / ACTION_END 桶的当前行动方（preemptive_right 推，与 FSM 同源；
// burn_trio_skills.cpp acting_side_of_bucket 同款）。
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

// 现代施加的"落地"判定（burn_trio_skills.cpp anomaly_landed 同款口径）：
// SUCCESS / 替换控场 / 同种延长都算触发；CONVERTED（被转化成别的异常）与各挡下属"未触发"。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 867 消除对手回合类效果，消除成功使对手下回合先制-{0} ──────────────────
// "消除成功"按 break_round_effects 返回值判（SUCCESS 才算，免断 IMMUNE 不算——
// 与 1808 的前置判定相比多出免断区分）。"下回合先制-{0}" → NextRounds 窗口 +
// MOVE_RIGHT 时点写对手 preemptive_level（2095"未触发则下N回合先制-X"同款落点）。
EffectResult effect_867_tick_preempt_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[0];
    const int down = args.int_args[2];
    if (opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[opp] -= down;
    return EffectResult::kOk;
}

EffectResult effect_867_break_rounds_preempt_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int down = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // "消除成功"门槛：没有可清物/免断都不触发
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 867,
                                /*rounds=*/1, {opp, 0, down}, &effect_867_tick_preempt_down);
    return EffectResult::kOk;
}

// ── 1012 吸取对手能力提升状态，吸取成功则为自身附加{0}点护盾 ────────────────
// 吸取走 transfer_stat_boosts 真原语（872/1044 同款判法：搬走 0 项 = 吸取失败）。
// 在荡世效果列里排 882 之前 → 吸到的强化当场参与"每种提升威力+20"计数（文件头口径①）。
EffectResult effect_1012_drain_boosts_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败 → 不附护盾
    }
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, amount, /*source_id=*/1012,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1679 造成的伤害高于{0}则{1}%令对手{2}，未触发则吸取对手最大体力的1/{3} ──
// 阈值判据读裸伤台账（1258/雷解同口径：公式值，挡伤/锁伤改不了）。
// 流血半支的施加时点 = OLD_ROUND_END_1（免死检测之后），见文件头口径②。
EffectResult effect_1679_bleed_flush(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, target_slot, anomaly, chance, arm_round}
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core
        || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int target_slot = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int chance = args.int_args[4];
    const int arm_round = args.int_args[5];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    if (ctx->on_stage[opp] != target_slot) {
        return EffectResult::kOk;   // 已死亡换宠：不把流血错挂到替换宠身上
    }
    ElfPet& target = ctx->seerRobot[opp].elfPets[target_slot];
    if (target.hp <= 0) {
        return EffectResult::kOk;   // 真死：存活门拒收（尸体不留标志，引擎偏差记档）
    }
    // chance=100 时 roll_percent 短路真、不消耗 rand（100% 流血确定性路径）
    if (roll_percent(ctx->rng(), chance)) {
        (void)g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_1679_after_action(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, threshold, chance, anomaly, drain_denom, arm_round}
    if (!ctx || !args.int_args || args.int_count < 7) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || args.int_args[6] != ctx->roundCount || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    const int dealt = ctx->ws.raw_attack_damage[owner];
    if (dealt <= args.int_args[2]) {
        // 未触发（≤阈值，=={0} 不算"高于"）→ 吸取 1/N；目标已死由 drain 的
        // TARGET_DEFEATED 挡下（与血条门同族偏差）。
        if (g_core && g_core->drain_hp && args.int_args[5] > 0) {
            (void)g_core->drain_hp(ctx, owner, opp, args.int_args[5]);
        }
        return EffectResult::kOk;
    }
    // 高于阈值 → 流血延后到 OLD_ROUND_END_1（免死检测之后）再施加。
    const std::vector<int> payload{owner, opp, ctx->on_stage[opp],
                                   args.int_args[4], args.int_args[3], args.int_args[6]};
    Effect e(1679, 0, owner, -1, EffectArgs(payload), &effect_1679_bleed_flush);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OLD_ROUND_END_1, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 1679;
    ctx->register_skill_effect(State::BATTLE_OLD_ROUND_END_1, owner, std::move(ce));
    return EffectResult::kOk;
}

EffectResult effect_1679_threshold_bleed_or_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, args.int_args[1], args.int_args[2],
                                   args.int_args[3], args.int_args[4], args.int_args[5],
                                   ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1679, 0, owner, -1, EffectArgs(payload), &effect_1679_after_action);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1679;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1814 {0}回合内每回合使用技能{1}%令对手{2}，未触发则吸取对手最大体力的1/{3} ──
// 窗口载荷/触发条件照 2320（hundun_diwang）口径：施放当回合即首回合；"每回合使用技能"
// 以 roundChoice==SELECT_SKILL 为准（嗑药/换宠不算）；"未触发"= 概率没中或施加未落地
// （immune/抗性/转化族），吸取作补偿。
EffectResult effect_1814_after_action(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, chance, anomaly, drain_denom}
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (roll_percent(ctx->rng(), args.int_args[2]) && g_core->apply_anomaly) {
        landed = anomaly_landed(
            g_core->apply_anomaly(ctx, opp, args.int_args[3], random_anomaly_duration(),
                                  owner));
    }
    if (!landed && g_core->drain_hp && args.int_args[4] > 0) {
        (void)g_core->drain_hp(ctx, owner, opp, args.int_args[4]);
    }
    return EffectResult::kOk;
}

EffectResult effect_1814_submission_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, args.int_args[1], args.int_args[3],
                                   args.int_args[4], args.int_args[5]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1814, rounds,
                          payload, &effect_1814_after_action);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1814, rounds,
                          payload, &effect_1814_after_action);
    return EffectResult::kOk;
}

// ── 2245 己方每有一只精灵阵亡则为自身附加{0}点护盾、{1}点护罩 ──────────────
// "阵亡" = hp≤0 且非消逝（is_vanished 排除；消逝离场不是阵亡）。护盾/护罩都吃
// burn_trio 2418 同款直写 bank（priority 0、无时长）。
EffectResult effect_2245_shield_hood_per_dead(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int shield = args.int_args[2];
    const int hood = args.int_args[3];
    if (owner < 0 || owner > 1 || shield <= 0 || hood <= 0) {
        return EffectResult::kOk;
    }
    int dead = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->is_vanished(owner, slot)) {
            continue;
        }
        if (ctx->seerRobot[owner].elfPets[slot].hp <= 0) {
            ++dead;
        }
    }
    if (dead <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    self.shield_bank_.add(/*priority=*/0, shield * dead, /*source_id=*/2245, /*duration=*/0);
    self.hood_bank_.add(/*priority=*/0, hood * dead, /*source_id=*/2245, /*duration=*/0);
    return EffectResult::kOk;
}

// ── 2303 自身为满体力时消耗自身最大体力的1/{0}并令对手{1}，自身不为满体力时
//    吸取对手最大体力的1/{2}并令对手所有技能PP值-{3} ────────────────────────
// 满体力判定照 2095（hp ≥ max）；自耗体力直写 hp（crawford 1563"损失自身"先例；
// 满血扣 max/2 无死亡风险）。流血走现代施加（技能附带异常）。
EffectResult effect_2303_full_hp_branch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int cost_denom = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int drain_denom = args.int_args[4];
    const int pp_down = args.int_args[5];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || cost_denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp >= max_hp_of(ctx, owner)) {
        // 满体力：消耗自身最大体力的 1/N + 令对手异常
        ElfPet& self = ctx->getPet(owner);
        self.hp -= max_hp_of(ctx, owner) / cost_denom;
        if (g_core->apply_anomaly && anomaly > 0) {
            (void)g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), owner);
        }
        return EffectResult::kOk;
    }
    // 非满体力：吸取 1/N + 对手所有技能 PP-N
    if (g_core->drain_hp && drain_denom > 0) {
        (void)g_core->drain_hp(ctx, owner, opp, drain_denom);
    }
    if (g_core->pp_reduce && pp_down > 0) {
        (void)g_core->pp_reduce(ctx, opp, pp_down);
    }
    return EffectResult::kOk;
}

// ── 2321 造成的攻击伤害低于{0}则令对手{1}回合内属性技能无效 ────────────────
// 阈值判据同 1679 读裸伤台账，但"低于"是严格小于；封属走 seal_skill 回合窗口型
// （2097 同款："N回合内"传 rounds 本身，非"下N回合"不 +1）。伤害落地后判 →
// AFTER_ACTION 延迟器（1382 先例）；目标已死不封。
EffectResult effect_2321_after_action(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, threshold, rounds, arm_round}
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || args.int_args[4] != ctx->roundCount || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (ctx->ws.raw_attack_damage[owner] >= args.int_args[2]) {
        return EffectResult::kOk;   // "低于{0}"：≥阈值不触发
    }
    if (ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].hp <= 0) {
        return EffectResult::kOk;
    }
    g_core->seal_skill(ctx, owner, opp, /*effect_id=*/2321,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/args.int_args[3],
                       /*penetrable=*/true, /*source_slot=*/-1, EffectScope::ON_STAGE,
                       /*hit_invalid=*/false, /*chance_pct=*/100,
                       /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

EffectResult effect_2321_low_damage_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, args.int_args[1], args.int_args[2],
                                   args.int_args[3], ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2321, 0, owner, -1, EffectArgs(payload), &effect_2321_after_action);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 2321;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 2322 使用时解除自身所处的异常状态，若自身处于算力限制则本次攻击无法造成伤害 ──
// 两半都在"使用时"（SKILL_EFFECT）结算：先解自身异常（无条件），再查算力限制标志 →
// 授 ATTACK_NULLIFY 票（文件头有红伤/粉伤/效果三层的完整口径）。
// ★ 实现选型（绕开两个实测定位的坑，2026-09-27）：
//   坑A：ATTACK_NULLIFY 票属回合类（不在纯查询/免断表，clear_round_type 可拆）——在
//        SKILL_EFFECT 授票会被本技第 2 效果 1237"消除双方……回合类效果"当场拆掉；
//   坑B：改在 ATTACK_DAMAGE 桶注册一次性授票节点（duration=-1），条目又使
//        has_round_effects(0) 变真 → 1237 的 break(0) 从 NO_EFFECTS 变 SUCCESS →
//        invalidate_all_round_effects 的 epoch bump 把同技能后继的 43/1814 条目一并
//        作废（被动桶对插件不开放，无可洁注册面）。
//   落点 = 直接置 `ws.hit_invalid_zero_damage[owner]`（③层白板归零标记，
//   finish_attack_damage 在管线后读它把红伤清 0）：ws 字段是插件既有写入口
//   （skill_power_view/preemptive_level 同类）、每回合 reset 自清、不进规则票体系
//   （1237 拆不掉、has_round_effects 不数、无 query_usage ②.6 全量拦截——效果照常）。
//   已知偏差：归零点在伤害管线之后（ATTACK_NULLIFY 在管线之前、护盾不消耗）——对手
//   挂护盾时护盾被正常消耗后伤害归零；渎神无粉伤，粉伤封锁差异不适用。主口径
//   "红伤归零 + 效果照常"两种实现一致，场景 137 T7a 锁。
EffectResult effect_2322_dispel_self_nullify_if_restricted(BattleContext* ctx,
                                                           const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 半支①（无条件）：使用时解除自身所处的异常状态
    if (g_core->dispel_active_anomalies) {
        (void)g_core->dispel_active_anomalies(ctx, owner);
    }
    // 半支②（算力限制 → 本次攻击无法造成伤害）：置③层白板归零标记，见上注。
    const ElfPet& self = ctx->getPet(owner);
    const auto it = self.on_stage_storage.find(kSuanliRestrictedKey);
    if (it != self.on_stage_storage.end()) {
        // 跨 dylib 的 std::any 只信基本类型（kongyuan_runtime.h 口径）；bool（soul 侧
        // 写入方）与 int（场景/跨线种子惯用）双读，类型不对齐时 any_cast 指针形式
        // 静默返 null。
        bool restricted = false;
        if (const bool* v = std::any_cast<bool>(&it->second)) {
            restricted = *v;
        } else if (const int* v = std::any_cast<int>(&it->second)) {
            restricted = *v != 0;
        }
        if (restricted) {
            ctx->ws.hit_invalid_zero_damage[owner] = true;
        }
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(867, effect_867_break_rounds_preempt_down);
SKILL_EFFECT(1012, effect_1012_drain_boosts_shield);
SKILL_EFFECT(1679, effect_1679_threshold_bleed_or_drain);
SKILL_EFFECT(1814, effect_1814_submission_window);
SKILL_EFFECT(2245, effect_2245_shield_hood_per_dead);
SKILL_EFFECT(2303, effect_2303_full_hp_branch);
SKILL_EFFECT(2321, effect_2321_low_damage_seal);
SKILL_EFFECT(2322, effect_2322_dispel_self_nullify_if_restricted);

}  // namespace
