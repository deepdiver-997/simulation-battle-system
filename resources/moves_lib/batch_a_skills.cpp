// 技能组批产 · A 组（2026-09-27，工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
// 本文件实现 7 只"魂印已注册"精灵的缺口效果共 19 条（effect id 归属 A 组独占）：
//   混沌魔君索伦森 3414 : 561（堕世 22745）
//   绮影罗心·织焰 4696  : 916（蛛烟罗织 37681）
//   杰诺 3408           : 842（致命爆破 30956）、756（铁甲游骑兵 30960）
//   珂莱蒂尔 3401       : 948（恶玄急冲 30918）、595（恐惧机甲 24216）、1011（魔能契约 25568）
//   圣光格劳瑞 4004     : 868、835（万钧奔腾 33991）、495（化影神雷 33992）
//   阿尔忒弥斯 4745     : 2274、2009（神驰天苍 29004 / 翎落地茫 29006）、
//                         1293、1655（风萧影抖 29005）
//   混沌魔尊 3683       : 1123（冥刃断灵 30868）、982（邪心黯天决 30872）、
//                         1367（混沌吞噬 26302）、938、731（无影禁锢 32395）
//
// 口径来源：数据库 effect_info（运行时权威）+ 4399 图鉴技能描述（reference/scrape_4399，
// pets_full.jsonl 定性）+ 既有同族实现（注册键查重确认 19 条全部无既有注册）。
// 各效果实现要点见各 handler 上方注释；场景回归 test/scenario/scenario_140..146。
//
// ★ 595 的 8 参切片结构（全库 8 个引用逐个与 4399 文本比对解出的编码，2026-09-27）：
//   args = [概率, 异常码, 攻Δ, 防Δ, 特攻Δ, 特防Δ, 速Δ, 命中Δ]
//   后 6 个是"若没有触发"的**能力下降向量**，官方位序 攻/防/特攻/特防/速/命中，
//   映射到引擎槽 [0, 2, 1, 3, 4, 5]（官方能力码 ≠ 引擎 stat 索引，交接文档"静默错位"条）。
//   证据：恐惧机甲 [-2,0,-2,0,-2,-2]="攻击-2，特攻-2，速度-2，命中-2"（防/特防位为 0）；
//         灼目天芒诀 [-1,-1,-1,0,0,0]="攻击、特攻、防御-1"；月影千魂断 [0,0,0,0,0,-2]="命中-2"。
//
// ★ 1655 的 3 参结构（耀之试炼/宁静星渊/蜂花雪月等 8 个引用与 4399 文本比对解出）：
//   官方完整语义 = "N回合内，每回合结束时降低对手所有技能{1}点PP值**并**恢复自身所有
//   技能{2}点PP值"——effect_info 模板"{0}回合内每回合结束后{1}恢复自身所有技能{2}点PP值"
//   把 {1} 写得像修饰词，实际它是**对手 PP 削减量**（风萧影抖 {1}=0 → 不削对手，图鉴
//   文本也确实只写恢复半边）。零头例证：冥辉幻络 [1,1,3]=1回合/对手-1/自身+3。
#include "common.h"

#include <functional>

