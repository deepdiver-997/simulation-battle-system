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
#include <effects/damage_pipeline.h>          // 属性伤害只跑管线子集（锁伤/挡伤阶段）
#include <numerical-calculation/calculation.h>   // 属性伤害的克制倍数（calculateRestraintMultiples）

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {

// 成功（异常状态实际改变）时向事件中心投递事件：
// - EVENT_ANOMALY_APPLIED：任意异常施加成功（第三方"当对方被挂异常时XXX"监听）
// - EVENT_CONTROLLED：控场类异常施加成功（第三方"当对方被控场时XXX"监听）
// amount = 落地的异常状态 id（2026-09-20 烧伤三精灵线补载荷：监听方要区分
// "这次落地的是不是烧伤"——如 上古炎兽 2481 的"下1次进入的异常转化为烧伤"撤除判定；
// 旧监听不读 amount，补载荷无行为影响）。
// emit 只入队，不内联执行；FSM 在 State 桶后 drain 投递。
void emit_anomaly_events(BattleContext* ctx, int target, int anomaly_id, int actor) {
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_ANOMALY_APPLIED, actor, target,
                                        anomaly_id});
    if (is_control_abnormal_status(static_cast<AbnormalStatusId>(anomaly_id))) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_CONTROLLED, actor, target});
    }
}

// 预死亡信号（2026-09-30 死亡模型，用户定稿）：伤害管线落血后**体力≤0**即广播。
// 消费方：反应型免死（②范围即时检测族——回调里写血，随后的死亡漏斗见 hp>0 走 NOT_DOWN
// 即自然否决）、内核"同归标志"（此处顺手计数）。
// ⚠️ 只在伤害通道发：体力修正通道（自爆 HP_CONSUME/斩杀印记——免死不可见的前提）与
//    秒杀通道（EVENT_HP_TO_ZERO 自己就是信号面）不发本信号。
// 同归标志 = 本回合第一个把对手"在场"体力打到 ≤0 的一方，先到先得（后续伤害不覆盖、
// ws 每回合 reset 清）；瞬杀/体力修正把人打到 0 不计数（口径待实测）。
void emit_hp_exhausted(BattleContext* ctx, int actor, int target, DamageKind kind) {
    if (!ctx || target < 0 || target > 1) {
        return;
    }
    if (ctx->seerRobot[target].elfPets[ctx->on_stage[target]].hp > 0) {
        return;   // 没死：非预死亡（护盾/护罩吃掉、掉血未致死）
    }
    BattleEvent ev{EventType::EVENT_HP_EXHAUSTED, actor, target,
                   0, static_cast<int>(ctx->currentState)};
    ev.damage_kind = static_cast<int>(kind);
    ctx->event_center_.emit(ev);
    // **emit 后立即 drain**（2026-09-30 哈莫链复刻定稿）：免死信号是"伪死亡通知"——
    // 回调（②族拉血）必须在任何死亡裁决读到体力**之前**完成。事件中心默认
    // "入队+晚 drain"会让回调落到死亡漏斗之后（场景 876 实测：skip 日志先于回调）。
    // 与 EVENT_BEFORE_SKILL_HIT 锚点同一先例；drain 会连带投递队列中既有事件
    // （与锚点先例同性质，accept）。
    ctx->event_center_.drain(ctx, ctx->roundCount);
}

// 同归标志落点（与信号分离）：**必须在 EVENT_TAKE_DAMAGE emit 之前**写——
// 受击 watcher（弹伤族）是同步执行的，会在本段伤害的信号发出去之前就把对手打到 ≤0，
// 抢先污染"谁先造成致命伤"。落血记榜当场写，先到先得。
void mark_first_exhaust_flag(BattleContext* ctx, int actor, int target) {
    if (!ctx || ctx->ws.first_to_exhaust_foe >= 0 || actor < 0 || actor > 1
        || actor == target || target < 0 || target > 1) {
        return;
    }
    ctx->ws.first_to_exhaust_foe = actor;
}

// 能力等级写入的**唯一落点**（2026-09-18，混沌魔君索伦森 1011 落地时收口）。
//
// 三件事绑在一起，缺一不可：
//   ① 写本体 `ctx->ability_levels`（权威状态，随上下场清）；
//   ② 写视图 `ctx->ws.view_levels`——**伤害公式/先手权读的是视图**，不同步就是
//      "等级改了却不生效"（本体/视图分裂曾算出 INT_MAX，见 032 踩坑记录）；
//   ③ emit `EVENT_STAT_CHANGED`——"监测能力等级变化"一族（索伦森 1011 的压制窗口、
//      后续"对手强化时…"类效果）靠它做即时反应；绕过本函数直写本体就会**静默**漏掉监测。
//
// ⚠️ 免疫/规则判定留在各原语入口（顺序有机制含义），本函数只做"落地"这一件事；
//    调用方保证 new_level 已在 [-6, 6] 内。
//
// ★ 这里是"**等级变更拦截**"的唯一正确落点（用户 2026-09-18 提出的设计，尚未实现）：
//   "对手的能力等级不得超过自身"一类**上限型**魂印（混沌魔君索伦森 1011 子句②）目前走的是
//   "事后压回"——挂在 EVENT_STAT_CHANGED 上，等变更落地后再改回去；代价是那个瞬态在一个
//   drain 波次内真实存在，且监听器要自己处理 (owner0→owner1) 的桶定序。
//   入口统一之后就不必这样了：在**本函数**里查一条 RuleCenter 的拦截条目，
//   命中即**钳到上限**（或拒绝），于是越界的那次变更从未发生 —— 不依赖任何事件或时点定序。
//   注意拦截是双向的：① 对手想超过我 → 钳；② 我自己的等级掉下去 → 我这一趟也要顺带复查
//   对手（它现在就超上限了），这正是本函数"所有变更都经此"的价值所在。
//   落地时需要给 RuleCenter 加一个条目族（不是 IMMUNE——没有次数/时点覆盖语义，
//   生命周期随宿主在场），见 soul_lib/chaos_sorensen.cpp 文件头的同段说明。
void write_ability_level(BattleContext* ctx, int target, int stat, int new_level) {
    const int delta = new_level - ctx->ability_levels[target][stat];
    if (delta == 0) {
        // 值没变（如 stat_change(delta=0)）→ 不写、不 emit。
        // ⚠️ 必须早退：否则挂在 EVENT_STAT_CHANGED 上的监听器会被自己的空操作再唤醒一次，
        //    形成无意义的事件波（drain 有 8 波上限，超了会响亮报错并丢弃事件）。
        return;
    }
    ctx->ability_levels[target][stat] = new_level;
    ctx->ws.view_levels[target][stat] = new_level;
    BattleEvent ev{EventType::EVENT_STAT_CHANGED, /*actor=*/-1, target, 0};
    ev.state = static_cast<int>(ctx->currentState);
    ev.stat_index = stat;
    ev.level_after = new_level;
    ev.level_delta = delta;
    ctx->event_center_.emit(ev);
}

} // namespace

