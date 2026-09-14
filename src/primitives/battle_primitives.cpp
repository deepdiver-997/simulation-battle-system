/**
 * battle_primitives.cpp — 原语实现
 *
 * 原语 = 技能/魂印效果程序调用的原子动作。返回结果枚举表达"发生了什么"。
 * 原语通过 BattleContext 的内核（效果桶 / 异常状态 / 事件中心 / 免疫内核）完成动作。
 *
 * 本文件承载：
 * - apply_anomaly    （从 abnormal-system/abnormal-applicator 迁移）
 * - break_round_effects
 */

#include <primitives/battle_primitives.h>

#include <abnormal-system/abnormal-types.h>
#include <effects/continuousEffect.h>
#include <entities/elf-pet.h>
#include <entities/mark.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include <iostream>

namespace {

// 成功（异常状态实际改变）时向事件中心投递事件：
// - EVENT_ANOMALY_APPLIED：任意异常施加成功（第三方"当对方被挂异常时XXX"监听）
// - EVENT_CONTROLLED：控场类异常施加成功（第三方"当对方被控场时XXX"监听）
// emit 只入队，不内联执行；FSM 在 State 桶后 drain 投递。
void emit_anomaly_events(BattleContext* ctx, int target, int anomaly_id, int actor) {
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_ANOMALY_APPLIED, actor, target});
    if (is_control_abnormal_status(static_cast<AbnormalStatusId>(anomaly_id))) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_CONTROLLED, actor, target});
    }
}

} // namespace

// ----------------------------------------------------------------
// apply_anomaly — 施加异常状态的统一入口
//
// 施加流程（按顺序执行的检查链）：
//   [Step 1] 参数校验
//   [Step 2] 目标存活检查
//   [Step 3] 免疫检查 — 免疫内核 is_immune(ANOMALY) + Mark ID 0 兜底
//   [Step 4] 同种异常 — 比较剩余回合，用长的覆盖短的
//   [Step 5] 控场替换 — 已有控场则清除旧控场，施加新控场
//   [Step 6] 特殊阻止检查（预留：抗性/装备/场地/保护机制）
//   [Step 7] 执行施加 — 写入 abnormal_status_end_round，成功路径 emit 事件
//
// 为什么集中在一个函数？
//   1. 异常免疫机制多种多样，分散在各处极易遗漏
//   2. 未来新增阻止条件时，只需在此函数加检查分支，所有调用方自动生效
//   3. 日志/调试在此集中输出
// ----------------------------------------------------------------
// 转化异常（单跳）：入→出。免疫异常(21)也可被转化——"让对手抗性抵抗挂上免疫异常图标，
// 再转化掉"的绕过魂免+抗性 exploit 路径。
static int resolve_anomaly_conversion(BattleContext* ctx, int target, int incoming) {
    const auto& conv = ctx->anomaly_conversion[target];
    const auto it = conv.find(incoming);
    return it != conv.end() ? it->second : incoming;
}