namespace {

// ── AFTER_ACTION / ACTION_END 桶的当前行动方（jiyuan_ds001_skills.cpp 同款）────────
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

// 现代施加的"落地"判定（burn_trio_skills.cpp anomaly_landed 同款口径）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// "自身处于一种能力提升状态"计数（962 同判据：本体正等级、**含命中**共 6 槽，
// 用户 2026-09-18 口径：提到等级提升下降一律指含命中的 6 种）。
int count_boost_kinds(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// 官方位序 攻/防/特攻/特防/速/命中 → 引擎槽 0攻/1特攻/2防/3特防/4速/5命中。
constexpr int kOfficialStatToSlot[6] = {0, 2, 1, 3, 4, 5};

// ═══════════════════════════════════════════════════════════════════════════
// 561 —— 先出手时当回合对手使用技能后若自身体力为0，则令自身体力等于最大体力
//（堕世 22745，args_num=0；4399 图鉴："若本回合受到致命伤害则强制存活并恢复所有体力"）
//
// 落点 = **死亡拦截器**（DeathInterceptor，复活档 sees_hp_consume=true——引擎口径
// "复活/真2命：触发就可不死"，与残留体力免死相区分）。Arming 条件在 SKILL_EFFECT
// 判"先出手"（acting_side_is_first；后手用堕世不武装——官方"先出手时"限定）。
// 拦截时点在死亡漏斗内（先手方被后手打死 → SECOND_MOVER_DEATH 漏斗询问拦截器），
// 早于"对手技能结算完"的字面时点，但可观察结果一致：宠物**留在场上**、体力=最大。
// 比起"OLD_ROUND_END 补写体力"有两个优点：①死亡补位不会先把它换下场（官方"强制
// 存活"的直义）；②不依赖死亡流程后续时点仍可达。
// 已知口径取态：actor==对手才拦（"对手使用技能后"——自己烧血/自爆不复活，保守取严，
// 实测翻案再放）；窗口 = 施放当回合（roundCount 闭环校验，过期拦截器自然哑火；
// register_death_interceptor 对同 source 幂等重注册，不累积）。
EffectResult effect_561_arm_first_mover_revive(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core
        || !g_core->register_death_interceptor) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手使用：561 不武装
    }
    DeathInterceptor ic;
    ic.source_effect_id = 561;
    ic.owner = owner;
    ic.sees_hp_consume = true;   // 复活档（"强制存活"，消耗体力也不死——见上注）
    const int slot = ctx->on_stage[owner];
    const int arm_round = ctx->roundCount;
    ic.fn = [owner, slot, arm_round](BattleContext* c, int side, int die_slot, int actor,
                                     DefeatCause cause) -> bool {
        (void)cause;
        if (!c || side != owner || die_slot != slot) {
            return false;
        }
        if (c->roundCount != arm_round) {
            return false;   // 只保当回合（"当回合对手使用技能后"）
        }
        if (actor != 1 - owner) {
            return false;   // 非对手所致（自爆/自耗）不复活——保守取严，见头注
        }
        ElfPet& pet = c->seerRobot[side].elfPets[slot];
        pet.hp = std::max(1, pet.numericalBase[NumericalPropertyIndex::HP]);
        return true;   // 契约：拦下必须把 hp 写回 >0，否则视为未拦下
    };
    g_core->register_death_interceptor(ctx, ic);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 916 —— 命中后{0}%使对手{1}，若未触发则恢复自身全部体力（蛛烟罗织 [80,10]=80%混乱）
// "未触发"= 概率没中**或**施加未落地（immune/抗性/转化族，1814 同口径）。恢复走
// heal 原语（吃封回血——官方"恢复体力"就是恢复效果）。
EffectResult effect_916_anomaly_or_full_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // 概率**申报给原语**（2026-09-22 概率闸门管道）：掷骰在原语内、位于亮节族闸门
    // 改写之后——"未触发"含 ROLL_FAILED（闸门压 0/掷点没中）与各被挡档（19531 同款）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (!anomaly_landed(r)) {
        g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner));
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 756 —— 命中后{0}%令对手{1}（铁甲游骑兵 [50,19]=50%瘫痪）
// {1} 是 battle_effects 异常码（现代参数化施加族，2189 同款）→ apply_anomaly 全检查链。
EffectResult effect_756_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // 概率申报版（亮节闸门管辖，916 同口径）
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 842 —— 若自身处于能力提升状态则造成的攻击伤害额外提升{0}%（致命爆破 [50]）
// 条件在生效时刻求值（962 全属性族同口径）；"额外提升"= **非通用增伤**（乘法
// AMP_EXTRA 通道，effect_set_damage_amp_extra——官方 L352"非通用增伤全部乘法计算"）。
EffectResult effect_842_boost_amp_extra(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (count_boost_kinds(ctx, owner) <= 0) {
        return EffectResult::kOk;   // 无任何提升：不增伤
    }
    return effect_set_damage_amp_extra(ctx, args);
}

// ═══════════════════════════════════════════════════════════════════════════
// 948 —— 吸取对手能力提升状态，吸取成功则对手{0}（恶玄急冲 [6]=害怕）
// 吸取走 transfer_stat_boosts 真原语（1012 同款判法：搬走 0 项=失败）。
EffectResult effect_948_drain_boosts_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->transfer_stat_boosts
        || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/target, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败 → 不施加
    }
    (void)g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 595 —— 技能使用成功时，{0}%使对手{1}，若没有触发，则对手{2}