// ----------------------------------------------------------------
// apply_anomaly — 施加异常状态的统一入口（双通道，2026-09-16）
//
// Modern 通道检查链（按顺序执行）：
//   [Step 1] 参数校验
//   [Step 2] 目标存活检查
//   [Step 3] 免疫检查 — 免疫内核 is_immune(ANOMALY) + Mark ID 0 兜底
//   [Step 4] 同种异常 — 比较剩余回合，用长的覆盖短的
//   [Step 5] 控场替换 — 已有控场则清除旧控场，施加新控场
//   [Step 6] 特殊阻止检查（预留：抗性/装备/场地/保护机制）
//   [Step 7] 执行施加 — 写入 abnormal_status_end_round，成功路径 emit 事件
// （实际实现里 [3] 免疫 → 弹控反弹 → [4] 抗性 → [5] 魂免 → [6] 转化 → 落地。）
//
// Ancient 通道（apply_anomaly_ancient，主动毒）：只查 ImmunityTier::Ancient 免疫
//（次免/魂免两段 + Mark 0），跳过 抗性/转化、不触发弹控，其余同。
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
// channel = 施加通道（Modern=全检查链；Ancient=主动毒：只查古代层免疫、跳抗性/转化、不反弹）。
// chance_pct = 施加方**申报的概率**（<0 = 未申报，闸门不介入；见 battle_primitives.h 注）。
static ApplyAnomalyResult apply_anomaly_impl(BattleContext* ctx,
                                             int target,
                                             int anomaly_id,
                                             int duration_rounds,
                                             int actor,
                                             int reflect_depth,
                                             AnomalyChannel channel,
                                             int chance_pct,
                                             ChanceSource source) {
    // 古早施加对现代免疫/弹控不可见 → 查询时按层级过滤（-1 = 不限层级）。
    // Raw（遗留裸施加）不做任何检测：免疫/抗性/转化/弹控全部跳过。
    const int tier_filter =
        (channel == AnomalyChannel::Ancient)
            ? static_cast<int>(ImmunityTier::Ancient) : -1;
    const bool check_defense = (channel != AnomalyChannel::Raw);
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
    // [1.1] 异常持续时间上限（套装线 2026-09-19 圣芒佑界 476："自身进入的异常状态
    // 最高为2回合"）。per-side 上限，0 = 无上限；在随机时长落定之后、进入任何
    // 判定分支之前钳制——[4] 抗性标记（固定 2 回合）、[7] 覆盖、[8] 落地统一生效。
    // 上限由效果层设置（shengmang_476.cpp 激活时装一次），clearAllEffects 复位。
    if (ctx->anomaly_duration_cap[target] > 0) {
        duration_rounds = std::min(duration_rounds, ctx->anomaly_duration_cap[target]);
    }

    ElfPet& pet = ctx->getPet(target);

    // [2] 目标存活检查
    if (pet.hp <= 0) {
        return ApplyAnomalyResult::TARGET_DEFEATED;
    }

    // [2.0] **附加禁令**（ATTACH_BAN，2026-09-20 神觉·米斯蒂克 4676 引入，用户拍板的
    // "查询式保护"落点）：actor 一侧的攻击技能无法附加异常状态——原语入口查询，**不是**
    // 给被保护方上一段免疫（那会把回合开始前/结束后的异常也挡掉）。
    //   · 只拦 **Modern 通道**（用户拍板"不拦"古早主动毒；Raw 被动毒同样不查）；
    //   · 只拦 **reflect_depth==0 的原生施加**——弹控**弹回来**的异常（depth==1）不拦
    //     （机制解析口径："对手无法附加异常"不能免疫被弹回来的异常，枫眠弹控反弹睡眠）；
    //   · 窗口在票上（coverage=行动段时点位 + register_round/remaining_rounds），见
    //     RuleCenter::has_attach_ban——回合开始前/结束后的施加天然不命中 coverage。
    // ⚠️ 排在 [2.1] 砥砺置位之前：被禁令挡在门外的施加**没执行过**，不算砥砺条件位。
    if (channel == AnomalyChannel::Modern && reflect_depth == 0 && actor >= 0 && actor <= 1
        && ctx->rule_center_.has_attach_ban(actor, RuleCenter::kAttachBanAnomaly,
                                            state_coverage_bit(ctx->currentState),
                                            ctx->roundCount)) {
        return ApplyAnomalyResult::BLOCKED_BY_EFFECT;
    }

    // [2.1] 砥砺(37) 的条件位：**对方**（Modern 通道的效果）对本方**执行过**"附加异常"这个动作。
    // 口径（语料 idx=90/157）：「执行异常效果的来源**必须是对方**，如果通过第三方效果是不算进去的，
    // 比如圣武砥砺、涂碟黑幕，**另外主动毒和被动毒也是不算进去**」→ 只看 Modern 通道 + actor 是对方。
    // ⚠️ 置位在**入口**（"执行过"不要求真落地：免疫/抗性/被弹控也算执行过）。
    // ⚠️ **砥砺(37) 自己不算**：语料 idx=90 同句点名「如果通过**第三方效果**是不算进去的，
    //    **比如圣武砥砺**、涂碟黑幕」——砺砺本身（由其魂印在登场时对双方施加）不计，
    //    否则"登场即砥砺"的场次里条件位会被自己的施加占满，砥砺等于永久失效。
    // ⚠️ 同句还列了「涂碟黑幕」——那是**名词级附属类**、本引擎未建模，故无从排除；
    //    若实测表明**所有附属类**都不算，把这里扩成
    //    `abnormal_status_kind(anomaly_id) == AbnormalStatusKind::Special` 的排除即可（一行）。
    //    当前证据只点名了砥砺，故只排除它。
    if (channel == AnomalyChannel::Modern && actor >= 0 && actor <= 1 && actor != target
        && anomaly_id != static_cast<int>(AbnormalStatusId::Resolution)) {
        ctx->ws.anomaly_applied_by_opponent[target] = true;
    }

    // [2.2] **概率裁定与掷骰**（2026-09-22，概率闸门管道；亮节 = 无极圣武 2436 / 帝皇侠）。
    // 只有**施加方申报了概率**（chance_pct >= 0）才介入——未申报 = 调用方自己已经掷过骰
    // 或本来就是"必定"，闸门一概不问（这是"未迁移调用点行为不变"的结构性保证）。
    //   ① 先问闸门（**被施加方** target 持有的概率闸门票）改写本效果的实际概率。
    //      官方口径「**以实际概率为准**」（战栗被提升到 100% 就不受影响）→ 判的是本次
    //      实际值；且必须**在掷骰之前**，这正是闸门做成查询式、事件中心承载不了的原因。
    //   ② 再按改写后的值掷一次：0 = 必定不触发（不消耗 rand）、>=100 = 必定触发
    //      （同样不消耗 rand，与 roll_percent / trait_proc_roll 的契约一致）、其余 rand()%100。
    // ⚠️ 位置：排在 ATTACH_BAN 与砥砺置位**之后**——"被禁令挡在门外"不算执行过（既有口径），
    //    而"概率没掷中"算执行过（动作已经做了，与免疫/抗性同类；官方"不要求真落地"）。
    // ⚠️ 通道无关：三个通道（Modern/Ancient/Raw）都过这一步。亮节原文点名覆盖"主动毒、
    //    被动毒"，而它们正是 Ancient/Raw 通道——闸门在这里是**比免疫链更外层**的东西
    //    （问的是"这次附加到底有没有发生"，不是"发生了会不会被挡"）。
    if (chance_pct >= 0) {
        const int final_pct = ctx->rule_center_.rewrite_anomaly_chance(
            target, actor, chance_pct, ChanceTag::AnomalyAttach, source, ctx->roundCount);
        if (final_pct <= 0) {
            return ApplyAnomalyResult::ROLL_FAILED;
        }
        if (final_pct < 100 && static_cast<int>(std::rand() % 100) >= final_pct) {
            return ApplyAnomalyResult::ROLL_FAILED;
        }
    }

    // [2.5] **凝滞（32）免疫控制类异常**（官方 effect_des 32：「凝滞：弱化类异常状态，限制类异常状态，
    // 该状态下精灵无法切换，**同时免疫受到的控制类异常状态**」）。
    // 口径（2026-09-18 用户拍板 + 官方文本）：
    //   - 只免疫**控制类**（is_control_abnormal_status）→ 烧伤/中毒等**弱化类照常受**；
    //   - 只对 **Modern 通道**生效 —— `Ancient`(主动毒)/`Raw`(被动毒) **照穿**
    //     （用户口径"主动/被动毒无法免疫"；这与两代施加通道的既有语义一致：
    //      主动毒无视一切现代免疫、被动毒什么都不查）；
    //   - **不消耗次免、不反弹** —— 这是凝滞自身附带的免疫，不是目标挂着的免疫票
    //     （官方文本没有"反弹"二字）；排在 [3] 免疫链之前，故次免不会被"白扣"。
    //   - 排在转化([6]) 之前：先按**原始入参**判类别。若把转化后的形态拿来判，
    //     "凝滞 + 目标预置转化表"会变成绕过凝滞的后门，与"免疫"的口径相反。
    if (check_defense && channel == AnomalyChannel::Modern
        && ctx->has_active_abnormal_status(target, static_cast<int>(AbnormalStatusId::Stasis))
        && is_control_abnormal_status(static_cast<AbnormalStatusId>(anomaly_id))) {
        return ApplyAnomalyResult::TARGET_IMMUNE;
    }

    // 弹控：目标免疫时把异常反弹给施放方。最多反弹 1 次（depth==1 不再弹）——防双方弹控打乒乓球。
    // ⚠️ 古早施加（主动毒）不触发弹控（"所有的技能弹控不能免疫主动毒，包含魂印弹控也不能"）
    //    → Ancient 通道免疫命中直接 TARGET_IMMUNE。
    const auto reflect = [&]() -> ApplyAnomalyResult {
        if (channel == AnomalyChannel::Modern && reflect_depth == 0
            && ctx->rule_center_.has_reflect(target)
            && actor >= 0 && actor != target) {
            apply_anomaly_impl(ctx, actor, anomaly_id, duration_rounds,
                               /*actor=*/target, /*reflect_depth=*/1,
                               /*channel=*/AnomalyChannel::Modern,
                               /*chance_pct=*/-1,   // 概率已在原生那趟判定过，反弹不再掷
                               source);
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
    if (check_defense
        && ctx->is_immune_effect(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id,
                                 tier_filter)) {
        // 被弹回来的异常（reflect_depth>0）**不消耗**——"被弹回来的异常不属于精灵受到异常"
        // （idx=375）；但它仍可**被**次免免疫（照常走本分支挡下，只是不扣次数）。
        if (reflect_depth == 0) {
            ctx->consume_immune(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id,
                                /*soul_filter=*/0, tier_filter);
        }
        // 免疫成功事件（2026-09-20 烧伤三精灵线）：「每次免疫成功…」族（秘纹护体 2059）
        // 的检测点。弹控反弹也算免疫成功 → 照发。
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_ANOMALY_IMMUNED, actor, target,
                                            anomaly_id, static_cast<int>(ctx->currentState)});
        return reflect();
    }

    // [4] 异常抗性 roll（弹回的异常不过抗性——depth>0 跳过；官方必修3/选修7）。
    // ⚠️ 古早施加（主动毒）也**无视异常抗性**（语料主动毒口径）→ Ancient 通道跳过。
    // 抗性成功：直写附加"免疫异常"异常(21, 2回合)——**击穿魂免**（不走免疫检查）；
    // 先走转化（攻击方可预置 conversion[target][21]=Y 把抵抗结果直接转成 Y）。
    if (channel == AnomalyChannel::Modern && reflect_depth == 0
        && pet.resistance.isResistantTo(anomaly_id)) {
        const int resolved = resolve_anomaly_conversion(
            ctx, target, static_cast<int>(AbnormalStatusId::AbnormalImmunity));
        ctx->set_abnormal_status_end_round(target, resolved, ctx->roundCount + 2);
        emit_anomaly_events(ctx, target, resolved, actor);
        // 抗性触发专属事件（套装线 2026-09-19 圣芒佑界 476"异常抗性触发时回血"用）：
        // 语义唯一——21 的 APPLIED 事件混有其他来源，判"抗性触发"只认本事件。
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_ANOMALY_RESISTED, actor, target,
                                            anomaly_id, static_cast<int>(ctx->currentState)});
        return ApplyAnomalyResult::RESISTED_BY_RESISTANCE;
    }

    // [5] 魂免（soul=true）—— 抗性判定失败后才查（官方优先级）。同样挡下后消费次数型。
    // Mark 0 = 老魂免兜底，属**古代层**（官方"带补丁"名单的机制化）→ 古早施加照常被它挡。
    if (check_defense
        && (ctx->is_immune_soul(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id,
                                tier_filter)
            || has_mark(pet.marks, 0))) {
        if (reflect_depth == 0) {  // 弹回的异常不消耗（同 [3]）
            ctx->consume_immune(target, ImmunityType::ANOMALY, ctx->currentState, anomaly_id,
                                /*soul_filter=*/1, tier_filter);
        }
        // 免疫成功事件（同 [3]）：魂免命中也是"免疫成功"。
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_ANOMALY_IMMUNED, actor, target,
                                            anomaly_id, static_cast<int>(ctx->currentState)});
        return reflect();
    }

    // [6] 转化（单跳）：入→出，直接施加（绕过抗性/免疫的转换路径）。
    // ⚠️ 古早施加（主动毒）无视转化异常 → Ancient 通道直通；Raw 同样直通。
    const int resolved = (channel == AnomalyChannel::Modern)
                             ? resolve_anomaly_conversion(ctx, target, anomaly_id)
                             : anomaly_id;
    const bool converted = (channel == AnomalyChannel::Modern && resolved != anomaly_id);

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
    // 狂信(41) 的配套写入（官方 effect_des 515）：「**进入狂信状态时**，**若自身没有信仰对象**，
    // 则**本次狂信状态的来源**成为自身信仰对象」。
    // ⚠️ 三条口径要点：
    //   ① **只在真的进入（落地成功）时写** —— 免疫/抗性抵抗/转移异常都不设信仰对象
    //      （语料：「如果对手免疫或者抵抗都是没有信仰对象，同时通过转移异常也是不会信仰对手」），
    //      所以写点必须在成功分支，不能放函数入口。
    //   ② **是条件写**（"若自身没有"）→ 已有信仰对象**不被覆盖**；要改写必须走魂印的"强制改写"
    //      入口（`ctx->set_faith_target`，如教皇魂印"改为其"、索伦森"改为索伦森"）。
    //   ③ "来源" = 施放方**此刻在场的那只**（`on_stage[actor]`）—— `actor` 只有方号，
    //      精灵身份靠 on_stage 补。
    // ⚠️ 弹控（REFLECTED）路径下 `actor` 已被换成原目标（见上面的 reflect lambda），此时"来源"
    //    该算谁**未找到官方口径** → 按"反弹后的施放方"写，待实测。
    if (resolved == static_cast<int>(AbnormalStatusId::Fanaticism)
        && actor >= 0 && actor <= 1 && actor != target) {
        if (ctx->faith_target(target).side < 0) {
            ctx->set_faith_target(target, actor, ctx->on_stage[actor]);
        }
    }
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

// 公开入口：现代施加（reflect_depth=0）。
ApplyAnomalyResult apply_anomaly(BattleContext* ctx,
                                 int target,
                                 int anomaly_id,
                                 int duration_rounds,
                                 int actor,
                                 int chance_pct,
                                 ChanceSource source) {
    return apply_anomaly_impl(ctx, target, anomaly_id, duration_rounds, actor,
                              /*reflect_depth=*/0, /*channel=*/AnomalyChannel::Modern,
                              chance_pct, source);
}

// 公开入口：古早施加（主动毒；reflect_depth=0，Ancient 通道不反弹、depth 恒 0）。
ApplyAnomalyResult apply_anomaly_ancient(BattleContext* ctx,
                                         int target,
                                         int anomaly_id,
                                         int duration_rounds,
                                         int actor,
                                         int chance_pct,
                                         ChanceSource source) {
    return apply_anomaly_impl(ctx, target, anomaly_id, duration_rounds, actor,
                              /*reflect_depth=*/0, /*channel=*/AnomalyChannel::Ancient,
                              chance_pct, source);
}

// 公开入口：遗留裸施加（特性被动毒；不做任何检测，reflect_depth 恒 0）。
ApplyAnomalyResult apply_anomaly_raw(BattleContext* ctx,
                                     int target,
                                     int anomaly_id,
                                     int duration_rounds,
                                     int actor,
                                     int chance_pct,
                                     ChanceSource source) {
    return apply_anomaly_impl(ctx, target, anomaly_id, duration_rounds, actor,
                              /*reflect_depth=*/0, /*channel=*/AnomalyChannel::Raw,
                              chance_pct, source);
}

// ----------------------------------------------------------------
// 随机异常附加（2026-09-19）：解析池 → 概率门 → 不重复挑选 → apply_anomaly 单通道
// （池 helper anomaly_pool_control/all 是头内联，见 battle_primitives.h）
// ----------------------------------------------------------------

RandomAnomalyResult attach_random_anomalies(BattleContext* ctx,
                                            int target,
                                            const std::vector<int>& base_pool,
                                            int count,
                                            int probability_pct,
                                            int actor,
                                            ChanceSource source) {
    RandomAnomalyResult out;
    if (!ctx || target < 0 || target > 1 || count <= 0 || probability_pct <= 0
        || base_pool.empty()) {
        return out;
    }
    // ① 解析池（⊕池改写票，授予序折叠 + 去重保序——都在 resolve_anomaly_pool 里）。
    const std::vector<int> pool = ctx->rule_center_.resolve_anomaly_pool(ctx, base_pool);
    if (pool.empty()) {
        return out;
    }
    out.requested = std::min(count, static_cast<int>(pool.size()));
    // ② 概率门：整个子句一次（同插件侧 roll_percent 语义：<100 时 rand()%100 判定）。
    // ⚠️ 掷骰**之前**先过闸门（亮节族）——与三个施加原语的 chance_pct 是同一道门。
    //    这是"整子句一次"的那次判定，故闸门在这里问；③ 里的逐只 apply_anomaly 传 -1
    //    （已判定过，不能再掷一次，否则一次子句会被掷 N 次）。
    const int gated_pct = ctx->rule_center_.rewrite_anomaly_chance(
        target, actor, probability_pct, ChanceTag::AnomalyAttach, source, ctx->roundCount);
    if (gated_pct < 100
        && (gated_pct <= 0 || static_cast<int>(std::rand() % 100) >= gated_pct)) {
        return out;
    }
    out.rolled = 1;
    // ③ 不重复挑选（部分 Fisher-Yates：rand 序随引擎全局 std::rand，测试可 srand 固定）。
    std::vector<int> bag = pool;
    for (int pick = 0; pick < out.requested; ++pick) {
        const int idx = pick + static_cast<int>(std::rand() % static_cast<unsigned>(bag.size() - pick));
        std::swap(bag[pick], bag[idx]);
        const ApplyAnomalyResult r = apply_anomaly(ctx, target, bag[pick],
                                                   /*duration_rounds=*/-1, actor);
        if (r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
            || r == ApplyAnomalyResult::DURATION_EXTENDED
            || r == ApplyAnomalyResult::CONVERTED) {
            ++out.landed;
        }
    }
    return out;
}