// 内部实现。reflect_depth = 反弹深度（0=原生施加；1=反弹回来：不再反弹、不过抗性）。
static ApplyAnomalyResult apply_anomaly_impl(BattleContext* ctx,
                                             int target,
                                             int anomaly_id,
                                             int duration_rounds,
                                             int actor,
                                             int reflect_depth) {
    // [1] 参数校验
    if (target < 0 || target > 1) {
        return ApplyAnomalyResult::INVALID_PARAM;
    }
    if (!is_valid_abnormal_status_id(anomaly_id)) {
        return ApplyAnomalyResult::INVALID_PARAM;
    }
    if (duration_rounds <= 0) {
        duration_rounds = random_anomaly_duration();
    }

    ElfPet& pet = ctx->getPet(target);

    // [2] 目标存活检查
    if (pet.hp <= 0) {
        return ApplyAnomalyResult::TARGET_DEFEATED;
    }

    // 弹控：目标免疫时把异常反弹给施放方。最多反弹 1 次（depth==1 不再弹）——防双方弹控打乒乓球。
    const auto reflect = [&]() -> ApplyAnomalyResult {
        if (reflect_depth == 0 && ctx->rule_center_.has_reflect(target)
            && actor >= 0 && actor != target) {
            apply_anomaly_impl(ctx, actor, anomaly_id, duration_rounds,
                               /*actor=*/target, /*reflect_depth=*/1);
            return ApplyAnomalyResult::REFLECTED;
        }
        return ApplyAnomalyResult::TARGET_IMMUNE;
    };

    // [3] 次免/回合类免疫（soul=false）—— 官方优先级：先于抗性判定挡下。
    // 挡下后消费（顺序不可颠倒：先判定、后扣次数，否则 counts=1 的次免会在判定前
    // 就被扣光注销，下一次判定查不到它 → 该挡下的没挡住）。
    // ⚠️ 官方规则（docs/02-效果系统/官方机制理解与引擎缺口对照.md §二，idx=418 第2条）：
    //    **存在回合类免控/弹控时，次免依旧正常消耗** —— 故"是窗口条目挡下的"也要扣
    //    次数型条目（consume_immune 会跳过 counts==0 的窗口条目继续扫）。
    if (ctx->is_immune_effect(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id)) {
        // 被弹回来的异常（reflect_depth>0）**不消耗**——"被弹回来的异常不属于精灵受到异常"
        // （idx=375）；但它仍可**被**次免免疫（照常走本分支挡下，只是不扣次数）。
        if (reflect_depth == 0) {
            ctx->consume_immune(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id,
                                /*soul_filter=*/0);
        }
        return reflect();
    }

    // [4] 异常抗性 roll（弹回的异常不过抗性——depth>0 跳过；官方必修3/选修7）。
    // 抗性成功：直写附加"免疫异常"异常(21, 2回合)——**击穿魂免**（不走免疫检查）；
    // 先走转化（攻击方可预置 conversion[target][21]=Y 把抵抗结果直接转成 Y）。
    if (reflect_depth == 0 && pet.resistance.isResistantTo(anomaly_id)) {
        const int resolved = resolve_anomaly_conversion(
            ctx, target, static_cast<int>(AbnormalStatusId::AbnormalImmunity));
        ctx->set_abnormal_status_end_round(target, resolved, ctx->roundCount + 2);
        emit_anomaly_events(ctx, target, resolved, actor);
        return ApplyAnomalyResult::RESISTED_BY_RESISTANCE;
    }

    // [5] 魂免（soul=true）—— 抗性判定失败后才查（官方优先级）。同样挡下后消费次数型。
    if (ctx->is_immune_soul(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id)
        || has_mark(pet.marks, 0)) {
        if (reflect_depth == 0) {  // 弹回的异常不消耗（同 [3]）
            ctx->consume_immune(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id,
                                /*soul_filter=*/1);
        }
        return reflect();
    }

    // [6] 转化（单跳）：入→出，直接施加（绕过抗性/免疫的转换路径）。
    const int resolved = resolve_anomaly_conversion(ctx, target, anomaly_id);
    const bool converted = (resolved != anomaly_id);

    // [7] 同种异常 — 回合覆盖
    bool already_has_same = ctx->has_active_abnormal_status(target, resolved);
    if (already_has_same) {
        int current_end = ctx->get_abnormal_status_end_round(target, resolved);
        int current_remaining = current_end - ctx->roundCount;
        if (duration_rounds > current_remaining) {
            ctx->set_abnormal_status_end_round(target, resolved,
                                               ctx->roundCount + duration_rounds);
            emit_anomaly_events(ctx, target, resolved, actor);
            return converted ? ApplyAnomalyResult::CONVERTED
                             : ApplyAnomalyResult::DURATION_EXTENDED;
        }
        // 新回合数不更长，不覆盖，但也不算失败——异常已经存在
        return converted ? ApplyAnomalyResult::CONVERTED : ApplyAnomalyResult::SUCCESS;
    }

    // [8] 执行施加
    // abnormal_status_end_round[target][anomaly_id] 是异常状态的唯一权威数据源。
    ctx->set_abnormal_status_end_round(target, resolved,
                                       ctx->roundCount + duration_rounds);
    // 成功路径 emit：异常状态实际改变才通知第三方（控场额外发 EVENT_CONTROLLED）
    emit_anomaly_events(ctx, target, resolved, actor);

#ifdef BATTLE_FSM_VERBOSE_DEFAULT
    std::cout << "[apply_anomaly] target=" << target
              << " anomaly=" << abnormal_status_name_cn(resolved)
              << "(" << resolved << ")"
              << " duration=" << duration_rounds
              << " end_round=" << (ctx->roundCount + duration_rounds)
              << (converted ? " [converted]" : "")
              << std::endl;
#endif

    return converted ? ApplyAnomalyResult::CONVERTED : ApplyAnomalyResult::SUCCESS;
}