//（恐惧机甲：args=[50, 6, -2,0,-2,0,-2,-2]——编码见文件头★）
// 触发半支 = 现代施加（{1} 是异常码）；未触发半支 = 6 位下降向量逐位 stat_drop
//（各查免弱、各钳 -6，749 全属性族同款；为 0 的位跳过）。
EffectResult effect_595_anomaly_or_stat_vector(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 10 || !g_core || !g_core->apply_anomaly
        || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // 概率申报版（亮节闸门管辖）：ROLL_FAILED（含闸门压 0）即"没有触发"→ 走补偿向量
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (anomaly_landed(r)) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[4 + i];
        if (delta == 0) {
            continue;
        }
        // 负 delta = 弱化 → stat_drop 的 amount 是**降幅 magnitude**（正数，effect_5 同款换算）；
        // 正 delta（罕见）→ stat_change 给对手加提升。
        if (delta < 0) {
            (void)g_core->stat_drop(ctx, target, kOfficialStatToSlot[i], -delta, owner);
        } else {
            (void)g_core->stat_change(ctx, target, kOfficialStatToSlot[i], delta);
        }
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1011 —— {0}回合内有{1}%的概率免疫对手攻击技能造成的伤害并直接扣除对手等量体力
//（魔能契约 [4,50]：4 回合、50%；官方文本"受到攻击时有50%概率免疫并反弹攻击伤害"）
//
// 落点 = 伤害管线 **BLOCK 阶段**插件条目（register_damage_effect，BLOCK 类别——
// 是"挡伤"，可被"挡伤失效"（蚀砚之泪族）抑制，官方归类正确）。ON_STAGE scope：
// 携带者下场条目随 epoch 作废。窗口判法同 803（roundCount 半开区间）。
// 命中时整段快照归零（同 core 默认 BLOCK 的归零形状，防下游读幻影伤害），"等量"
// 取归零前的 d.final（减伤后、挡下前）——记入 pending，在**攻击方 AFTER_ACTION**
// 一次性节点里以固定伤害结算（"直接扣除等量体力"吸取口径=2548：固伤+按实际回？
// 不——官方只说扣对手，没有回血半边，就只扣）。
// ⚠️ 不在管线回调里直接 deal_pink_damage：管线 walk 中再入结算/死亡漏斗是
//    use-after-free 级风险（mortarion 091 教训），故一律延后到 AFTER_ACTION 结算。
// ⚠️ 概率掷点在管线回调内（每次攻击一掷）；多段连击 ×N 已并入 final，一次判定。
// pending 记账走 plugin_storage（键段 9901011xx，新增键段已 grep 无撞）。
constexpr int k1011PendingBase = 990101101;   // +owner：待扣除量（累计）
constexpr int k1011ArmBase = 990101103;       // +owner：窗口起点回合（校验用）

EffectResult effect_1011_block_tick_settle(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_1011_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->plugin_storage[k1011ArmBase + self] = start;
    ctx->plugin_storage[k1011PendingBase + self] = 0;

    // ① BLOCK 条目：窗口内每次"对手以攻击技能打我"掷 {1}% → 归零 + 记 pending。
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, opp, chance, start, expire](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || d.attackerId != opp) {
                return;   // 只管"对手打我"
            }
            if (!d.isRed || d.isDirect || d.final <= 0) {
                return;   // "攻击技能造成的伤害"：红伤且非属性直伤
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外
            }
            if (!roll_skill_percent(*c, 1011, 0, self, opp, chance)) {
                return;   // 本回合没触发：照常受伤
            }
            const int taken = d.final;
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            // 累计（连击/额外行动同回合多次攻击）；plugin_storage 是 any 槽，读改写。
            int& slot_ref = [&]() -> int& {
                auto it = c->plugin_storage.find(k1011PendingBase + self);
                if (it == c->plugin_storage.end()) {
                    it = c->plugin_storage.emplace(k1011PendingBase + self, 0).first;
                }
                return std::any_cast<int&>(it->second);
            }();
            slot_ref += taken;
        });

    // ② 攻击方 AFTER_ACTION 一次性结算：把 pending 以固定伤害"直接扣除"。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, opp, 1011, rounds,
                              {self, opp, start, expire}, &effect_1011_block_tick_settle);
    }
    return EffectResult::kOk;
}