// ----------------------------------------------------------------
// 异常自然结算点（2026-09-18）
// ----------------------------------------------------------------

namespace {

// 目标最大体力（本体优先、逐级回退）。与 battleFsm 的 resolve_pet_max_hp 同口径。
int anomaly_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// 按档案把一条异常的伤害落成一次 deal_damage。
//   TruePercent → TRUE        （护盾/护罩/抗性都不响应，官方 idx=266）
//   Percent     → PERCENT_VALUE（值已算好，但走百分比抗性/护罩/免粉）
//   Fixed       → FIXED       （固定档，吃护罩/固定抗性）
// （用 PERCENT_VALUE 而不是 PERCENT：PERCENT 的 amount 是整数百分比，
//   1/8 会被 12% 截断成 max_hp*12/100，与 max_hp/8 差一截。）
void deal_anomaly_damage(BattleContext* ctx, int target, const ElfPet& pet,
                         const AbnormalDamageProfile& profile, int actor) {
    int amount = profile.numerator;
    if (profile.denominator > 0) {
        const int max_hp = anomaly_max_hp(pet);
        if (max_hp <= 0) {
            return;
        }
        amount = std::max(1, max_hp * profile.numerator / profile.denominator);
    }
    if (amount <= 0) {
        return;
    }
    DamageKind kind = DamageKind::FIXED;
    switch (profile.tier) {
        case AbnormalDamageTier::TruePercent: kind = DamageKind::TRUE; break;
        case AbnormalDamageTier::Percent:     kind = DamageKind::PERCENT_VALUE; break;
        case AbnormalDamageTier::Fixed:       kind = DamageKind::FIXED; break;
        case AbnormalDamageTier::None:        return;
    }
    const int hp_before = pet.hp;
    deal_damage(ctx, target, amount, kind, actor);
    // 寄生（官方 3/4）："同时对手会恢复等量的体力"——按**实际扣掉的量**回给对面。
    if (profile.heal_opponent_equal && actor >= 0 && actor != target) {
        const ElfPet& pet_after = ctx->getPet(target);
        const int actual = hp_before - pet_after.hp;
        if (actual > 0) {
            heal_amount(ctx, actor, actual);
        }
    }
}

// 衍化：把"自然结束的源异常"变成它的转出形态（官方"a 异常结束后会转化为 b"）。
// ⚠️ 直接写 end_round，**不走 apply_anomaly**（那是"新施加"，会重跑免疫/抗性/转化/弹控；
//    官方的衍化是同一异常改变形态 —— 见头文件 tick_abnormal_statuses 注释）。
// 附带弱化走 stat_drop_piercing（**无视免弱**，语料 idx=37/466）。
void derive_anomaly(BattleContext* ctx, int target, const ElfPet& /*pet*/,
                    int source_id, int actor) {
    const AbnormalDerivation d = abnormal_derivation(static_cast<AbnormalStatusId>(source_id));
    if (d.group == AbnormalDerivationGroup::None) {
        return;
    }

    int derived_id = d.derived_id;
    if (d.group == AbnormalDerivationGroup::CurseRoll) {
        // 官方 23：诅咒结束后随机转化为 烈焰诅咒 24 / 致命诅咒 25 / 虚弱诅咒 26 之一。
        static constexpr int kCurseRoll[] = {
            static_cast<int>(AbnormalStatusId::FlameCurse),
            static_cast<int>(AbnormalStatusId::DeathCurse),
            static_cast<int>(AbnormalStatusId::WeaknessCurse),
        };
        derived_id = kCurseRoll[std::rand() % 3];
    }
    if (!is_valid_abnormal_status_id(derived_id)) {
        return;
    }

    int rounds = random_anomaly_duration();
    if (d.derived_rounds_min > 0) {
        const int hi = std::max(d.derived_rounds_min, d.derived_rounds_max);
        rounds = d.derived_rounds_min + (std::rand() % (hi - d.derived_rounds_min + 1));
    }
    // ⚠️ **+1 是必须的**：衍化发生在**回合扣减点**，而该回合的行动阶段（ACTION_START 等）
    //    早已跑完 —— 转出异常的"第一个生效回合"是**下一回合**。而 `has_active` 是
    //    `roundCount < end`，若照 `roundCount + rounds` 写，转出异常会在第 `rounds` 个回合
    //    就失效 → 实际只生效 `rounds - 1` 个回合（少一回合）。
    //    加 1 后：生效回合 = 下一回合起共 `rounds` 个，与"新施加一个 `rounds` 回合的异常"等价
    //    （`apply_anomaly` 在**选择期**施加，那时本回合还没打，故它用 `roundCount + duration`）。
    //    ⚠️ 场景 081 的 N 组就是钉这个的（按官方文本核"转出异常剩余回合数"）。
    const int new_end = ctx->roundCount + rounds + 1;
    // 同种转出已存在 → 只把回合数取长（与 apply_anomaly 的"同种延长"同口径）；
    // 否则直接落地。两种情况都 emit（转出异常确实改变了状态）。
    const int existing_end = ctx->get_abnormal_status_end_round(target, derived_id);
    if (!ctx->has_active_abnormal_status(target, derived_id) || existing_end < new_end) {
        ctx->set_abnormal_status_end_round(target, derived_id, new_end);
    }
    emit_anomaly_events(ctx, target, derived_id, actor);

    // 附带弱化（无视免弱）：冰封→速度-1 / 焚烬→命中-1 / 感染→攻击&特攻-1
    for (int i = 0; i < 2; ++i) {
        if (d.stat_indices[i] < 0 || d.stat_deltas[i] == 0) {
            continue;
        }
        stat_drop_piercing(ctx, target, d.stat_indices[i], -d.stat_deltas[i]);
    }

#ifdef BATTLE_FSM_VERBOSE_DEFAULT
    std::cout << "[derive_anomaly] target=" << target
              << " " << abnormal_status_name_cn(source_id) << "(" << source_id << ")"
              << " -> " << abnormal_status_name_cn(derived_id) << "(" << derived_id << ")"
              << " rounds=" << rounds << std::endl;
#endif
}

}  // namespace

int tick_abnormal_statuses(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return 0;
    }
    ElfPet& pet = ctx->getPet(target);
    if (pet.hp <= 0) {
        return 0;   // 已倒地：异常随换宠/清场处理，不在结算点造伤害
    }

    // ① 回合扣减点档伤害（官方 24 烈焰诅咒 / 30 沉默："每回合结束后"）。
    //    只对**仍生效**的异常结算（本回合起已失效的不该再扣）。骰子挂在档案的 chance_pct。
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!ctx->has_active_abnormal_status(target, id)) {
            continue;
        }
        const AbnormalDamageProfile p = abnormal_damage_profile(static_cast<AbnormalStatusId>(id));
        if (p.timing != AbnormalDamageTiming::RoundReduction) {
            continue;
        }
        if (p.chance_pct < 100 && (std::rand() % 100) >= p.chance_pct) {
            continue;
        }
        if (pet.hp <= 0) {
            break;   // 前一条已经打死（串联伤害不越过死亡）
        }
        deal_anomaly_damage(ctx, target, pet, p, /*actor=*/1 - target);
    }

    // ①.5 **异常回合数冻结**（宙变之殢族，神谕古王线）：frozen 时对全部**活跃**异常
    //    end_round +1，补偿本回合 roundCount 的推进 → 剩余回合数（end - roundCount）恒定，
    //    永不进入下面的到期集合 → 无到期伤害/衍化/EVENT_ANOMALY_EXPIRED（星盘族冻住）。
    //    已到期（end <= roundCount）的不补——冻结只保"还在身上的"，不复活已结束的异常。
    if (ctx->anomaly_rounds_frozen[target]) {
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (!ctx->has_active_abnormal_status(target, id)) {
                continue;
            }
            const int end = ctx->get_abnormal_status_end_round(target, id);
            ctx->set_abnormal_status_end_round(target, id, end + 1);
        }
    }

    // ② 到期项：先"…结束时"伤害，再**清槽**，最后衍化。
    //    ⚠️ 次序是刻意的：衍化会往同一个数组写转出异常的 end_round，
    //    若把清槽放在衍化之后，当"转出 id 恰好也在本次到期集合里"（如 感染→中毒 且中毒同时到期）
    //    就会把刚写进去的新回合数一起清掉。**先把清槽做完、再衍化**，"同种转出已存在"的判断
    //    也就自然落在干净的数组上（转出项拿到完整回合数，而不是跟旧值比长短）。
    //    ⚠️ 两趟快照：清槽要精确只清"本次到期的"，不能扫全表清 `end <= roundCount`
    //    （免得误伤本回合别处刚挂上的异常）。
    int expired[kOfficialAbnormalStatusSlotCount] = {0};
    int expired_count = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        const int end = ctx->get_abnormal_status_end_round(target, id);
        if (end > 0 && end <= ctx->roundCount) {
            expired[expired_count++] = id;
        }
    }

    // ②a "…结束时"档伤害（官方 28 束缚）
    for (int i = 0; i < expired_count; ++i) {
        const int id = expired[i];
        const AbnormalDamageProfile p = abnormal_damage_profile(static_cast<AbnormalStatusId>(id));
        if (p.timing != AbnormalDamageTiming::OnExpire || pet.hp <= 0) {
            continue;
        }
        if (p.chance_pct < 100 && (std::rand() % 100) >= p.chance_pct) {
            continue;
        }
        deal_anomaly_damage(ctx, target, pet, p, /*actor=*/1 - target);
    }

    // ②b 过期回写：清槽 —— 修掉"过期不回写 → `end != 0` 判'有异常'是假阳性"的历史问题。
    for (int i = 0; i < expired_count; ++i) {
        ctx->set_abnormal_status_end_round(target, expired[i], 0);
    }

    // ②c 衍化（写转出形态 + 无视免弱的附带弱化）
    int derived = 0;
    for (int i = 0; i < expired_count; ++i) {
        if (abnormal_derivation(static_cast<AbnormalStatusId>(expired[i])).group
            == AbnormalDerivationGroup::None) {
            continue;
        }
        derive_anomaly(ctx, target, pet, expired[i], /*actor=*/1 - target);
        ++derived;
    }

    // ②d **异常自然到期事件**（EVENT_ANOMALY_EXPIRED，2026-09-20）。
    // ⚠️ 只对**没有衍化规则**的到期项发——有衍化规则的走 ②c"转化"，按官方星盘族实测口径
    //   （天启星魂：转化/转移都不算"异常状态结束"）不触发"自然结束"类效果。
    //   事件在清槽**之后**发，监听方看到的是干净状态（该异常已不在身上）。
    //   actor = -1（自然耗尽，无施加方）；amount = 到期的异常 id。
    for (int i = 0; i < expired_count; ++i) {
        const int id = expired[i];
        if (abnormal_derivation(static_cast<AbnormalStatusId>(id)).group
            != AbnormalDerivationGroup::None) {
            continue;
        }
        BattleEvent ev;
        ev.type = EventType::EVENT_ANOMALY_EXPIRED;
        ev.actor = -1;
        ev.target = target;
        ev.amount = id;
        ev.state = static_cast<int>(ctx->currentState);
        ctx->event_center_.emit(ev);
    }

    // ③ **附属类异常的回合结束收益**（官方 33/34/38）。放在伤害与形态变化**之后**：
    //    先把本回合该结算的（回合扣减点档伤害 / "结束时"档伤害 / 衍化）走完，再给收益。
    //    ⚠️ 只处理**仍生效**的异常（`has_active` 严格 `<`）——本回合到期的那些已在 ②b 清 0。
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!ctx->has_active_abnormal_status(target, id)) {
            continue;
        }
        if (id == static_cast<int>(AbnormalStatusId::StarBlessing)) {
            // 星赐(33)「每回合结束时恢复2点PP值」
            for (Skills& sk : pet.skills) {
                if (sk.pp < 0 || sk.maxPP < 0) {
                    continue;   // -1 = 无限 PP，`min(-1, -1+2) = 1` 会把无限变成 1
                }
                sk.pp = std::min(sk.maxPP, sk.pp + 2);
            }
        } else if (id == static_cast<int>(AbnormalStatusId::StarWisdom)) {
            // 星哲(34)「每回合结束时恢复最大体力的1/4」（走 heal_amount → 吃封回血/恢复修正）
            heal_amount(ctx, target, std::max(1, anomaly_max_hp(pet) / 4));
        }
    }

    // 星赎(38)「每回合结束时**所有非限制类异常状态的回合数+1**」（官方 effect_des 38）。
    //
    // 语义（用户 2026-09-18 口径）：这是**增加剩余回合数**的手段 —— 回合数每回合被自然扣减 1，
    // 这里再补 1 → 指定异常的剩余回合数**保持不变**（等效"永久保持"，但**不是**加一个永久字段）。
    // ⚠️ 为什么必须写成 `end_round += 1` 而不是一个"永久"布尔：
    //    (a) 加永久字段会在实现上丢掉"剩余回合数"这个概念，而**有的效果正是按剩余异常回合数**办事的
    //        （如衰弱 11 的"受到的攻击伤害额外提升 25/50/100/250/500%"按剩余回合数分档）；
    //    (b) `end_round += 1` 天然满足"剩余 = end - roundCount 每回合 +1 再 -1 = 不变"，
    //        且所有读剩余回合数的地方（含上面那类）自动看到正确值。
    //
    // ⚠️ 位置必须在 ②b 清槽**之后**：`has_active` 是 `roundCount < end`，本回合该到期的那些
    //    （`end == roundCount`）已经失效并被清 0；若在清槽前 +1 会把它们写成 `end+1`
    //    → 复活成一个 active 异常，且**跳过**衍化与"结束时"伤害（②a 用的是清槽前的快照）。
    // ⚠️ 与 ②c 衍化的先后（**口径待实测**）：当前放在衍化**之后** → 本回合刚衍化出的转出异常
    //    也会 +1（多一回合）。要避免就挪到 ②a 与 ②b 之间。两种都不能放在 ②b 之前。
    if (ctx->has_active_abnormal_status(target,
            static_cast<int>(AbnormalStatusId::StarRedemption))) {
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (is_restriction_abnormal_status(static_cast<AbnormalStatusId>(id))) {
                continue;   // 限制类（瘫痪 19 / 凝滞 32）不在"非限制类"之列
            }
            if (!ctx->has_active_abnormal_status(target, id)) {
                continue;
            }
            const int end = ctx->get_abnormal_status_end_round(target, id);
            ctx->set_abnormal_status_end_round(target, id, end + 1);
        }
    }

    return derived;
}