// 公开入口：原生施加（reflect_depth=0）。
ApplyAnomalyResult apply_anomaly(BattleContext* ctx,
                                 int target,
                                 int anomaly_id,
                                 int duration_rounds,
                                 int actor) {
    return apply_anomaly_impl(ctx, target, anomaly_id, duration_rounds, actor,
                              /*reflect_depth=*/0);
}

// ----------------------------------------------------------------
// break_round_effects — 断回合原语
//
// 返回"发生了什么"，技能据此分支：
//   - SUCCESS   : 清除了目标回合类效果（成功路径 emit EVENT_BREAK）
//   - NO_EFFECTS: 目标没有可清除的回合类效果（清除失败：无事发生）
//   - IMMUNE    : 目标在当前时点免断（本次断回合无效）
// ----------------------------------------------------------------
BreakResult break_round_effects(BattleContext* ctx, int target) {
    if (target < 0 || target > 1) {
        return BreakResult::NO_EFFECTS;
    }

    // 免断检查：目标在当前时点免疫断回合 → 本次断回合无效。
    // 低级免断只覆盖部分时点，未覆盖时点 is_immune 返回 false，照常可断。
    if (ctx->is_immune(target, ImmunityType::BREAK, ctx->currentState)) {
        return BreakResult::IMMUNE;
    }

    // 对手没有可清除的回合类效果 → 清除失败（无事发生），不 emit。
    // 回合类盔/威/封属也算回合类效果（可被断清除）。
    if (!ctx->has_round_effects(target) && !ctx->rule_center_.has_round_type(target)) {
        return BreakResult::NO_EFFECTS;
    }

    // 成功：机械无效化 + 事件（补偿 watcher 在 FSM drain 点投递）
    ctx->invalidate_all_round_effects(target);
    // 断回合清回合类盔/威/封属（次数类保留）。统一锚 source：断"target(被断方)"名下
    // 挂载的回合类拦截——封属属于其施放方，故断施放方回合解封。
    ctx->rule_center_.clear_round_type(target);
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_BREAK, ctx->opponent(target), target});
    return BreakResult::SUCCESS;
}