EffectResult effect_1011_block_tick_settle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int start = args.int_args[2];
    const int expire = args.int_args[3];
    if (bucket_acting_side(ctx) != opp) {
        return EffectResult::kOk;   // 只在攻击方的行动后结算
    }
    auto it = ctx->plugin_storage.find(k1011PendingBase + self);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) <= 0) {
        return EffectResult::kOk;
    }
    const int pending = any_int_or(it->second);
    it->second = 0;
    if (ctx->roundCount < start || ctx->roundCount >= expire) {
        return EffectResult::kOk;   // 窗口已过（防御性；正常路径 pending 当回合清）
    }
    if (ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].hp > 0) {
        (void)g_core->fixed_damage(ctx, opp, pending);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 495 —— 若对手处于{0}状态，则{1}%概率秒杀对手（化影神雷 [0,10]：对手麻痹→10%秒杀）
// 时点 = AFTER_ACTION 延迟器（1382/1679 同款）：伤害落地后判秒杀，避免"先秒后伤"
// 的乱序与死亡漏斗重入；对手已被本次攻击打死则无需再秒（存活门）。
// 秒杀走 force_hp_to_zero 原语（秒杀体系统一落点：免疫票/短路转化/EVENT_HP_TO_ZERO）。
EffectResult effect_495_kill_flush(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, anomaly, chance, arm_round}
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->has_active_anomaly || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int chance = args.int_args[3];
    const int arm_round = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || arm_round != ctx->roundCount || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    ElfPet& target = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (target.hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：无需再秒
    }
    if (!g_core->has_active_anomaly(ctx, opp, anomaly)) {
        return EffectResult::kOk;   // 前提：对手处于{0}状态（结算后复判）
    }
    if (roll_skill_percent(*ctx, 495, 0, owner, opp, chance)) {
        (void)g_core->force_hp_to_zero(ctx, opp, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_495_paralysis_kill(BattleContext* ctx, const EffectArgs& args) {
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
        Effect e(495, 0, owner, -1, EffectArgs(payload), &effect_495_kill_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 495;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 835 —— 反转自身能力下降状态，反转成功则恢复自身全部体力（万钧奔腾，args_num=0）
// 反转挂 AFTER_ACTION（521/1221 口径：攻击技能先结算伤害再反转，反转不参与本次
// 伤害——本次用反转前等级，提升留给下次）。"反转成功"= REVERSED（有下降可反）。
EffectResult effect_835_reverse_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_reversal
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int arm_round = args.int_args[1];
    if (owner < 0 || owner > 1 || arm_round != ctx->roundCount
        || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) == StatReversalResult::REVERSED) {
        g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner));
    }
    return EffectResult::kOk;
}

EffectResult effect_835_reverse_own_drops_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(835, 0, owner, -1, EffectArgs(payload), &effect_835_reverse_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 835;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 868 —— 消除对手能力提升状态，消除成功则令对手下回合所有技能先制-{0}
//（万钧奔腾 [2]）。"消除成功"= clear_stat_boosts 清掉数 > 0（485 同判法）；
// "下回合先制-{0}" = NextRounds(1) + MOVE_RIGHT tick（867 同款落点，先手权处
// memset 之后写 → 生效）。
EffectResult effect_868_tick_preempt_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[0];
    const int down = args.int_args[2];
    if (target < 0 || target > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[target] -= down;
    return EffectResult::kOk;
}

EffectResult effect_868_clear_boosts_preempt_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int down = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 消除失败（无提升/被免消除挡下）→ 后续不触发
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 868,
                                /*rounds=*/1, {target, 0, down}, &effect_868_tick_preempt_down);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1123 —— 吸取对手能力提升状态，吸取成功则下{0}回合自身所有技能先制+{1}
//（冥刃断灵 [1,2]）。吸取=transfer_stat_boosts；"下{0}回合"=NextRounds 窗口 +
// MOVE_RIGHT tick 给自己加先制（843/867 同族落点）。
EffectResult effect_1123_tick_preempt_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[2];
    if (owner < 0 || owner > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

EffectResult effect_1123_drain_boosts_preempt_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0 || bonus <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/target, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1123, rounds,
                                {owner, 0, bonus}, &effect_1123_tick_preempt_up);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 938 —— 使对手全属性-{0}，先出手时效果翻倍（无影禁锢 [1]）
// "全属性"含命中共 6 槽逐项 stat_drop（749 同款：各查免弱、各钳 -6）；
// 翻倍判 is_second_mover（968 同款）。SKILL_EFFECT 同步结算（下降在伤害前生效——
// 与 749"50%令对手全属性-1"同族时点）。
EffectResult effect_938_all_stats_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int n = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    const int amount = ctx->is_second_mover(owner) ? n : n * 2;   // 先出手翻倍（968 同款判向）
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, amount, owner);
    }
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 731 —— 本回合打出致命一击则下{0}回合其它攻击技能必定致命一击（无影禁锢 [1]）
// arming 判 ctx->crit_happened[owner]（query_usage ①.5 在 ON_SKILL_HIT 掷、
// SKILL_EFFECT 时已可见；miss/打盔不进 SKILL_EFFECT → "打出"天然成立）。
// "下{0}回合" = NextRounds 窗口 + ROUND_START tick 重写 ws.must_crit（ws 每回合
// reset → 窗口效果每回合重写，58 圣光气同款）。
EffectResult effect_731_tick_must_crit(BattleContext* ctx, const EffectArgs& args) {
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

EffectResult effect_731_crit_next_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!ctx->crit_happened[owner]) {
        return EffectResult::kOk;   // 本回合没打出致命一击
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 731, rounds,
                                {owner}, &effect_731_tick_must_crit);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 982 —— {0}%使对手{1}，自身每处于一种能力提升状态则概率提高{2}%