int reduce_active_anomaly_rounds(BattleContext* ctx, int target, int delta) {
    if (!ctx || target < 0 || target > 1 || delta <= 0) {
        return 0;
    }
    // 冻结（宙变之殢族）：回合数不会减少——加速消耗整条短路（含天启星魂 2207 自身技能）。
    if (ctx->anomaly_rounds_frozen[target]) {
        return 0;
    }
    int reduced = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!ctx->has_active_abnormal_status(target, id)) {
            continue;
        }
        const int end = ctx->get_abnormal_status_end_round(target, id);
        // 只往下压，不钳底：压到 end <= roundCount 的那批由下一轮 tick 自然到期收尾
        // （到期伤害/衍化/到期事件照常走）——这正是"加速消耗"与"直接解除"的分界。
        ctx->set_abnormal_status_end_round(target, id, end - delta);
        ++reduced;
    }
    return reduced;
}

int set_anomaly_rounds(BattleContext* ctx, int target, int status_id, int rounds) {
    if (!ctx || target < 0 || target > 1 || !is_valid_abnormal_status_id(status_id)
        || rounds <= 0) {
        return 0;
    }
    // 直接设定剩余回合数（区别于 apply_anomaly 的"同种只延长不缩短"——2399 宿世归泯
    // 的"衰弱回合数归 1"是刷新可以向下）。end = roundCount + rounds，与 apply_anomaly
    // 同口径（施加时本回合还没打完，剩余 = rounds）。冻结不拦"设定"（那是写入不是减少）。
    if (!ctx->has_active_abnormal_status(target, status_id)) {
        return 0;   // 只对已存在的异常刷新（"归 1"语义，不无中生有）
    }
    ctx->set_abnormal_status_end_round(target, status_id, ctx->roundCount + rounds);
    return 1;
}

int dispel_active_anomalies(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return 0;
    }
    int dispelled = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (!ctx->has_active_abnormal_status(target, id)) {
            continue;
        }
        ctx->set_abnormal_status_end_round(target, id, 0);
        ++dispelled;
    }
    return dispelled;
}

int cure_anomalies(BattleContext* ctx, int target, const int* anomaly_ids, int count) {
    if (!ctx || target < 0 || target > 1 || !anomaly_ids || count <= 0) {
        return 0;
    }
    int cured = 0;
    for (int i = 0; i < count; ++i) {
        const int id = anomaly_ids[i];
        if (id < 0 || id > kOfficialAbnormalStatusMaxId) {
            continue;
        }
        if (!ctx->has_active_abnormal_status(target, id)) {
            continue;
        }
        ctx->set_abnormal_status_end_round(target, id, 0);
        ++cured;
    }
    return cured;
}

OffFieldTrueDamageResult deal_off_field_true_damage(BattleContext* ctx, int side, int slot,
                                                    int amount, int actor, int residue_floor) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5 || amount <= 0
        || residue_floor < 0) {
        return OffFieldTrueDamageResult::INVALID;
    }
    if (slot == ctx->on_stage[side]) {
        return OffFieldTrueDamageResult::INVALID;   // 在场走 deal_true_damage（有事件/管线语义）
    }
    if (ctx->is_vanished(side, slot)) {
        return OffFieldTrueDamageResult::INVALID;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    if (pet.hp <= 0) {
        return OffFieldTrueDamageResult::INVALID;
    }
    if (ctx->off_field_stats_protected(side, slot)) {
        return OffFieldTrueDamageResult::PROTECTED;   // 保护规则不是抗性，真伤照旧被挡
    }
    const bool would_kill = pet.hp - amount <= residue_floor;
    pet.hp = std::max(residue_floor, pet.hp - amount);
    if (pet.hp <= 0) {
        // 击杀走死亡漏斗：EVENT_DEATH + 拦截器（圣光灵神式场下复活照常响应）
        defeat_pet(ctx, side, slot, actor, DefeatCause::DAMAGE);
        return ctx->seerRobot[side].elfPets[slot].hp <= 0 ? OffFieldTrueDamageResult::DEFEATED
                                                          : OffFieldTrueDamageResult::DEALT;
    }
    return would_kill ? OffFieldTrueDamageResult::RESIDUE : OffFieldTrueDamageResult::DEALT;
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
    // ★ 次数型免断（"下1次免断"）在此消费：**仅当没有窗口票覆盖时**才烧次数票——
    //   元神神之宣告的免断 = 「1回合免断 ➕ 下1次免断」两层票（语料《机制解析—免断》
    //   + 作者顶置的官方 n-1 特判）：窗口期内断进来由窗口兜着，"下1次"留给窗口结束后。
    //   consume_immune 对窗口票（counts==0）天然不扣；is_immune_window 门控解决的是
    //   "两层并存"时保留次数票。（异常族"窗口在也烧次免"是 ANOMALY 官方口径，
    //   免断不随——分层是「下1次」措辞的本意。）
    if (ctx->is_immune(target, ImmunityType::BREAK, ctx->currentState)) {
        if (!ctx->rule_center_.is_immune_window(target, static_cast<int>(ImmunityType::BREAK),
                                                state_coverage_bit(ctx->currentState),
                                                ctx->roundCount)) {
            ctx->consume_immune(target, ImmunityType::BREAK, ctx->currentState);
        }
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
// 粉伤结算 — 一段粉伤走一遍 PinkDamagePipeline，当场落到体力
//
// 「多段粉」= **每次调用 = 一段**，逐段独立结算（各自取整/各自扣护罩/各自检测），
// 不累积不合并 —— 官方算例是 `300×0.65` 各自取整后相加，而不是 `(300×4)×0.65`（L445）。
// 所以引擎端没有"段计数"这种东西，`deal_pink_damage` 被调 N 次就是 N 段。
//
// 流程：PERCENT 换算 → 填 resolvedPink → 跑管线（增粉/抗性/特效免减/上限/护罩/检测）
//       → 剩余 > 0 才扣体力 + 发事件。
// ----------------------------------------------------------------
static void run_pink_damage(BattleContext* ctx, int target, int amount,
                            DamageKind kind, int actor) {
    ElfPet& pet = ctx->getPet(target);

    int value = amount;
    if (kind == DamageKind::PERCENT) {
        const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
        value = max_hp > 0 ? max_hp * amount / 100 : 0;
        if (value <= 0) {
            return;
        }
    }
    // PERCENT_VALUE：amount 已是具体伤害值（如"自身已损失体力50%"），不再换算——
    // 但**档位**仍属百分比伤害（走百分比抗性/护罩）。

    PinkDamageResolved& r = ctx->resolvedPink;
    r = PinkDamageResolved{};
    r.target = target;
    r.actor = actor;
    r.tier = (kind == DamageKind::FIXED) ? PinkDamageTier::FIXED : PinkDamageTier::PERCENT;
    r.raw = value;    // 粉转真按**原始量**转（不是被免减后的量）
    r.final = value;
    ctx->pink_damage_pipeline_.run(ctx, actor, target);

    // **粉转真**：检测点（`PINK_TO_TRUE_IMMUNE` / `PINK_TO_TRUE_UNHARMED`）上的效果
    // 自己把要转的量写进 `true_damage`，这里统一发真伤——直通护盾/护罩、穿抗性/免疫。
    // ⚠️ 检测点在护罩**之前**的那一条（免疫型）因此**不消耗护罩**（跑到它时护罩还没消费）；
    //    在护罩**之后**的那一条（未受到型）看得到 `absorbed`。
    // ⚠️ 引擎**不猜**转不转：没有全局开关，谁的效果谁负责置位（见结构体注释）。
    if (r.true_damage > 0) {
        deal_damage(ctx, target, r.true_damage, DamageKind::TRUE, actor);
        return;
    }
    if (r.final <= 0) {
        return;  // 本体体力不变（没受到粉伤：不算"受到"）
    }

    // 扣血
    const int hp_before = pet.hp;
    pet.hp -= r.final;
    if (pet.hp < 0) {
        pet.hp = 0;
    }
    const int actual_damage = hp_before - pet.hp;

    // 落血记录（弹伤族数据源）：粉伤档位照实记，读方按 kind 排除（粉伤斩杀不弹）。
    ctx->last_landed_hit[target][ctx->on_stage[target]] = {actor, actual_damage,
                                                           static_cast<int>(kind), true};
    mark_first_exhaust_flag(ctx, actor, target);

    // 粉伤事件：**只有真的结算到本体（体力下降）才发**。
    // 经过护罩抵消后没有剩余 → 上面就 return 了，这里根本到不了。
    // ⚠️ 这就是「免粉补偿」一类检测的全部依据（用户 2026-09-15 口径）：
    //    检测体力和护罩是否变化的那一类（L338 类2）在管线 DETECT 阶段读 `resolvedPink.absorbed`，
    //    只看体力的一类（类1）与本事件同构——本事件只是把"体力降了"这件事**类型化**，
    //    免得 watcher 靠 `ev.state` 反推这是不是粉伤时点。
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_TAKE_PINK_DAMAGE, actor, target,
                                        actual_damage, static_cast<int>(ctx->currentState)});
    emit_hp_exhausted(ctx, actor, target, kind);
    // 受到伤害事件（第三方"受到攻击伤害后/受高伤/受低伤"监听），amount = 实际扣血。
    // 带上 emit 当时的 FSM 时点：watcher 在 drain 时才跑，那时 currentState 已经推进，
    // 靠 ctx->currentState 判不出"是攻击伤害还是粉伤"（见 BattleEvent::state）。
    BattleEvent take_damage_ev{EventType::EVENT_TAKE_DAMAGE, actor, target,
                               actual_damage, static_cast<int>(ctx->currentState)};
    take_damage_ev.damage_kind = static_cast<int>(kind);
    ctx->event_center_.emit(take_damage_ev);
}