// ----------------------------------------------------------------
// deal_damage — 伤害原语（统一伤害入口）
//
// 流程：粉伤抗性（固定/百分比）→ 护盾/护罩吸收 → 扣血 → emit EVENT_TAKE_DAMAGE。
// 护盾只响应红伤(NORMAL)、护罩只响应粉伤(FIXED/PERCENT)、真实伤害直通（官方护盾/护罩分离）。
// 被击破时 emit EVENT_SHIELD_BROKEN（对应描述"护盾消失时XXX"，护罩破罩同事件）。
// 攻击方可设 ws.ignore_shield 使本次攻击无视护盾/护罩响应。
// 所有伤害类机制都应走这里，避免效果函数直接改 hp 绕过管线。
// ----------------------------------------------------------------
void deal_damage(BattleContext* ctx, int target, int amount,
                 DamageKind kind, int actor) {
    if (target < 0 || target > 1 || amount <= 0) {
        return;
    }
    ElfPet& pet = ctx->getPet(target);
    if (pet.hp <= 0) {
        return;  // 目标已死亡
    }

    int effective = amount;
    // 粉转真时按原始量（PERCENT 已换算成具体数值）转，故保留 pre_resist。
    int pre_resist = amount;
    if (kind == DamageKind::PERCENT) {
        const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
        effective = max_hp > 0 ? max_hp * amount / 100 : 0;
        if (effective <= 0) {
            return;
        }
        pre_resist = effective;
    }
    // PERCENT_VALUE：amount 已是具体伤害值（如"自身已损失体力50%"），不再换算——
    // 但**分档**仍属百分比伤害（走百分比抗性/护罩），见下方 kind 判定。

    // 粉伤抗性层（固定/百分比伤害）：免疫粉伤 → 对应来源抗性% → 减粉% 逐级削减。
    // 伤害抗性按来源分型：FIXED 走固定抗性、PERCENT 走百分比抗性（官方：暴击/固定/百分比）。
    // ⚠️ 读的是 ws **有效视图**而非 pet 本体——临时 buff 可修改抗性（混元天尊死亡 buff
    //    把己方精灵抗性视为 100%）。视图基线由 sync_damage_resist_view 在回合开始/换宠重基。
    // 被挡下（<=0）且粉转真 → 改以真实伤害结算（直通护盾/护罩、穿抗性/免疫）。TRUE 绕过此层。
    if (kind == DamageKind::FIXED || kind == DamageKind::PERCENT
        || kind == DamageKind::PERCENT_VALUE) {
        const int resist_pct =
            kind == DamageKind::FIXED ? ctx->ws.eff_fixed_resist_pct[target]
                                      : ctx->ws.eff_percent_resist_pct[target];
        if (ctx->pink_immune[target]) {
            effective = 0;
        } else {
            effective -= effective * resist_pct / 100;
            effective -= effective * ctx->pink_reduce_pct[target] / 100;
        }
        if (effective <= 0) {
            if (ctx->pink_to_true[target]) {
                deal_damage(ctx, target, pre_resist, DamageKind::TRUE, actor);
            }
            return;  // 被免粉挡下：目标体力不变（免粉补偿一类在效果侧比较 HP）
        }
    }

    // 护盾/护罩吸收：护盾只响应红伤(NORMAL)，护罩只响应粉伤(FIXED/PERCENT)，真实伤害直通。
    // 攻击方可设 ws.ignore_shield[attacker] 使本次攻击无视护盾/护罩响应（如无极圣武魂印）。
    const bool ignore_bank = (actor >= 0 && actor <= 1 && ctx->ws.ignore_shield[actor]);
    int broken = 0;
    int remaining = effective;
    if (!ignore_bank) {
        if (kind == DamageKind::NORMAL) {
            remaining = pet.shield_bank_.absorb(effective, &broken);
        } else if (kind == DamageKind::FIXED || kind == DamageKind::PERCENT
                   || kind == DamageKind::PERCENT_VALUE) {
            remaining = pet.hood_bank_.absorb(effective, &broken);
        }
        // DamageKind::TRUE：护盾/护罩均不响应，直通
    }
    if (broken > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, actor, target});
    }
    if (remaining <= 0) {
        return;  // 护盾/护罩完全挡下
    }

    // 扣血
    const int hp_before = pet.hp;
    pet.hp -= remaining;
    if (pet.hp < 0) {
        pet.hp = 0;
    }
    const int actual_damage = hp_before - pet.hp;

    // 受到伤害事件（第三方"受到攻击伤害后/受高伤/受低伤"监听），amount = 实际扣血。
    // 带上 emit 当时的 FSM 时点：watcher 在 drain 时才跑，那时 currentState 已经推进，
    // 靠 ctx->currentState 判不出"是攻击伤害还是粉伤"（见 BattleEvent::state）。
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_TAKE_DAMAGE, actor, target,
                                        actual_damage, static_cast<int>(ctx->currentState)});
}

// 粉伤入口（插件可调）：走 deal_damage，返回"发生了什么"。
FixedDamageResult deal_pink_damage(BattleContext* ctx, int target, int amount,
                                   DamageKind kind, int actor) {
    if (!ctx || target < 0 || target > 1 || amount <= 0) {
        return FixedDamageResult::INVALID_PARAM;
    }
    if (kind != DamageKind::FIXED && kind != DamageKind::PERCENT
        && kind != DamageKind::PERCENT_VALUE) {
        return FixedDamageResult::INVALID_PARAM;  // 只做粉伤；红伤/真伤另有入口
    }
    deal_damage(ctx, target, amount, kind, actor);
    return ctx->getPet(target).hp <= 0 ? FixedDamageResult::TARGET_DEFEATED
                                       : FixedDamageResult::SUCCESS;
}