//（邪心黯天决 [40,7,10]：基础 40% 疲惫，每种提升 +10%）。提升种数按 6 槽正等级
// 计（962 同判据）；概率 ≥100 由 roll_percent 短路必发。
EffectResult effect_982_boost_scaled_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int per_boost = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    const int chance = base + per_boost * count_boost_kinds(ctx, owner);
    // 概率申报版（亮节闸门管辖）：闸门按本次实际值改写后再掷
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1367 —— 随机附加{0}种异常状态，未触发则{1}回合内令对手使用的属性技能无效
//（混沌吞噬 [2,2]）。随机附加走 attach_random_anomalies 原语（池⊕改写票/概率门/
// 不重复挑选三段账收口，混沌魔尊 1263 魂印首用的同款通道）。
// ★"未触发"取 landed==0（一个都没落上才补偿；部分落地不补偿——原语注释明说
// "消费方按自己的口径读三段"，此处取保守档，实测翻案再放）。
EffectResult effect_1367_random_anomalies_or_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    const int seal_rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, target, anomaly_pool_all(), count, /*probability_pct=*/100, owner,
        ChanceSource::Skill);
    if (r.landed > 0) {
        return EffectResult::kOk;
    }
    // 未触发：{1}回合内对手属性技能无效（"N回合内"=InRounds 窗口，2321 同款传法）
    (void)g_core->seal_skill(ctx, owner, target, /*effect_id=*/1367,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/seal_rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1293 —— 免疫下{0}次对手的攻击，免疫成功则自身全属性+{1}
//（风萧影抖 [1,1]）。次数盔走 2006 的**实测口径范式**（"免疫下N次对手的攻击"在本
// 仓的既有建模 = SEAL 次数盔：拦对手的攻击技能、可穿、带后续子句 → 被穿也消耗）；
// 监听自己那条盔的结算信号（按 grant_id 精确匹配、两种结算都自删监听器——2006
// 注释的完整理由见 selection_seal.cpp），blocked=true（免疫成功）→ 全属性+{1}
//（六项含命中，all_stats_up 走 stat_change 真原语）。
EffectResult effect_1293_count_armor(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int count = args.int_args[2];
    const int up = args.int_count >= 4 ? args.int_args[3] : 0;
    if (self < 0 || self > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    const int grant_id = g_core->seal_skill(
        ctx, /*source=*/self, /*target=*/1 - self, /*effect_id=*/1293,
        /*attribute=*/false, /*attack=*/true, /*count=*/count,
        /*duration_rounds=*/0, /*penetrable=*/true,
        /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
        /*chance_pct=*/100, /*consumed_when_pierced=*/true);
    if (grant_id <= 0) {
        return EffectResult::kOk;
    }
    auto watcher_id = std::make_shared<int>(-1);
    *watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_ARMOR_RESOLVED,
        self,
        ctx->roundCount,
        0,      // 永久窗口（结算时自删兜底）
        false,  // once：事件对任何盔结算都发，靠 grant_id 匹配
        [self, grant_id, watcher_id, up](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.grant_id != grant_id) {
                return;
            }
            c->event_center_.remove_watcher(*watcher_id);   // 生效/被穿都自删
            if (!ev.blocked || up <= 0) {
                return;   // 被穿 → 子句不触发
            }
            all_stats_up(c, self, up);   // 免疫成功 → 自身全属性+{1}
        });
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 1655 —— {0}回合内每回合结束时降低对手所有技能{1}点PP值并恢复自身所有技能{2}点
// PP值（风萧影抖 [4,0,1]：4回合、不削对手、自回1；结构解码见文件头★）。
// "回合结束时" = BATTLE_ROUND_END 窗口 tick（1165 同款注册面）；{1}>0 才削对手
//（pp_reduce 原语，清 PP 必走原语——直写绕过事件）；恢复走 pp_restore。
EffectResult effect_1655_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->pp_reduce
        || !g_core->pp_restore) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int opp_down = args.int_args[3];
    const int self_up = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (opp_down > 0) {
        (void)g_core->pp_reduce(ctx, target, opp_down);
    }
    if (self_up > 0) {
        (void)g_core->pp_restore(ctx, owner, self_up, /*only_empty=*/false);
    }
    return EffectResult::kOk;
}