// ----------------------------------------------------------------
// deal_damage — 伤害原语（统一伤害入口）
//
// 粉伤（FIXED/PERCENT/PERCENT_VALUE）走 **PinkDamagePipeline**（见上），红伤(NORMAL)与本函数
// 内联的护盾吸收，真实伤害(TRUE)直通（官方护盾/护罩分离）。
// 护盾只响应红伤、护罩只响应粉伤；被击破时 emit EVENT_SHIELD_BROKEN。
// 攻击方可设 ws.ignore_shield 使本次攻击无视护盾/护罩响应。
// 所有伤害类机制都应走这里，避免效果函数直接改 hp 绕过管线。
// ----------------------------------------------------------------
void deal_damage(BattleContext* ctx, int target, int amount,
                 DamageKind kind, int actor) {
    if (target < 0 || target > 1 || amount <= 0) {
        return;
    }
    // 臣服(31)「无法对对手造成**攻击伤害、固定伤害、百分比伤害**」（官方 effect_des 31）。
    //
    // 用户 2026-09-18 口径：**只拦红伤与粉伤** ——「真伤拦不住，属性直伤也拦不住，
    // 身上有臣服也可以给对面附加烧伤然后造成真伤」。
    // 所以这里是**唯一**该拦红伤的地方（`DamageKind::NORMAL`），而：
    //   · `TRUE`（真伤，含异常扣血的 TruePercent 档）→ **放行**
    //   · `ATTRIBUTE`（属性直伤，之诗 2088 族走的 `deal_attribute_damage`）→ **放行**
    //   · `FIXED / PERCENT / PERCENT_VALUE`（粉伤）→ 在 `deal_pink_damage` 拦，
    //     **不在这里**拦：异常状态自身的扣血（沉默/流血/烈焰诅咒…）也走这三个档，
    //     且 `tick_abnormal_statuses` 的回合扣减点档把 actor 记成**对面**
    //     → 在这里拦会把"对手的异常扣血"一起误封（用户明确说这种情况要能造成伤害）。
    //     `fixed_damage()` 因 actor=-1 无归因而拦不到（已记待办）。
    // ⚠️ 只按"actor 有臣服且 actor != target"判：自伤/无源不受影响。
    if (kind == DamageKind::NORMAL && actor >= 0 && actor <= 1 && actor != target
        && ctx->has_active_abnormal_status(actor,
               static_cast<int>(AbnormalStatusId::Submission))) {
        return;
    }
    ElfPet& pet = ctx->getPet(target);
    if (pet.hp <= 0) {
        // 已倒地目标被伤害结算命中：**0 血状态检测**（2026-09-30 死亡模型，②范围即时
        // 免死的检测面——"范围内检测到 0 血就消耗免死"）。发预死亡信号让信号回调型
        // 免死（如帝皇之御）有机会拉起；随后本段伤害不落地（early return 保持）。
        // ⚠️ 同归标志不计数（mark_first_exhaust_flag 只在真实落血处写）。
        emit_hp_exhausted(ctx, actor, target, kind);
        return;  // 目标已死亡
    }

    if (kind == DamageKind::FIXED || kind == DamageKind::PERCENT
        || kind == DamageKind::PERCENT_VALUE) {
        run_pink_damage(ctx, target, amount, kind, actor);
        return;
    }

    // 护盾吸收：护盾只响应红伤(NORMAL)，真实伤害直通。
    // 攻击方可设 ws.ignore_shield[attacker] 使本次攻击无视护盾/护罩响应（如无极圣武魂印）。
    const bool ignore_bank = (actor >= 0 && actor <= 1 && ctx->ws.ignore_shield[actor]);
    int broken = 0;
    int remaining = amount;
    if (!ignore_bank && kind == DamageKind::NORMAL) {
        // 承伤乘区（精灵王线 K2）：封邪之嶂"每有1层印记护盾承伤值+10%"——
        // ws 每回合 reset，由混地魂印程序按层数重写（damage_add_pct 同套路）。
        remaining = pet.shield_bank_.absorb_with_bonus(
            amount, ctx->ws.shield_absorb_bonus_pct[target], &broken);
    }
    if (broken > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, actor, target});
    }
    if (remaining <= 0) {
        return;  // 护盾完全挡下
    }

    // 扣血
    const int hp_before = pet.hp;
    pet.hp -= remaining;
    if (pet.hp < 0) {
        pet.hp = 0;
    }
    const int actual_damage = hp_before - pet.hp;

    // 死亡归因：记下"这一槽最后的伤害是谁造成的"，供 EVENT_DEATH 带出击杀方
    // （空元之录的"自身击败对手后 / 己方其他精灵被击败后"两条归因条款靠它区分）。
    // 只在体力确实下降时写——被护盾完全挡下不算"造成伤害"（同"点数减伤打盔不消耗"口径）。
    if (actual_damage > 0) {
        ctx->last_damage_actor[target][ctx->on_stage[target]] = actor;
        // 落血记录（弹伤族数据源）：红伤(NORMAL)/真伤(TRUE)/属性直伤(ATTRIBUTE)档照实记，
        // 读方只认 NORMAL/ATTRIBUTE（粉伤在 run_pink_damage 自己的落血点记）。
        // 口径（2026-09-24 用户修正，替代 2026-09-20"实际掉血"拍板）：机盖弹伤 = 对手
        // **造成了多少红伤/属性直伤就全额反弹多少**（致死时以真伤全额反弹），不按剩余
        // 体力截断——记 remaining（护盾后全额伤害，过量击杀不缩水）。
        ctx->last_landed_hit[target][ctx->on_stage[target]] = {actor, remaining,
                                                               static_cast<int>(kind), true};
        mark_first_exhaust_flag(ctx, actor, target);
    }

    // 砥砺(37)「处于该状态的精灵**受到真实伤害后**，若**本回合未执行过附加异常状态的效果**，
    // 则**增加此伤害值 80%** 的体力」（官方 effect_des 300 / icon 37）。
    //
    // ⚠️ **就地钩在这里**，不开"真伤数值槽"：官方是"受到真伤后**实时**结算的增加体力"
    //    （语料 idx=157「每次受到真伤后触发实时结算的增加体力」），回血量此刻就可得；开数值槽
    //    反而把"实时"做成"迟一拍"。
    // ⚠️ 也不靠事件流事后判：`BattleEvent` **没有 `damage_kind` 字段**，三种档共用同一个
    //    `EVENT_TAKE_DAMAGE` → 从事件里分不出"刚才那次是不是真伤"。就地判 `kind == TRUE` 是唯一可靠口
    //    （同款先例：雷解 42 也在本文件就地判；`damage.isTrueDamage` 那条属技能伤害通道）。
    // ⚠️ 回血走 `heal_amount` 而非直写 hp：吃封回血 / 恢复效果修正%
    //    （语料 idx=165「可以被免疫效果响应」）。
    // ⚠️ 属性伤害(ATTRIBUTE)不是真伤 → 不触发；异常扣血的 TruePercent 档走 TRUE → **会**触发
    //    （官方"受到真实伤害"字面如此：中毒/烧伤/冻伤/寄生的 1/8 都算）。
    if (kind == DamageKind::TRUE && actual_damage > 0
        && ctx->has_active_abnormal_status(target,
               static_cast<int>(AbnormalStatusId::Resolution))
        && !ctx->ws.anomaly_applied_by_opponent[target]) {
        const int heal_back = actual_damage * 80 / 100;
        if (heal_back > 0) {
            heal_amount(ctx, target, heal_back);
        }
    }

    BattleEvent take_damage_ev{EventType::EVENT_TAKE_DAMAGE, actor, target,
                               actual_damage, static_cast<int>(ctx->currentState)};
    take_damage_ev.damage_kind = static_cast<int>(kind);
    ctx->event_center_.emit(take_damage_ev);
    emit_hp_exhausted(ctx, actor, target, kind);
}

// 真实伤害入口（插件可调）：走 deal_damage(TRUE)，返回"发生了什么"。
FixedDamageResult deal_true_damage(BattleContext* ctx, int target, int amount, int actor) {
    if (!ctx || target < 0 || target > 1 || amount <= 0) {
        return FixedDamageResult::INVALID_PARAM;
    }
    deal_damage(ctx, target, amount, DamageKind::TRUE, actor);
    return ctx->getPet(target).hp <= 0 ? FixedDamageResult::TARGET_DEFEATED
                                       : FixedDamageResult::SUCCESS;
}

// 属性伤害（《赛学必修16—伤害类型》）：数值 = 声明点数 × 克制倍数，**以红伤形式结算**。
//
// 它吃哪些修正（用户 2026-09-18 口径）：
//   「只吃**克制关系**」「会受到**锁伤**修正的影响，但是**常规挡伤**不会触发，只有像岚那种
//     **非本系抵挡**的挡伤才可以」「**不会被护盾/护罩抵挡**」「如果属性技能带有**穿透限伤**的
//     凭证也是可以无视的」。
//   → 阶段子集：`CAP`（锁伤）+ `BLOCK`（挡伤阶段）。
//     · 锁伤会生效，但**带 697 穿透限伤凭证时连锁伤一起无视**（→ 把 CAP 也踢出子集）；
//     · 常规挡伤靠 `skip_routine_block` 标志在 core 默认回调里自跳；系别条件性抵挡（岚）是插件
//       自己的 BLOCK 条目、照常被走到，由它按 `attribute_element` 判——这正是"只有系别抵挡可以"；
//     · 增伤/非通用增伤/各类减伤/保底/DETECT 全部**排除**（那些条目措辞都是"攻击伤害"）；
//     · 落血用 `DamageKind::ATTRIBUTE` → **护盾不吸收**（护罩在粉伤管线，红伤本就不经过它）。
//
// ⚠️ 快照要**保存/恢复**：属性伤害可能发生在一次攻击的**中途**（如某效果在 SKILL_EFFECT 里
//    "附加 X 点系伤害"），直接改写 `resolvedDamage` 会把在途的攻击结算弄脏。
// ⚠️ 克制倍数的**防御方一侧**取 `ws.view_elementalAttributes`（与伤害计算同一读取源——
//    `calculation.h` 的伤害公式用的就是这个视图；属性反转改的也是它）。
// 本笔属性伤害能否**穿锁伤**（官方 697「无视伤害限制效果」）。
// 来源两处，取或：
//   ① 本回合**所选技能自带** 697/750（`skill.penetration_flags`）——词条无附加限定
//      （"无视伤害限制效果"/"先出手时无视伤害限制效果"）→ 属性伤害同样吃；
//   ② RuleCenter 的 **PENETRATE_ATTRIBUTE 门**上有生效条目（薇尔诗·白皑之纷争那种
//      "直接无视、没有附加词条"→ 授予时**两条门都登记**，属性侧就是这条）。
//   ⚠️ 霍光·无罔之心「下2次**攻击技能**无视」只登记 **PENETRATE_ATTACK** → 属性伤害查的是
//      另一个门，**既不命中也不消耗**（用户 2026-09-18 口径）。
//   查询命中后同样**消费一次**（只扣次数型；窗口型不扣 → "当回合多次查询都不失效"）。
static bool attribute_damage_pierces_limit(BattleContext* ctx, int actor) {
    if (!ctx || actor < 0 || actor > 1) {
        return false;
    }
    bool pierce = false;
    // ① 所选技能自带
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (ctx->roundChoice[actor][0] == kActionSelectSkill) {
        const int slot = ctx->roundChoice[actor][1];
        const auto& skills = ctx->getPet(actor).skills;
        if (slot >= 0 && slot < static_cast<int>(skills.size())
            && skills[static_cast<std::size_t>(slot)].penetration_flags.ignore_damage_limit) {
            pierce = true;
        }
    }
    // ② 属性门上的凭证（查一次；命中则消费一次）
    if (!pierce) {
        const RuleCenter::PenetrationQuery q = ctx->rule_center_.query_penetrate(
            actor, RuleCategory::PENETRATE_ATTRIBUTE, ctx->roundCount);
        pierce = q.valid && q.ignore_damage_limit;
    }
    if (pierce) {
        ctx->rule_center_.consume_penetrate(actor, RuleCategory::PENETRATE_ATTRIBUTE,
                                            ctx->roundCount);
    }
    return pierce;
}