int seal_skill(BattleContext* ctx, int source, int target, int effect_id, bool attribute,
               bool attack, int count, int duration_rounds, bool penetrable, int source_slot,
               EffectScope scope, bool hit_invalid, int chance_pct, bool consumed_when_pierced) {
    if (!ctx || source < 0 || source > 1 || target < 0 || target > 1 || count <= 0) {
        return 0;
    }
    if (!attribute && !attack) {
        return 0;
    }
    // 统一调度到 RuleCenter：source=挂载(施放)方，target=生效(被封)方。scope 默认 ON_STAGE。
    // 命中失效语义(SEAL_ATTRIBUTE_HIT)只在"封属性技能"下有意义。
    SealKind kind = (attribute && attack) ? SealKind::SEAL_ALL
        : (attack ? SealKind::SEAL_ATTACK : SealKind::SEAL_ATTRIBUTE);
    if (hit_invalid && attribute && !attack) {
        kind = SealKind::SEAL_ATTRIBUTE_HIT;
    }
    // 概率封属（如 effect 695「{0}回合内{1}%令对手使用的属性技能无效」）：
    // 概率**每次响应时**掷（不是授予时掷一次）——官方表现是"每回合都有机会封住"，
    // 而非"授予时决定这几回合封不封"。走 RuleTicket::condition（notify 逐条求值点）。
    std::function<bool(BattleContext*, int, int, bool)> condition = nullptr;
    if (chance_pct < 100) {
        const int chance = chance_pct <= 0 ? 0 : chance_pct;
        condition = [chance](BattleContext*, int, int, bool) {
            if (chance <= 0) {
                return false;
            }
            return (std::rand() % 100) < chance;
        };
    }
    // 回合型 / 次数型**二选一**（文档 §5.1）：有 duration 走回合型（remaining_counts 不设，
    // 响应不消耗，靠 tick/断回合结束）；否则走次数型（响应即减，减到 0 注销）。
    if (duration_rounds > 0) {
        return ctx->rule_center_.grant_seal(source, source_slot, effect_id, target, kind,
                                            /*counts=*/0, duration_rounds, penetrable, scope,
                                            std::move(condition), ctx->round_effect_valid_id[source],
                                            consumed_when_pierced);
    }
    return ctx->rule_center_.grant_seal(source, source_slot, effect_id, target, kind,
                                        count, /*rounds=*/0, penetrable, scope,
                                        std::move(condition), ctx->round_effect_valid_id[source],
                                        consumed_when_pierced);
}

void hit_effect_invalid(BattleContext* ctx, int target, HitInvalidMode mode,
                        int count, bool is_attribute_skill, int source_id) {
    if (!ctx || target < 0 || target > 1 || count <= 0) {
        return;
    }
    // ③层并入 RuleCenter（HIT_INVALID_ATTACK / HIT_INVALID_ATTRIBUTE 两类）：
    // target=被失效方(防御方)。source_id 无 effect 语义，此处作覆盖键来源占位（同 target+mode 刷新）。
    // is_attribute_skill 决定挂哪一类——要"两种技能都失效"就调两次本原语。
    ctx->rule_center_.grant_hit_invalid(target, /*source_slot=*/-1, source_id, static_cast<int>(mode),
                                        count, is_attribute_skill);
}

StatChangeResult stat_change(BattleContext* ctx, int target, int stat, int delta) {
    if (!ctx || target < 0 || target > 1 || stat < 0 || stat >= 6) {
        return StatChangeResult::INVALID_PARAM;
    }
    ElfPet& pet = ctx->getPet(target);
    int& level = pet.levels[stat];
    const int new_level = level + delta;
    if (new_level > 6 || new_level < -6) {
        return StatChangeResult::AT_CAP;  // 到上限/下限，不变更
    }
    level = new_level;
    // 同步到 ws 视图：伤害公式读 ws.view_levels（每回合从本体重基），回合内改本体必须同刷视图，
    // 否则本体/视图分裂（如反转后 view 残留旧负值触 getTempAbilityValue 除零 → INT_MAX）。
    ctx->ws.view_levels[target][stat] = new_level;
    return StatChangeResult::SUCCESS;
}

// 内部：按指定量恢复，返回实际恢复量（封回血检查 + 恢复效果修正 + 记录 last_heal_amount）。
static int heal_impl(BattleContext* ctx, int target, int heal_amount) {
    ElfPet& pet = ctx->getPet(target);
    if (heal_amount <= 0) {
        ctx->ws.last_heal_amount[target] = 0;
        return 0;
    }
    // 封回血：本时点覆盖内封锁体力回复（位覆盖仿魂免——coverage 全置位=闭环恒封；
    // 只含部分时点=低级，未覆盖时点的恢复有效）。
    if (ctx->is_immune(target, ImmunityType::HEAL_BLOCK, ctx->currentState)) {
        ctx->ws.last_heal_amount[target] = 0;
        return 0;
    }
    // 恢复效果修正%（正=提升，负=降低；封回血=-100 等价归零）。
    heal_amount = heal_amount * (100 + ctx->heal_mod_pct[target]) / 100;
    if (heal_amount < 0) {
        heal_amount = 0;
    }
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    const int hp_before = pet.hp;
    pet.hp = std::min(max_hp, pet.hp + heal_amount);
    const int actual = pet.hp - hp_before;
    ctx->ws.last_heal_amount[target] = actual;
    return actual;
}