EffectResult effect_1655_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1655, rounds,
                          {args.int_args[0], args.int_args[1], args.int_args[2],
                           args.int_args[3], args.int_args[4]},
                          &effect_1655_round_end_tick);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2009 —— 若对手处于异常状态则{0}回合内令对手使用的属性技能无效
//（神驰天苍/翎落地茫 [2]）。条件（对手处于任一异常）在生效时刻求值（1048 同判据
// has_any_active_abnormal_status）；封属走 seal_skill 回合窗口型（2321/2097 同款：
// "N回合内"传 rounds 本身，非"下N回合"不 +1）。
EffectResult effect_2009_conditional_seal_attribute(BattleContext* ctx,
                                                    const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 前提不成立
    }
    (void)g_core->seal_skill(ctx, owner, target, /*effect_id=*/2009,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ═══════════════════════════════════════════════════════════════════════════
// 2274 —— 恢复自身全部PP值，每恢复1点吸取对手{0}点体力（神驰天苍/翎落地茫 [30]）
// "恢复全部PP"= 所有有上限的槽补到 maxPP（pp_restore 原语、钳 max、无限 PP 跳过）；
// 恢复点数 = 施放前各槽缺口和（pp_restore 返回槽数不是点数，手工结账）。
// "吸取{0}点体力" = 点数吸血（引擎无该原语，1613/2548 口径：fixed_damage +
// 按实际扣减回血——吃护盾/免粉走固伤通道，回血吃封回血）。
EffectResult effect_2274_restore_pp_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->pp_restore
        || !g_core->fixed_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_point = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || per_point <= 0) {
        return EffectResult::kOk;
    }
    int deficit = 0;
    for (const Skills& s : ctx->getPet(owner).skills) {
        if (s.maxPP > 0 && s.pp >= 0 && s.pp < s.maxPP) {
            deficit += s.maxPP - s.pp;
        }
    }
    if (deficit <= 0) {
        return EffectResult::kOk;   // 没恢复任何点数 → 不吸取
    }
    (void)g_core->pp_restore(ctx, owner, /*amount=*/999, /*only_empty=*/false);
    ElfPet& tgt = ctx->getPet(target);
    const int before = tgt.hp;
    (void)g_core->fixed_damage(ctx, target, deficit * per_point);
    const int dealt = before - tgt.hp;
    if (dealt > 0) {
        g_core->heal_amount(ctx, owner, dealt);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）────────────────────────
SKILL_EFFECT(561, effect_561_arm_first_mover_revive);
SKILL_EFFECT(916, effect_916_anomaly_or_full_heal);
SKILL_EFFECT(756, effect_756_chance_anomaly);
SKILL_EFFECT(842, effect_842_boost_amp_extra);
SKILL_EFFECT(948, effect_948_drain_boosts_anomaly);
SKILL_EFFECT(595, effect_595_anomaly_or_stat_vector);
SKILL_EFFECT(1011, effect_1011_window);
SKILL_EFFECT(495, effect_495_paralysis_kill);
SKILL_EFFECT(835, effect_835_reverse_own_drops_heal);
SKILL_EFFECT(868, effect_868_clear_boosts_preempt_down);
SKILL_EFFECT(1123, effect_1123_drain_boosts_preempt_up);
SKILL_EFFECT(938, effect_938_all_stats_down);
SKILL_EFFECT(731, effect_731_crit_next_must_crit);
SKILL_EFFECT(982, effect_982_boost_scaled_anomaly);
SKILL_EFFECT(1367, effect_1367_random_anomalies_or_seal);
SKILL_EFFECT(1293, effect_1293_count_armor);
SKILL_EFFECT(1655, effect_1655_window);
SKILL_EFFECT(2009, effect_2009_conditional_seal_attribute);
SKILL_EFFECT(2274, effect_2274_restore_pp_drain);