FixedDamageResult deal_attribute_damage(BattleContext* ctx, int target, int points,
                                        const int element[2], int actor) {
    if (!ctx || target < 0 || target > 1 || points <= 0 || !element) {
        return FixedDamageResult::INVALID_PARAM;
    }
    if (ctx->getPet(target).hp <= 0) {
        return FixedDamageResult::TARGET_DEFEATED;
    }
    const int defender_elem[2] = {ctx->ws.view_elementalAttributes[target][0],
                                  ctx->ws.view_elementalAttributes[target][1]};
    const double multiplier = Calculation::calculateRestraintMultiples(element, defender_elem);
    // 官方倍率档位是 {0, 0.5, 1, 2}（双属性相乘后可为 4 等）→ 取整用**截断**。
    // ⚠️ 取整口径（截断/四舍五入）未见官方明文，先按截断；若游戏内对照发现差 1 点，改这里。
    const int amount = static_cast<int>(static_cast<double>(points) * multiplier);
    if (amount <= 0) {
        return FixedDamageResult::SUCCESS;   // 倍率 0（被该系别免疫）或不足 1 点：不打伤害
    }

    const DamageSnapshot saved = ctx->resolvedDamage;   // 见上：可能打断在途攻击
    DamageSnapshot& d = ctx->resolvedDamage;
    d = DamageSnapshot{};
    d.attackerId = actor;
    d.defenderId = target;
    d.isRed = true;                 // 红伤：算红伤落血（但护盾不吸收，见落血的 kind）
    d.base = d.afterAdd = d.afterMul = d.final = amount;
    d.skip_routine_block = true;    // "免疫下N次攻击伤害"票不响应属性伤害
    d.attribute_element[0] = element[0];
    d.attribute_element[1] = element[1];

    // 带"穿透限伤"凭证 → 连**锁伤**也一起无视（把 CAP 踢出子集）。
    const bool pierce_limit = attribute_damage_pierces_limit(ctx, actor);
    const uint32_t phases = DamagePipeline::phase_bit(DamagePhase::BLOCK)
                          | (pierce_limit ? 0u : DamagePipeline::phase_bit(DamagePhase::CAP));
    ctx->damage_pipeline_.run(ctx, actor, target, phases);
    const int final_amount = d.final;
    ctx->resolvedDamage = saved;    // 恢复在途快照后再落血（落血不读快照）

    if (final_amount <= 0) {
        return FixedDamageResult::SUCCESS;   // 被锁到 0 / 被系别抵挡挡下
    }
    // 落血走 `deal_damage(ATTRIBUTE)`：**跳过护盾吸收** → 扣血 → EVENT_TAKE_DAMAGE + 死亡归因。
    // ⚠️ 不复用 apply_resolved_damage —— 它读 `ctx->resolvedDamage`（此刻已恢复成在途值）。
    deal_damage(ctx, target, final_amount, DamageKind::ATTRIBUTE, actor);
    return ctx->getPet(target).hp <= 0 ? FixedDamageResult::TARGET_DEFEATED
                                       : FixedDamageResult::SUCCESS;
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
    // 「攻击技能无法造成伤害」票（1090 族，2026-09-21 口径）：**同时封锁红伤与粉伤**——
    // 被压制一方的攻击效果（含强制执行放行的）产出粉伤一律无效。红伤那半边在
    // finish_attack_damage 前置归零；真伤不封（独立通道）。
    if (actor >= 0 && actor <= 1
        && ctx->rule_center_.is_attack_nullified(actor, ctx->roundCount)) {
        return FixedDamageResult::SUCCESS;   // 已"处理"，但不造成伤害
    }
    // 臣服(31)：**固定伤害与百分比伤害**被拦（官方 effect_des 31；用户 2026-09-18 口径）。
    // 这里是"效果对对手造成粉伤"的唯一插件入口（插件不链接 sim_core，调不到 deal_damage）
    // → 拦这里即可，且**不会**碰到异常自身的粉伤扣血（那些走内部 deal_damage → run_pink_damage）。
    // 真伤不走本函数（`deal_true_damage` 另有入口）→ 天然"拦不住真伤"，与口径一致。
    if (actor >= 0 && actor <= 1 && actor != target
        && ctx->has_active_abnormal_status(actor,
               static_cast<int>(AbnormalStatusId::Submission))) {
        return FixedDamageResult::SUCCESS;   // 已"处理"，但不造成伤害
    }
    deal_damage(ctx, target, amount, kind, actor);
    return ctx->getPet(target).hp <= 0 ? FixedDamageResult::TARGET_DEFEATED
                                       : FixedDamageResult::SUCCESS;
}

// 体力归零原语（"秒杀"族专用）：不是伤害——护盾/护罩/减伤/抗性一概不参与。
// 三路短路（命中任一即不归零，但事件照发 blocked=true，供转化方计数）：
//   ① 目标方 hp_zero_converted 标记（咤克斯式"秒杀改为使咤获得咒怨"，全队转化）；
//   ② 来源方瞬杀特性被抑制（suppress_common_trait，瞬杀被抑制≠不触发）；
//   ③ 目标方 RuleCenter 的 INSTANT_KILL 免疫票（奥菲次数型"免疫一次秒杀"/希望态永久型）。
HpZeroResult force_hp_to_zero(BattleContext* ctx, int target, int actor) {
    if (!ctx || target < 0 || target > 1) {
        return HpZeroResult::INVALID_PARAM;
    }
    ElfPet& pet = ctx->getPet(target);
    const int hp_before = pet.hp;
    if (hp_before <= 0) {
        return HpZeroResult::TARGET_DOWN;   // 目标本已倒地：无事发生、不发事件
    }
    bool short_circuited = ctx->hp_zero_converted[target];
    if (!short_circuited && actor >= 0 && actor <= 1) {
        short_circuited = ctx->is_trait_suppressed(actor, TraitKind::InstantKill);
    }
    // 秒杀免疫票（RuleCenter）：纯查询命中才消费——窗口/永久票（希望态 counts=0）命中不扣。
    if (!short_circuited
        && ctx->is_immune(target, ImmunityType::INSTANT_KILL, ctx->currentState)) {
        ctx->consume_immune(target, ImmunityType::INSTANT_KILL, ctx->currentState);
        short_circuited = true;
    }
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_HP_TO_ZERO, actor, target,
                                        hp_before, static_cast<int>(ctx->currentState),
                                        /*grant_id=*/-1, /*blocked=*/short_circuited});
    if (short_circuited) {
        return HpZeroResult::CONVERTED;
    }
    pet.hp = 0;
    // 死亡归因：秒杀也记来源方（actor 由调用点给出）——空元之录"自身击败对手后"要靠它。
    if (actor >= 0 && actor <= 1) {
        ctx->last_damage_actor[target][ctx->on_stage[target]] = actor;
    }
    return HpZeroResult::EXECUTED;
}

int seal_skill(BattleContext* ctx, int source, int target, int effect_id, bool attribute,
               bool attack, int count, int duration_rounds, bool penetrable, int source_slot,
               EffectScope scope, bool hit_invalid, int chance_pct, bool consumed_when_pierced) {
    if (!ctx || source < 0 || source > 1 || target < 0 || target > 1) {
        return 0;
    }
    // 次数 vs 回合**二选一**（见头文件）：次数型要 count>0；**回合型由 duration_rounds 表达、
    // count 无意义**（下面 duration 分支把 counts 固定传 0）。
    // ⚠️ 原守卫写的是 `count <= 0` 一律拒绝 → 与头文件"duration>0 走回合型"的文档矛盾，
    //    纯回合型封属必须凑一个 count=1 的假值才过得去（695/781 就是这么绕的，2026-09-17 踩到）。
    if (count <= 0 && duration_rounds <= 0) {
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

// ── PP 恢复（精灵王线 K7，2026-09-24）──
int pp_restore_slot(BattleContext* ctx, int side, int pet_slot, int skill_slot) {
    if (!ctx || side < 0 || side > 1 || pet_slot < 0 || pet_slot > 5 || skill_slot < 0
        || skill_slot > 4) {
        return 0;
    }
    Skills& skill = ctx->seerRobot[side].elfPets[pet_slot].skills[skill_slot];
    if (skill.pp == -1 || skill.maxPP <= 0 || skill.pp >= skill.maxPP) {
        return 0;
    }
    const int restored = skill.maxPP - skill.pp;
    skill.pp = skill.maxPP;
    return restored;
}

int pp_restore_points(BattleContext* ctx, int target, int slot, int points) {
    if (!ctx || target < 0 || target > 1 || slot < 0 || slot > 4 || points <= 0) {
        return 0;
    }
    Skills& skill = ctx->getPet(target).skills[slot];
    if (skill.pp == -1 || skill.maxPP <= 0) {
        return 0;
    }
    const int restored = std::min(points, skill.maxPP - skill.pp);
    if (restored <= 0) {
        return 0;
    }
    skill.pp += restored;
    return restored;
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
    // stat 域 = 能力等级槽 0..5：0=攻击 1=特攻 2=防御 3=特防 4=速度 **5=命中**。
    // ⚠️ 槽 5 是**命中**不是体力（2026-09-18 口径更正，见 battleContext.h 的长注释）——
    //    "基础数值"的第 6 项才是体力，两套索引只在 0..4 重合。
    //    既有插件 `moves_lib/stat_dispel.cpp` 的 `kStatMap` 早就在用 `stat_change(..., 5, ...)` 传命中。
    // ⚠️ 不用 `>= 6` 字面量：跟着 `kAbilityLevelSlotCount` 走，槽数变了一处改。
    if (!ctx || target < 0 || target > 1
        || stat < 0 || stat >= BattleContext::kAbilityLevelSlotCount) {
        return StatChangeResult::INVALID_PARAM;
    }
    // "无法处于能力提升状态"（ImmunityType::STAT_BOOST，混沌魔君索伦森 1011）：
    // 只挡**往上抬**的方向——往下压（delta<0）照常走。放在 AT_CAP 之前判定：
    // 被封锁时等级一点不动，与"到上限"是两回事，调用方要能分开。
    if (delta > 0 && ctx->is_immune(target, ImmunityType::STAT_BOOST, ctx->currentState)) {
        return StatChangeResult::BLOCKED;
    }
    int& level = ctx->ability_levels[target][stat];   // 本体在 context（on-stage 作用域）
    const int new_level = level + delta;
    if (new_level > 6 || new_level < -6) {
        return StatChangeResult::AT_CAP;  // 到上限/下限，不变更
    }
    // 本体写入 + 视图同步 + emit EVENT_STAT_CHANGED 三合一（见 write_ability_level 注释）。
    write_ability_level(ctx, target, stat, new_level);
    return StatChangeResult::SUCCESS;
}

// 内部：按指定量恢复，返回实际恢复量（封回血检查 + 恢复效果修正 + 记录 last_heal_amount）。
static int heal_impl(BattleContext* ctx, int target, int heal_amount) {
    ElfPet& pet = ctx->getPet(target);
    // 空位守卫（2026-09-26 上场少于 6）：空位 id=-1、hp=0，被群体恢复"复活"就是
    // 事故——一律 0（last_heal_amount 同步置 0，监听方不误触发）。
    // ⚠️ 用负 id 不用 0：0 是场景测试宠常用 id，不能误杀。
    if (pet.id < 0) {
        ctx->ws.last_heal_amount[target] = 0;
        return 0;
    }
    // ★ 死亡裁决封闭（2026-10-01 死切/终局改造 D2）：死亡漏斗已登记的**在场**宠，
    //   治疗 no-op —— 登记即裁决成立，"奶活"不可逆转死亡（fuzz seed 11263 活锁的
    //   根修半边：等待死切期间死亡被治疗翻转 → need_input 前提消失）。
    //   复活走 revive_pet 原语（登记复位后再可治疗）；免死/真2命拦截发生在登记
    //   **之前**，不受影响。嗑药不经本函数（"药是玩家操作"口径）。
    const int t_slot = ctx->on_stage[target];
    if (t_slot >= 0 && t_slot < 6 && ctx->pet_death_notified[target][t_slot]) {
        ctx->ws.last_heal_amount[target] = 0;
        return 0;
    }
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
    // 星赎(38)「**体力恢复效果提升30%**」（官方 effect_des 38；2026-09-18 扩表后接入）：
    // 与本修正**同通道相加**。
    // ⚠️ 只能在这里**读异常**，不能写成 `ctx->heal_mod_pct[o] += 30` —— 那是 context 级、
    //    跨回合不清（只在换宠/clearAllEffects 清）→ 会变成 30/60/90… 每回合累加。
    // ⚠️ 顺带：`heal_impl` 是吸血/吸取与一切恢复的必经点 → 星赎对它们一并生效；
    //    嗑药不走这里（`seer-robot.cpp` 直写 hp），符合"药是玩家操作"的既有口径。
    int heal_mod = ctx->heal_mod_pct[target];
    if (ctx->has_active_abnormal_status(target,
            static_cast<int>(AbnormalStatusId::StarRedemption))) {
        heal_mod += 30;
    }
    heal_amount = heal_amount * (100 + heal_mod) / 100;
    if (heal_amount < 0) {
        heal_amount = 0;
    }
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    const int hp_before = pet.hp;
    pet.hp = std::min(max_hp, pet.hp + heal_amount);
    const int actual = pet.hp - hp_before;
    ctx->ws.last_heal_amount[target] = actual;
    // 体力恢复落地事件（精灵王线 K3）：蛊类"对手每次回血后插入真伤"的检测点。
    // 只认实际恢复量 > 0——封回血/减疗吞掉的不算"恢复了"（解析 190：减疗期间不触发蛊）。
    if (actual > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_HEAL_RESTORED, -1, target,
                                            actual, static_cast<int>(ctx->currentState)});
    }
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

int reset_hp(BattleContext* ctx, int target, int pct) {
    if (!ctx || target < 0 || target > 1 || pct < 0) {
        return 0;
    }
    ElfPet& pet = ctx->getPet(target);
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        return 0;   // 0/0（消逝等）：没有可重置的体力标尺
    }
    if (pct > 100) {
        pct = 100;
    }
    const int before = pet.hp;
    // 重置 ≠ 恢复（见头文件口径）：刻意不走 heal_impl——不查封回血、不吃恢复修正、
    // 不记 last_heal_amount、不发 EVENT_HEAL_RESTORED（回血触发不认体力重置）。
    pet.hp = max_hp * pct / 100;
    return pet.hp - before;
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
    int* levels = ctx->ability_levels[target];
    int cleared = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (levels[i] > 0) {  // 只清提升（正等级），不动弱化/负等级
            // 写入走唯一落点：本体/视图同步 + emit EVENT_STAT_CHANGED
            //（视图是伤害公式的读取源，不同步会让本次消强在伤害上"没发生"）。
            write_ability_level(ctx, target, i, 0);
            ++cleared;
        }
    }
    // E13：提升被消除成功 → 广播（573"若能力强化状态被消除则…"的消费点）。
    if (cleared > 0) {
        BattleEvent lost_ev{EventType::EVENT_STAT_BOOSTS_LOST, /*actor=*/-1, target, cleared};
        lost_ev.stat_loss_kind = 0;   // 消除
        ctx->event_center_.emit(lost_ev);
    }
    return cleared;  // 0 = 目标本无提升（消强未成功）
}