HealResult heal(BattleContext* ctx, int target, int fraction_denom) {
    if (!ctx || target < 0 || target > 1) {
        return HealResult::INVALID_PARAM;
    }
    ElfPet& pet = ctx->getPet(target);
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    int heal_amount = 0;
    if (fraction_denom > 0) {
        heal_amount = max_hp > 0 ? max_hp / fraction_denom : 0;
    } else {
        heal_amount = max_hp;  // 恢复全部
    }
    heal_impl(ctx, target, heal_amount);
    return HealResult::SUCCESS;
}

HealResult heal_amount(BattleContext* ctx, int target, int amount) {
    if (!ctx || target < 0 || target > 1 || amount < 0) {
        return HealResult::INVALID_PARAM;
    }
    heal_impl(ctx, target, amount);
    return HealResult::SUCCESS;
}

int clear_stat_boosts(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return 0;
    }
    // 免消除强化（能力提升无法被消除或吸取，如希拓神煌炎舞斩 1960）：整次消除失败。
    // 放在最前面——否则"消强成功→后续分支（必先/固伤）"会被误触发。
    if (ctx->is_immune(target, ImmunityType::STAT_CLEAR, ctx->currentState)) {
        return 0;
    }
    ElfPet& pet = ctx->getPet(target);
    int cleared = 0;
    for (int i = 0; i < static_cast<int>(pet.levels.size()); ++i) {
        if (pet.levels[i] > 0) {  // 只清提升（正等级），不动弱化/负等级
            pet.levels[i] = 0;
            // 本体/视图同步：视图是伤害公式的读取源，不同步会让本次消强在伤害上"没发生"
            // （反例：INT_MAX 那次本体/视图分裂）。
            ctx->ws.view_levels[target][i] = 0;
            ++cleared;
        }
    }
    return cleared;  // 0 = 目标本无提升（消强未成功）
}

int clear_stat_drops(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return 0;
    }
    // ⚠️ 与 clear_stat_boosts 不对称：**故意不查任何免疫**。
    //   清弱化对目标有利——免弱(STAT_DROP) 挡"施加弱化"、免消除强化(STAT_CLEAR) 护"提升"，
    //   都不该挡"把弱化拿掉"（用户 2026-09-13 口径："清理弱化什么都不用查"）。
    ElfPet& pet = ctx->getPet(target);
    int cleared = 0;
    for (int i = 0; i < static_cast<int>(pet.levels.size()); ++i) {
        if (pet.levels[i] < 0) {  // 只清弱化（负等级），不动提升/正等级
            pet.levels[i] = 0;
            ctx->ws.view_levels[target][i] = 0;   // 本体/视图同步（同 clear_stat_boosts）
            ++cleared;
        }
    }
    return cleared;
}

bool crit_defense_break(BattleContext* ctx, int defender, int skill_type) {
    if (!ctx || defender < 0 || defender > 1) {
        return false;
    }
    // 0=物理 → 防御(2)；1=特殊 → 特防(3)。与伤害公式的取索引一致（type + 2）。
    if (skill_type != 0 && skill_type != 1) {
        return false;
    }
    // ⚠️ 故意**不查任何免疫**：暴击破防是暴击自带规则，不是"消除强化效果"，
    //    免消除强化(STAT_CLEAR) 与它无关（用户 2026-09-13 口径）。
    const int stat = skill_type + 2;
    ElfPet& pet = ctx->getPet(defender);
    if (pet.levels[stat] <= 0) {
        return false;   // 没有正等级可破（负等级/零不动）
    }
    pet.levels[stat] = 0;
    ctx->ws.view_levels[defender][stat] = 0;   // 本体/视图同步（同其它能力等级通道）
    return true;
}

// 转换/吸取能力提升：把 from 的**正等级**整体搬到 to 身上（from 清零，to 等量累加）。
// 官方 effect 85"使对手的能力提升效果转化到自己身上"；effect 1287"吸取对手能力提升"同一动作，
// 区别只在吸取成功后额外给的东西（1287 另有"下N次受击减伤"）→ 共用本原语。
// 返回搬走的属性项数（0 = 无可转化 或 被免消除强化挡下，两者调用方按同一分支处理）。
// ⚠️ 免消除强化查**from**（要失去提升的那一方）；被挡时 to 也拿不到——"无法被消除或吸取"。
int transfer_stat_boosts(BattleContext* ctx, int from, int to) {
    if (!ctx || from < 0 || from > 1 || to < 0 || to > 1 || from == to) {
        return 0;
    }
    if (ctx->is_immune(from, ImmunityType::STAT_CLEAR, ctx->currentState)) {
        return 0;
    }
    ElfPet& src = ctx->getPet(from);
    ElfPet& dst = ctx->getPet(to);
    int moved = 0;
    for (int i = 0; i < static_cast<int>(src.levels.size()); ++i) {
        const int lv = src.levels[i];
        if (lv <= 0) {
            continue;  // 只转化提升，不动弱化
        }
        src.levels[i] = 0;
        ctx->ws.view_levels[from][i] = 0;
        int gained = dst.levels[i] + lv;
        if (gained > 6) {
            gained = 6;   // 能力等级上限 +6（与 stat_change 一致）
        }
        dst.levels[i] = gained;
        ctx->ws.view_levels[to][i] = gained;
        ++moved;
    }
    return moved;
}