int clear_stat_boosts_as(BattleContext* ctx, int target, int actor) {
    const int cleared = clear_stat_boosts(ctx, target);
    if (cleared > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_STAT_REMOVED, actor, target,
                                            cleared, static_cast<int>(ctx->currentState)});
    }
    return cleared;
}

// ── 主动消耗护盾（精灵王线 K2，2026-09-24）──
// 沧岚 2263「消耗自身的护盾值」/ 混地 2318「消耗自身的全部护盾并附加消耗量100%的
// 百分比伤害，以此法消耗的护盾视为被击破」用：消耗是**资源支出**而非挨打吸收，
// 被拿空的每条盾 emit 一次 EVENT_SHIELD_BROKEN（"视为被击破"喂给魂印的破盾子句）。
int consume_shield(BattleContext* ctx, int target, int amount, int* broken_count) {
    if (!ctx || target < 0 || target > 1 || amount < 0) {
        if (broken_count) {
            *broken_count = 0;
        }
        return 0;
    }
    int broken = 0;
    const int consumed = ctx->getPet(target).shield_bank_.consume(amount, &broken);
    for (int i = 0; i < broken; ++i) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, -1, target});
    }
    if (broken_count) {
        *broken_count = broken;
    }
    return consumed;
}

int clear_stat_drops(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return 0;
    }
    // ⚠️ 与 clear_stat_boosts 不对称：**故意不查任何"有利"免疫**。
    //   清弱化对目标有利——免弱(STAT_DROP) 挡"施加弱化"、免消除强化(STAT_CLEAR) 护"提升"，
    //   都不该挡"把弱化拿掉"（用户 2026-09-13 口径："清理弱化什么都不用查"）。
    // ⚠️ 唯一例外 = **能力下降锁定**（STAT_DROP_LOCK，1684 心朽魂凋 28808，2026-10-01
    //   补查询点——rule_center.h 早已承诺"解除命中返 0"，落地时漏接成死票）：
    //   对手给它上的锁是敌意效果，护的正是"它的下降不被拿走"，必须拦。
    if (ctx->is_immune(target, ImmunityType::STAT_DROP_LOCK, ctx->currentState)) {
        return 0;
    }
    int* levels = ctx->ability_levels[target];
    int cleared = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (levels[i] < 0) {  // 只清弱化（负等级），不动提升/正等级
            write_ability_level(ctx, target, i, 0);   // 本体/视图同步 + emit（同 clear_stat_boosts）
            ++cleared;
        }
    }
    return cleared;
}

StatDropResult stat_drop_piercing(BattleContext* ctx, int target, int stat, int amount) {
    // stat 域同 stat_drop：0..5（5 = 命中等级）。
    if (!ctx || target < 0 || target > 1
        || stat < 0 || stat >= BattleContext::kAbilityLevelSlotCount || amount <= 0) {
        return StatDropResult::INVALID_PARAM;
    }
    // ⚠️ **故意不查免弱**（STAT_DROP）——本原语存在的唯一理由。
    //    语料 idx=37：「异常转化为弱化会无视免弱」。查了就与官方口径相反。
    // ⚠️ 也不查 STAT_BOOST（"无法处于能力提升状态"）：那张票只挡**往上抬**的方向。
    int& level = ctx->ability_levels[target][stat];
    if (level <= -6) {
        return StatDropResult::AT_FLOOR;
    }
    const int new_level = std::max(level - amount, -6);
    // 写入仍走**唯一落点** `write_ability_level`（本体 + 视图 + emit EVENT_STAT_CHANGED）：
    // 衍化产生的等级变化同样要能被"写后即知"的窗口型监听器（如索伦森 1011 的压制）看见。
    write_ability_level(ctx, target, stat, new_level);
    return StatDropResult::SUCCESS;
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
    int& level = ctx->ability_levels[defender][stat];
    if (level <= 0) {
        return false;   // 没有正等级可破（负等级/零不动）
    }
    write_ability_level(ctx, defender, stat, 0);   // 本体/视图同步 + emit（同其它等级通道）
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
    int* src_levels = ctx->ability_levels[from];
    int moved = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = src_levels[i];
        if (lv <= 0) {
            continue;  // 只转化提升，不动弱化
        }
        write_ability_level(ctx, from, i, 0);   // from 清零（本体/视图同步 + emit）
        int gained = ctx->ability_levels[to][i] + lv;
        if (gained > 6) {
            gained = 6;   // 能力等级上限 +6（与 stat_change 一致）
        }
        // to 侧也是"往上抬"→ 同样受"无法处于能力提升状态"约束：
        // 如果 to 被封锁，本次搬运只清掉了 from、to 拿不到（与 STAT_CLEAR 被挡时"from 也拿不到"
        // 的分支不同——那是"不能失去"，这是"不能获得"，故按方向各自拦）。
        if (ctx->is_immune(to, ImmunityType::STAT_BOOST, ctx->currentState)) {
            ++moved;
            continue;
        }
        write_ability_level(ctx, to, i, gained);
        ++moved;
    }
    // E13：提升被吸取成功 → 广播（573"被消除**或吸取**"的另一半消费点）。
    if (moved > 0) {
        BattleEvent lost_ev{EventType::EVENT_STAT_BOOSTS_LOST, /*actor=*/to, from, moved};
        lost_ev.stat_loss_kind = 1;   // 被吸取
        ctx->event_center_.emit(lost_ev);
    }
    return moved;
}

int transfer_stat_boosts_as(BattleContext* ctx, int from, int to, int actor) {
    const int moved = transfer_stat_boosts(ctx, from, to);
    if (moved > 0) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_STAT_REMOVED, actor, from,
                                            moved, static_cast<int>(ctx->currentState)});
    }
    return moved;
}

// 弱化原语：把目标的能力等级往下压。规则见头文件（先查免弱、可穿强化保护、钳 -6）。
StatDropResult stat_drop(BattleContext* ctx, int target, int stat, int amount, int actor) {
    // stat 域 0..5（5 = 命中等级，同 stat_change 的说明）。
    if (!ctx || target < 0 || target > 1
        || stat < 0 || stat >= BattleContext::kAbilityLevelSlotCount || amount <= 0) {
        return StatDropResult::INVALID_PARAM;
    }
    // ⓪ **附加禁令**（ATTACH_BAN，2026-09-20 神觉·米斯蒂克 4676 引入，用户拍板的"查询式
    // 保护"落点）：按**施加方 actor** 判定——被禁方自己的技能/效果施加弱化被整次挡下；
    // 别的效果代为施加（如米斯蒂克魂印在对手行动中对对手的 -1，actor=米斯蒂克方）不受影响。
    // 弱化没有通道之分、没有反弹路径 → 任何 stat_drop 都查（stat_drop_piercing 是衍化
    // 专用、不查，见其注释）。窗口在票上（coverage=行动段时点位）：回合开始前/结束后的
    // 弱化天然不受影响。actor<0 = 调用方未声明 → 跳过本查询（免弱照查）。
    if (actor >= 0 && actor <= 1
        && ctx->rule_center_.has_attach_ban(actor, RuleCenter::kAttachBanStatDrop,
                                            state_coverage_bit(ctx->currentState),
                                            ctx->roundCount)) {
        return StatDropResult::IMMUNE;
    }
    // ① 免弱：**无条件、最先查**。目标身上还有强化也照样失败——不许拿"有 +N 可抵消"当理由降。
    if (ctx->is_immune(target, ImmunityType::STAT_DROP, ctx->currentState)) {
        return StatDropResult::IMMUNE;
    }
    // ⚠️ 此处**不查** STAT_CLEAR（免消除强化）：那个护的是"已有的提升被消除/吸取"，
    //    弱化是往下压，原理不同，可以穿过强化保护（用户 2026-09-13 定）。
    int& level = ctx->ability_levels[target][stat];
    if (level <= -6) {
        return StatDropResult::AT_FLOOR;  // 已到底，不越界
    }
    const int new_level = std::max(level - amount, -6);  // ② 不突破 -6（钳制）
    write_ability_level(ctx, target, stat, new_level);   // 视图同步（伤害/先手权读它）+ emit
    return StatDropResult::SUCCESS;
}

// 反转目标的能力下降：负等级 → 正等级（下降翻成提升），不动已有提升。
// 与 clear_stat_boosts（消除提升）独立——"反转"不等于"消除"。
StatReversalResult stat_reversal(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return StatReversalResult::NOTHING;
    }
    // 禁止反转（STAT_DROP_LOCK 通道，1684 心朽魂凋 28808，2026-10-01 落地——
    // 原 TODO 现接：rule_center.h 承诺的第二个查询点，"反转"命中返 BLOCKED）。
    if (ctx->is_immune(target, ImmunityType::STAT_DROP_LOCK, ctx->currentState)) {
        return StatReversalResult::BLOCKED;
    }
    int* levels = ctx->ability_levels[target];
    bool any_reversed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (levels[i] < 0) {    // 只反下降（负等级）
            // 下降 → 提升：这是"往上抬"，受"无法处于能力提升状态"约束。
            // 被封锁时该项**不反转**（保留负等级）——否则目标会进入它不该能处于的提升态。
            if (ctx->is_immune(target, ImmunityType::STAT_BOOST, ctx->currentState)) {
                continue;
            }
            write_ability_level(ctx, target, i, -levels[i]);
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
    int* levels = ctx->ability_levels[target];
    bool any_reversed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (levels[i] > 0) {   // 只反提升（正等级）
            write_ability_level(ctx, target, i, -levels[i]);   // 提升 → 下降（等量）
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
    Effect e(999201, 0, 0, 0, EffectArgs(std::vector<int>{owner, owner ^ 1, tier}),
             &effect_set_guaranteed_first);  // 必先授予（全参构造，Effect 无默认构造）
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
        // 已倒地目标被固伤结算命中：0 血状态检测（②范围即时免死检测面，与
        // deal_damage 尸体分支同口径，2026-09-30 哈莫链复刻）。
        emit_hp_exhausted(ctx, -1, target, DamageKind::FIXED);
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
    int slot = 0;
    for (Skills& skill : pet.skills) {
        if (skill.pp == -1) {
            ++slot;
            continue;  // 无限 PP 不参与
        }
        const int before = skill.pp;
        skill.pp = std::max(0, skill.pp - amount);
        if (skill.pp != before) {
            changed = true;
            ctx->event_center_.emit(BattleEvent{
                EventType::EVENT_PP_REDUCED, /*actor=*/-1, target,
                /*amount=*/before - skill.pp, static_cast<int>(ctx->currentState),
                /*grant_id=*/-1, /*blocked=*/false, /*slot=*/slot});
        }
        ++slot;
    }
    return changed ? PpReduceResult::SUCCESS : PpReduceResult::INVALID_PARAM;
}

PpReduceResult pp_zero_slot(BattleContext* ctx, int target, int slot, int actor) {
    if (!ctx || target < 0 || target > 1 || slot < 0
        || slot >= static_cast<int>(ctx->getPet(target).skills.size())) {
        return PpReduceResult::INVALID_PARAM;
    }
    Skills& skill = ctx->getPet(target).skills[static_cast<std::size_t>(slot)];
    if (skill.pp == -1) {
        return PpReduceResult::INVALID_PARAM;  // 无限 PP 不参与（也不发事件）
    }
    if (skill.pp <= 0) {
        return PpReduceResult::SUCCESS;  // 已是 0：幂等成功、无变化、不发事件
    }
    // PP 保留（琼华之庇，2026-09-25 语料 idx=94 口径"挂着1层印记可以跳掉帝天战佛"）：
    // 庇持有侧（pp_retained，豁免槽=莫妮卡自己）的技能对**对手来源**的清除/归零免疫——
    // 本击不归零，按"被清除但保留"发事件（blocked=true, amount=原值），莫妮卡魂印的
    // watcher 据此挂对手下回合攻先 ≤0 反应。消耗（自然使用，不经本原语）、调整
    // （pp_reduce 减 N）、无归属（actor=-1，如己方 1237 双方归零）不保留、也不触发反应。
    if (ctx->pp_retain_pet[target][slot] && actor == 1 - target) {
        ctx->event_center_.emit(BattleEvent{
            EventType::EVENT_PP_REDUCED, actor, target, skill.pp,
            static_cast<int>(ctx->currentState),
            /*grant_id=*/-1, /*blocked=*/true, /*slot=*/slot});
        return PpReduceResult::SUCCESS;  // 保留：PP 不变
    }
    const int before = skill.pp;
    skill.pp = 0;
    ctx->event_center_.emit(BattleEvent{
        EventType::EVENT_PP_REDUCED, /*actor=*/-1, target,
        /*amount=*/before, static_cast<int>(ctx->currentState),
        /*grant_id=*/-1, /*blocked=*/false, /*slot=*/slot});
    return PpReduceResult::SUCCESS;
}

int pp_restore(BattleContext* ctx, int target, int amount, bool only_empty) {
    if (!ctx || target < 0 || target > 1 || amount <= 0) {
        return 0;
    }
    ElfPet& pet = ctx->getPet(target);
    int changed = 0;
    for (Skills& skill : pet.skills) {
        if (skill.pp == -1) {
            continue;  // 无限 PP 不参与
        }
        if (only_empty && skill.pp != 0) {
            continue;  // 亮节口径：只恢复 pp==0 的槽
        }
        if (skill.pp >= skill.maxPP) {
            continue;  // 只补不削
        }
        const int before = skill.pp;
        skill.pp = std::min(skill.maxPP, skill.pp + amount);
        if (skill.pp != before) {
            ++changed;
        }
    }
    return changed;
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
    // ⚠️ 必须读 numericalBase（体力上限的权威槽）：numericalProperties[HP] 被 `hp` 成员
    //    别名（elf-pet.h 构造处 hp 是它的引用）——读它拿到的是**当前体力**，官方口径
    //    "吸取对手最大体力的1/{denom}"（CoreApi 槽注释同）。2026-09-24 批 9 光螳螂②修正。
    const int max_hp = std::max(1, defender.numericalBase[NumericalPropertyIndex::HP]);
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
    // 臣服(31)：吸取属"固定伤害 + 回等量体力" → 伤害被拦时**也不该回血**
    //（拦了伤害却照回血，会变成"臣服吸血流"这种没人想要的组合）。
    if (ctx->has_active_abnormal_status(actor,
            static_cast<int>(AbnormalStatusId::Submission))) {
        return DrainHpResult::SUCCESS;
    }
    deal_damage(ctx, target, amount, DamageKind::FIXED, actor);
    heal_impl(ctx, actor, amount);
    return DrainHpResult::SUCCESS;
}


// ════════════════════════════════════════════════════════════════════════
// 精灵生命周期：存活 / 死亡 / 消逝
//
// 三态定义与官方依据见 effects/spirit_lifecycle.h 头注释。一句话：
//   死亡 = 体力归零（可复活、仍占位）；消逝 = 体力上限归零（不可逆、从空间与位置剔除）。
// ════════════════════════════════════════════════════════════════════════

namespace {

// 官方取整（idx=274 尤纳斯算例，**对结果取整、不是对增减量取整**）：
//   提升："571×1.01=576.71，向下取整 576"      → new = floor(base×(100+pct)/100)
//   削减："576×0.9=518.4，向上取整 519"        → new = ceil (base×(100-pct)/100)
// ⚠️ 别写成"先算减掉的量再取整"：576 削 10% 那样会得到 518（差 1 点）。
//   两者都朝对精灵有利的方向取整（提升多拿、削减少掉）。
int hp_after_raise(int base, int pct) {
    return static_cast<int>((static_cast<long long>(base) * (100 + pct)) / 100);
}
int hp_after_reduce(int base, int pct) {
    const long long raw = static_cast<long long>(base) * (100 - pct);
    // 整数上取整除法（对负数也成立：clamp 到 floor 由调用方负责）
    return static_cast<int>((raw + 99) / 100);
}

} // namespace

int count_dead(const BattleContext* ctx, int side, DeadScope scope) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    // "不在场"与"场下"都排除在场精灵；"背包"与"全部阵亡"包含它（官方定义见 DeadScope）
    const bool exclude_on_stage =
        (scope == DeadScope::OFF_FIELD || scope == DeadScope::NOT_ON_STAGE);
    const int on_stage_slot = ctx->on_stage[side];
    int n = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->is_vanished(side, slot)) {
            continue;   // 已消逝：四个口径都不计
        }
        if (exclude_on_stage && slot == on_stage_slot) {
            continue;
        }
        if (ctx->seerRobot[side].elfPets[slot].hp <= 0) {
            ++n;
        }
    }
    // 额外精灵只进"不在场"与"全部阵亡"（官方 idx=149：额外精灵不属于背包、不属于场下，
    // 但属于不在场；"每有1只则xx"未写明背包/场下的默认含它）
    if (scope == DeadScope::NOT_ON_STAGE || scope == DeadScope::ALL) {
        n += ctx->count_extra_spirits(side, ExtraSpiritState::DEAD);
    }
    return n;
}