// 弱化原语：把目标的能力等级往下压。规则见头文件（先查免弱、可穿强化保护、钳 -6）。
StatDropResult stat_drop(BattleContext* ctx, int target, int stat, int amount) {
    if (!ctx || target < 0 || target > 1 || stat < 0 || stat >= 6 || amount <= 0) {
        return StatDropResult::INVALID_PARAM;
    }
    // ① 免弱：**无条件、最先查**。目标身上还有强化也照样失败——不许拿"有 +N 可抵消"当理由降。
    if (ctx->is_immune(target, ImmunityType::STAT_DROP, ctx->currentState)) {
        return StatDropResult::IMMUNE;
    }
    // ⚠️ 此处**不查** STAT_CLEAR（免消除强化）：那个护的是"已有的提升被消除/吸取"，
    //    弱化是往下压，原理不同，可以穿过强化保护（用户 2026-09-13 定）。
    ElfPet& pet = ctx->getPet(target);
    int& level = pet.levels[stat];
    if (level <= -6) {
        return StatDropResult::AT_FLOOR;  // 已到底，不越界
    }
    const int new_level = std::max(level - amount, -6);  // ② 不突破 -6（钳制）
    level = new_level;
    ctx->ws.view_levels[target][stat] = new_level;  // 视图同步（伤害/先手权读它）
    return StatDropResult::SUCCESS;
}

// 反转目标的能力下降：负等级 → 正等级（下降翻成提升），不动已有提升。
// 与 clear_stat_boosts（消除提升）独立——"反转"不等于"消除"。
StatReversalResult stat_reversal(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return StatReversalResult::NOTHING;
    }
    // TODO（禁止反转，用户约定）：若 target 身上存在"禁止反转下降"的回合类规则
    //   （RuleCenter 回合类查询效果命中），应返回 BLOCKED 使反转失败。当前未接入，留作未来查询。
    ElfPet& pet = ctx->getPet(target);
    bool any_reversed = false;
    for (int i = 0; i < 6; ++i) {
        if (pet.levels[i] < 0) {    // 只反下降（负等级）
            pet.levels[i] = -pet.levels[i];   // 下降 → 提升
            ctx->ws.view_levels[target][i] = pet.levels[i];  // 同步 ws 视图（见 stat_change 注释）
            any_reversed = true;
        }
    }
    return any_reversed ? StatReversalResult::REVERSED : StatReversalResult::NOTHING;
}

// 反转目标的**能力提升**（正 → 等负）：弱化类动作，先查免弱（见头文件）。
StatReversalResult stat_boost_reversal(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return StatReversalResult::NOTHING;
    }
    // ① 免弱：**最先查、无条件**（与 stat_drop 同一条规则）——目标有免弱则整次失败。
    if (ctx->is_immune(target, ImmunityType::STAT_DROP, ctx->currentState)) {
        return StatReversalResult::BLOCKED;
    }
    // ⚠️ 不查 STAT_CLEAR（免消除强化）：反转是"把提升压成下降"，与弱化同理可穿强化保护。
    ElfPet& pet = ctx->getPet(target);
    bool any_reversed = false;
    for (int i = 0; i < 6; ++i) {
        if (pet.levels[i] > 0) {   // 只反提升（正等级）
            pet.levels[i] = -pet.levels[i];   // 提升 → 下降（等量）
            ctx->ws.view_levels[target][i] = pet.levels[i];
            any_reversed = true;
        }
    }
    return any_reversed ? StatReversalResult::REVERSED : StatReversalResult::NOTHING;
}

namespace {

// 必先授予效果：在先手权时点把 ws.guaranteed_first[owner] 置为 tier。
EffectResult effect_set_guaranteed_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.guaranteed_first[owner] = args.int_args[2];  // tier
    return EffectResult::kOk;
}

}  // namespace

void grant_guaranteed_first(BattleContext* ctx, int owner, int tier) {
    if (!ctx || owner < 0 || owner > 1 || tier <= 0) {
        return;
    }
    // 注册 once 回合类效果到 BATTLE_FIRST_MOVE_RIGHT：下一次先手权时点触发一次 → "下一回合必先"。
    // duration=2 保证活到下一回合(N+1)。先手权处每回合 memset → 只在下一回合生效；断回合可移除它。
    Effect e;
    e.id = 999201;  // 必先授予
    e.logic = &effect_set_guaranteed_first;
    e.args = EffectArgs(std::vector<int>{owner, owner ^ 1, tier});
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, owner,
                                                 /*duration_rounds=*/2, ctx->roundCount);
    ce->once_ = true;
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
}

FixedDamageResult fixed_damage(BattleContext* ctx, int target, int amount) {
    if (!ctx || target < 0 || target > 1 || amount < 0) {
        return FixedDamageResult::INVALID_PARAM;
    }
    ElfPet& pet = ctx->getPet(target);
    if (pet.hp <= 0) {
        return FixedDamageResult::TARGET_DEFEATED;
    }
    deal_damage(ctx, target, amount, DamageKind::FIXED, -1);
    return FixedDamageResult::SUCCESS;
}

PpReduceResult pp_reduce(BattleContext* ctx, int target, int amount) {
    if (!ctx || target < 0 || target > 1 || amount <= 0) {
        return PpReduceResult::INVALID_PARAM;
    }
    ElfPet& pet = ctx->getPet(target);
    bool changed = false;
    for (Skills& skill : pet.skills) {
        if (skill.pp == -1) {
            continue;  // 无限 PP 不参与
        }
        const int before = skill.pp;
        skill.pp = std::max(0, skill.pp - amount);
        if (skill.pp != before) {
            changed = true;
        }
    }
    return changed ? PpReduceResult::SUCCESS : PpReduceResult::INVALID_PARAM;
}

RemoveRoundEffectsResult remove_round_effects(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return RemoveRoundEffectsResult::INVALID_PARAM;
    }
    switch (break_round_effects(ctx, target)) {
        case BreakResult::SUCCESS:    return RemoveRoundEffectsResult::SUCCESS;
        case BreakResult::NO_EFFECTS: return RemoveRoundEffectsResult::NONE;
        case BreakResult::IMMUNE:     return RemoveRoundEffectsResult::IMMUNE;
    }
    return RemoveRoundEffectsResult::INVALID_PARAM;
}

DrainHpResult drain_hp(BattleContext* ctx, int actor, int target, int fraction_denom) {
    if (!ctx || actor < 0 || actor > 1 || target < 0 || target > 1
        || fraction_denom <= 0 || actor == target) {
        return DrainHpResult::INVALID_PARAM;
    }
    ElfPet& defender = ctx->getPet(target);
    if (defender.hp <= 0) {
        return DrainHpResult::TARGET_DEFEATED;
    }
    const int max_hp = std::max(1, defender.numericalProperties[NumericalPropertyIndex::HP]);
    const int amount = std::max(1, max_hp / fraction_denom);
    deal_damage(ctx, target, amount, DamageKind::FIXED, actor);
    // 自身恢复等量——走 heal_impl（封回血/恢复效果修正生效；被封则吸不到血）
    heal_impl(ctx, actor, amount);
    return DrainHpResult::SUCCESS;
}

// drain_hp_amount — 吸取固定伤害（固定量版；恢复同样走 heal_impl）。
DrainHpResult drain_hp_amount(BattleContext* ctx, int actor, int target, int amount) {
    if (!ctx || actor < 0 || actor > 1 || target < 0 || target > 1
        || amount <= 0 || actor == target) {
        return DrainHpResult::INVALID_PARAM;
    }
    ElfPet& defender = ctx->getPet(target);
    if (defender.hp <= 0) {
        return DrainHpResult::TARGET_DEFEATED;
    }
    deal_damage(ctx, target, amount, DamageKind::FIXED, actor);
    heal_impl(ctx, actor, amount);
    return DrainHpResult::SUCCESS;
}

KillResult kill(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return KillResult::INVALID_PARAM;
    }
    ElfPet& pet = ctx->getPet(target);
    if (pet.hp <= 0) {
        return KillResult::ALREADY_DEFEATED;
    }
    pet.hp = 0;
    return KillResult::SUCCESS;
}