int reduce_max_hp_pct(BattleContext* ctx, int side, int slot, int pct, int floor) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6 || pct <= 0) {
        return -1;
    }
    if (ctx->is_vanished(side, slot)) {
        return -1;   // 上限已是 0：消逝不可逆，不接受任何数值操作
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    int& max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    const int before = max_hp;
    int after = hp_after_reduce(before, pct);
    if (floor < 1) {
        floor = 1;
    }
    // ★ 下限钳制：这是"上限==0 ⇔ 被消逝"这条不变量的**唯一守卫**。
    //   没有它，空元卪连杀 10 只就会把上限削到 0，把精灵误判成已消逝。
    if (after < floor) {
        after = floor;
    }
    max_hp = after;
    if (pet.hp > max_hp) {
        pet.hp = max_hp;   // 上限降到当前体力以下 → 同步压低（不可能高于上限）
    }
    return after;
}

int raise_max_hp_pct(BattleContext* ctx, int side, int slot, int pct) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6 || pct <= 0) {
        return -1;
    }
    if (ctx->is_vanished(side, slot)) {
        return -1;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    int& max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    max_hp = hp_after_raise(max_hp, pct);
    return max_hp;   // 提升上限不动当前体力
}

int reduce_max_hp_flat(BattleContext* ctx, int side, int slot, int amount, int floor) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6 || amount <= 0) {
        return -1;
    }
    if (ctx->is_vanished(side, slot)) {
        return -1;   // 上限已是 0：消逝不可逆，不接受任何数值操作
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    int& max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    int after = max_hp - amount;
    if (floor < 1) {
        floor = 1;
    }
    // ★ 下限钳制：同 reduce_max_hp_pct——"上限==0 ⇔ 被消逝"不变量的守卫。
    //   空元之渎的每回合 -200 也不会把上限削到 0。
    if (after < floor) {
        after = floor;
    }
    max_hp = after;
    if (pet.hp > max_hp) {
        pet.hp = max_hp;   // 上限降到当前体力以下 → 同步压低
    }
    return after;
}

int raise_max_hp_flat(BattleContext* ctx, int side, int slot, int amount) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6 || amount <= 0) {
        return -1;
    }
    if (ctx->is_vanished(side, slot)) {
        return -1;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    pet.numericalBase[NumericalPropertyIndex::HP] += amount;
    return pet.numericalBase[NumericalPropertyIndex::HP];   // 提升上限不动当前体力
}

DefeatResult defeat_pet(BattleContext* ctx, int side, int slot, int actor, DefeatCause cause) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return DefeatResult::INVALID;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    if (pet.hp > 0) {
        return DefeatResult::NOT_DOWN;   // 还没倒：调用方应先把体力打到 0
    }
    if (ctx->pet_death_notified[side][slot]) {
        return DefeatResult::ALREADY_DEAD;   // 幂等：重复调用安全（多时点都会来问一次）
    }
    // 拦截层：免死（残留体力）/ 复活（真2命）在"死亡成立"之前依次询问。
    // ⚠️ 消耗全部体力会跳过 sees_hp_consume=false 的条目——官方 idx=339：
    //    "所有的残留体力效果面对消耗体力效果，都不能免死，会被直接强制死亡"，
    //    而复活"只要触发就可以当回合不死"。两层可见性不同，故不能合成一个钩子。
    for (const DeathInterceptor& ic : ctx->death_interceptors) {
        if (!ic.fn) {
            continue;
        }
        if (cause == DefeatCause::HP_CONSUME && !ic.sees_hp_consume) {
            continue;
        }
        if (ic.fn(ctx, side, slot, actor, cause) && pet.hp > 0) {
            return DefeatResult::INTERCEPTED;   // 拦下且已落地（hp 已写回 > 0）
        }
    }
    ctx->pet_death_notified[side][slot] = true;
    // 目标真死：解除其身上全部消耗体力印记（"直到挂上印记的精灵死亡或切走为止"）
    ctx->clear_hp_consume_marks(side, slot);
    BattleEvent ev{EventType::EVENT_DEATH, actor, side, 0};
    ev.state = static_cast<int>(ctx->currentState);
    ev.slot = slot;
    ev.cause = static_cast<int>(cause);
    ctx->event_center_.emit(ev);
    return DefeatResult::DEFEATED;
}

int revive_pet(BattleContext* ctx, int side, int slot, int hp) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6 || hp <= 0) {
        return -1;
    }
    if (ctx->is_vanished(side, slot)) {
        return -1;   // 官方 idx=251：重生复活"被消逝的精灵除外"——消逝不可逆
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    // 空位守卫（同 heal_impl）：复活扫描（如重生之翼随机复活）不得选中空位——
    // max_hp=0 会钳回 0（数值无害）但会白耗一次复活名额。
    if (pet.id < 0) {
        return -1;
    }
    const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (hp > max_hp) {
        hp = max_hp;
    }
    pet.hp = hp;
    // 复位死亡登记：一只宠一生可"死亡 → 复活 → 再死"，第二次要能再发 EVENT_DEATH。
    ctx->pet_death_notified[side][slot] = false;
    return hp;
}

VanishResult vanish_spirit(BattleContext* ctx, int side, int slot, int actor) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return VanishResult::INVALID;
    }
    if (ctx->is_vanished(side, slot)) {
        return VanishResult::ALREADY_VANISHED;   // 先到先得：后到的消逝方拿不到收益
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    pet.hp = 0;                                              // 消逝蕴含阵亡
    pet.numericalBase[NumericalPropertyIndex::HP] = 0;       // 官方"体力上限直接变成 0/0"
    // 登记为"阵亡已处理"，但**不发 EVENT_DEATH**：消逝不是死亡——否则会误触发
    // 击败/亡语类效果（官方 idx=149 #5 额外精灵死亡不触发击败效果同源）。
    ctx->pet_death_notified[side][slot] = true;
    BattleEvent ev{EventType::EVENT_VANISH, actor, side, 0};
    ev.state = static_cast<int>(ctx->currentState);
    ev.slot = slot;
    ctx->event_center_.emit(ev);
    return VanishResult::VANISHED;
}

int vanish_dead_spirits(BattleContext* ctx, int side, int count, bool include_extra) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    int done = 0;
    const bool unlimited = (count <= 0);
    for (int slot = 0; slot < 6; ++slot) {
        if (!unlimited && done >= count) {
            break;
        }
        if (ctx->is_vanished(side, slot)) {
            continue;
        }
        if (ctx->seerRobot[side].elfPets[slot].hp > 0) {
            continue;   // 只消逝**已阵亡**的（"令对方全部阵亡精灵消逝"）
        }
        if (vanish_spirit(ctx, side, slot, -1) == VanishResult::VANISHED) {
            ++done;
        }
    }
    // ⚠️ 待实测／待做：官方 idx=33 说魂帝在"某方同时有阵亡的额外精灵和场下阵亡精灵"时
    //    **随机**消逝其中一个（"运气不好会消逝自己的尸骸"）。这里实现为**本体优先**的
    //    确定性选择——随机版本要等有官方口径/需要复现时再加（场景断言也需要确定性）。
    if (include_extra && (unlimited || done < count)) {
        for (ExtraSpirit& es : ctx->extra_spirits[side]) {
            if (!unlimited && done >= count) {
                break;
            }
            if (es.state != ExtraSpiritState::DEAD) {
                continue;
            }
            es.state = ExtraSpiritState::VANISHED;
            ++done;
        }
    }
    return done;
}

void register_death_interceptor(BattleContext* ctx, DeathInterceptor interceptor) {
    if (!ctx) {
        return;
    }
    if (interceptor.source_effect_id != 0) {
        remove_death_interceptors(ctx, interceptor.source_effect_id);   // 同源覆盖：幂等重注册
    }
    ctx->death_interceptors.push_back(std::move(interceptor));
}

void remove_death_interceptors(BattleContext* ctx, int source_effect_id) {
    if (!ctx) {
        return;
    }
    std::vector<DeathInterceptor>& v = ctx->death_interceptors;
    v.erase(std::remove_if(v.begin(), v.end(),
                           [source_effect_id](const DeathInterceptor& ic) {
                               return ic.source_effect_id == source_effect_id;
                           }),
            v.end());
}

void sync_pending_deaths(BattleContext* ctx) {
    if (!ctx) {
        return;
    }
    for (int side = 0; side < 2; ++side) {
        for (int slot = 0; slot < 6; ++slot) {
            if (ctx->is_vanished(side, slot)) {
                continue;   // 消逝不是死亡事件，不给它补发
            }
            if (ctx->seerRobot[side].elfPets[slot].hp > 0) {
                // 曾经死过又活着 = 被复活过 → 复位登记，允许"再死一次"能再通知
                if (ctx->pet_death_notified[side][slot]) {
                    ctx->pet_death_notified[side][slot] = false;
                }
                continue;
            }
            // 漏发的死亡（绕过原语的直写路径：咤连锁击杀场下宠、反弹伤害直写 hp-=n…）
            defeat_pet(ctx, side, slot, -1, DefeatCause::EFFECT);
        }
    }
}
