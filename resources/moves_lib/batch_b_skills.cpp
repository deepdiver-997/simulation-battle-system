#include <utils/sbs_rand.h>
// 技能组批产 B 组：高引用共享效果 id（2026-09-27，feat/skill-batch-b）
//
// 工单：docs_local/docs/05-任务清单/技能组批产工单.md（B 组 = 共享效果批产）。
// 本文件一次实现多个精灵共同引用的效果 id；每只精灵的验收场景见 test/scenario/150~159。
//
// ══ 实现清单（SKILL_EFFECT 注册）══
//   共享批（工单点名）：
//     1   吸取族：给予对方损伤的一半会恢复自己的体力（0 参）
//     38  降低对方{0}点战斗时的最高HP（reduce_max_hp_flat）
//     55  {0}回合内自身与对方反转战斗时属性（交换 ws.view_elementalAttributes）
//     63  将能力下降状态反馈给对手（crawford 1876 同款镜像，无封技半边）
//     64  自身在烧伤、冻伤、中毒状态下造成的伤害反而加倍（ws.damage_add_pct）
//     66  当次攻击击败对方出战精灵时恢复自身最大体力的1/{0}（击败时点探针）
//     67  当次攻击击败对方时减少对方下次出战精灵的最大体力1/{0}（arm+登场 watcher）
//     89  {0}回合内每次造成伤害的1/{1}恢复自己的HP（EVENT_TAKE_DAMAGE watcher）
//     94  命中后{0}%令对手石化（apply_anomaly_checked）
//     102 对手处于麻痹状态时，威力翻倍（变威力 skill_power_view）
//     106 {0}回合内特殊攻击对自身必定miss（MOVE_RIGHT 查选技类别 → hit_rate_mod=0）
//     128 {0}回合内将受到的攻击伤害转化为自体现力（EVENT_TAKE_DAMAGE watcher）
//     155 恢复全部体力，消除所有能力下降，使自己睡眠{0}回合
//     165 {0}回合内每回合防御和特防等级+{1}（ROUND_START 窗口 stat_change）
//     185 若击败{0}的对手则对方下只出战精灵{0}（{0}=异常码；击败探针+登场 watcher）
//     198 随机使对手{0}种能力等级-{1}（stat_drop 随机槽）
//     440 {0}回合内对手使用技能消耗的PP值变为{1}倍（ws.skill_pp_cost_multiplier）
//     599 {0}回合内受到{1}伤害减少{2}%（{1}：0=物理/1=特殊/2=全部；damage_reduce_mul）
//     620 {0}回合内致命一击率上升{1}/16（ws.crit_rate_add）
//     626 随机使自己{0}项能力+{1}（stat_change 随机槽）
//     660 消除{0}状态，消除成功恢复{1}%体力（{0}：0=对手能力提升/1=自身能力下降）
//     931 吸取对手能力提升状态，吸取成功则对手{0}技能PP值归零（随机槽）
//   精灵专属批（第二批，逐批追加）：111/880/884/1039/788/606/1176/1010/868/864/807/861/756/
//     438/765/1528/1529/1530/1235/1655/1469/565/148/2145/761/561/727/587/524/1 见上/
//     1140/877/1207/1171/126/187/502/1154/1351/1138/871/540/723/834/1770/1025/983/
//     1099/799/1568/1113/2144/1603/1463/2163/1711/1868
//
// ══ 明确不做 / 非缺口（验收记录）══
//   · 697 / 699（无视伤害限制 / 无视攻击免疫）：**内核已处理，非缺口**——effect_meta
//     把它们分类为 Penetration（src/effects/effect_meta.cpp kMetaOverrides），loadSkills
//     时并入 Skills::penetration_flags（skills.cpp penetration_flags_for_effect），出手时
//     materialize_attack_credential 合成 ws.attack_credential 供门判定/伤害管线消费。
//     注册 SKILL_EFFECT 反而多余（loadSkills 对该分类 continue，不会走注册路径）。
//   · 118（随机改为{0}~{1}点威力）：**运行时无参数，无法实现**——side_effect 表
//     arg_count=0 而 effect_info.args_num=2，既存口径不一致（build_skill_effects 注释
//     点名 118/596/1154"以 side_effect 为准、不可互换"）→ 切片恒 0 参，威力区间无从
//     取得。需数据层修正 side_effect.arg_count 后再实现（或内核喂参）。涂蝶混色等
//     引用技的该子句暂缺，记入台账。
//   · 828 / 1774 / 1395（条件先制+1 / 对手用攻击技自身先制+1 / 先出手必暴）：注册时点
//     需要在 MOVE_RIGHT（先手权比较前 / 暴击掷点前）——引擎的"选择期效果路由"是
//     src/entities/skills.cpp 里的硬编码 id 表（2000/610/954/1848/881/1532），SKILL_EFFECT
//     时点的处理器救不回本回合的先制/暴击。禁改内核 → 跳过并记录。
//   · 2190（模板字面 "{0}"，side_effect.arg_count=0）：运行时无参，且它的 4 个引用技
//     （无为觉者 十玄释/百法拜/孑身诫/千秋虔）各挂**不同**的"无为：…"子句——共享
//     处理器拿不到技能 id 无法区分。跳过并记录。
//   · 1154 的翻倍条件：side_effect.arg_count=2 → {2}（翻倍条件码）切片不了；两个已知
//     引用技（憾地之力/无上荣光）的官方文案都是"自身体力低于对手则翻倍" → 硬编码该
//     条件，出现异码引用时再泛化（文件头记待拍板）。
//
// ══ 约定 ═══════════════════════════════════════════════════════════
//   · args = [owner, target, ...切片参数]（build_effect_args_for_skill 前置 0/1，
//     bind_participants 运行时改写）。
//   · 概率施加走 apply_anomaly_checked（申报概率、受亮节闸门管辖）；100% 时
//     roll_percent 短路真、不耗 rand（与既有场景确定性约定一致）。
//   · 击败类探针挂 BATTLE_OPPONENT_DEFEAT_STATUS（zhakes 2017 同款：duration=1 本回合
//     一次性 + find_defeated_foe_slot 归因，last_damage_actor 为真相源）。
#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

// ── 通用小工具（jiyuan_ds001_skills.cpp 同款，插件各自持有一份）─────────────

// AFTER_ACTION / ACTION_END 桶的当前行动方（preemptive_right 推，与 FSM 同源）。
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

// 击杀归因（soul_lib find_defeated_foe_slot 同款内联：对手在场宠倒地 + 最后一击是本方）。
int batch_b_defeated_foe_slot(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return -1;
    }
    const int foe = 1 - owner;
    const int foe_slot = ctx->on_stage[foe];
    if (ctx->seerRobot[foe].elfPets[foe_slot].hp > 0) {
        return -1;
    }
    if (ctx->last_damage_actor[foe][foe_slot] != owner) {
        return -1;
    }
    return foe_slot;
}

// owner 本回合是否先出手（preemptive_right 在 MOVE_RIGHT 已定，SKILL_EFFECT 之后可读）。
bool batch_b_is_first_side(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    const bool first_is_0 = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_1;
    return owner == 0 ? first_is_0 : !first_is_0;
}

// 对方本回合选中的技能下标（-1 = 没选技能）。roundChoice 在选择期写入、跨 ROUND_START 保留。
int batch_b_selected_skill_index(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return -1;
    }
    if (ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return -1;
    }
    const int idx = ctx->roundChoice[side][1];
    const int count = static_cast<int>(
        ctx->seerRobot[side].elfPets[ctx->on_stage[side]].skills.size());
    return (idx >= 0 && idx < count) ? idx : -1;
}

// 从 6 个能力槽里随机挑 want 个不同槽（want > 6 时按 6）。返回选中槽列表。
std::vector<int> batch_b_pick_random_stats(int want) {
    std::vector<int> all{0, 1, 2, 3, 4, 5};
    if (want >= static_cast<int>(all.size())) {
        return all;
    }
    // Fisher-Yates 局部洗牌（sbs_rand()；场景断言按"总和"写、不依赖具体槽）。
    for (int i = 0; i < want; ++i) {
        const int j = i + (sbs_rand() % (static_cast<int>(all.size()) - i));
        std::swap(all[static_cast<std::size_t>(i)], all[static_cast<std::size_t>(j)]);
    }
    all.resize(static_cast<std::size_t>(want));
    return all;
}

// 随机挑 want 个不同技能槽做 PP 归零（want >= 5 → 全部；跳过无限 PP 槽由原语处理）。
void batch_b_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return;
    }
    std::vector<int> slots{0, 1, 2, 3, 4};
    const int n = std::min(want, static_cast<int>(slots.size()));
    for (int i = 0; i < n; ++i) {
        const int j = i + (sbs_rand() % (static_cast<int>(slots.size()) - i));
        std::swap(slots[static_cast<std::size_t>(i)], slots[static_cast<std::size_t>(j)]);
        (void)g_core->pp_zero_slot(ctx, target, slots[static_cast<std::size_t>(i)], actor);
    }
}

// 现代异常施加（100% 短路不耗 rand；<100% 走申报概率，受亮节闸门管辖）。
bool batch_b_apply_anomaly(BattleContext* ctx, int target, int anomaly, int actor, int chance) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return false;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), actor, chance, ChanceSource::Skill);
    return r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// plugin_storage 键（本文件私有命名空间；int 键 + int/bool 值，跨 dylib 只信基本类型）。
constexpr int kKeyBase = 77000000;
constexpr int kWatch67Arm = kKeyBase + 670;     // 67 击败 arm 标记（+owner）
constexpr int kWatch67Watch = kKeyBase + 671;   // 67 登场 watcher 已注册（+owner）
constexpr int kWatch185Arm = kKeyBase + 1850;   // 185 击败 arm 标记（+owner）
constexpr int kWatch185Watch = kKeyBase + 1851; // 185 登场 watcher 已注册（+owner）
constexpr int kSnap727Base = kKeyBase + 7270;   // 727 等级快照（+owner*10+槽）→ 值=等级
constexpr int kSnap727Round = kKeyBase + 7271;  // 727 快照回合（+owner）
constexpr int kSnap727Armed = kKeyBase + 7272;  // 727 快照窗口已注册（+owner）
constexpr int kLedger807Base = kKeyBase + 8070; // 807 对手上次伤害台账（+owner）
constexpr int kWatch807Armed = kKeyBase + 8071; // 807 台账 watcher 已注册（+owner）
constexpr int kIntercept561Arm = kKeyBase + 5610;    // 561 拦截器本回合 armed（+owner）
constexpr int kIntercept1463Arm = kKeyBase + 14630;  // 1463 拦截器本回合 armed（+owner）

// ════════════════════════════════════════════════════════════════════
// 1 —— 给予对方损伤的一半会恢复自己的体力（吸取/强力吸取/超级吸取…12156 技引用）
// Args: [owner, target]（0 参模板）。
// "损伤" = 对手实际受到的红伤（吸血族口径：读 resolvedDamage.final，1256/101 同族）。
// 处理器挂 AFTER_ACTION（伤害结算后可知），attackerId 归因防串位。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_1_after_action(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, arm_round}
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || args.int_args[2] != ctx->roundCount
        || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (ctx->ws.resolvedDamage.attackerId != owner) {
        return EffectResult::kOk;   // 本回合本方没打出攻击伤害（属性技/被盔等）
    }
    const int dealt = ctx->ws.resolvedDamage.final;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt / 2);   // "损伤的一半"
    }
    return EffectResult::kOk;
}

EffectResult effect_1_drain_half(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, 1 - owner, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1, 0, owner, -1, EffectArgs(payload), &effect_1_after_action);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 38 —— 降低对方{0}点战斗时的最高HP（无为觉者·无兆乱 官方文案 50 点）
// Args: [owner, target, points]。体力上限削减走原语（下限钳 1、不致死、同步压当前体力）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_38_reduce_max_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->reduce_max_hp_flat) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || points <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->reduce_max_hp_flat(ctx, opp, ctx->on_stage[opp], points, /*floor=*/1);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 55 —— {0}回合内自身与对方反转战斗时属性（涂蝶·地狱变相 / 属性反转 20113）
// Args: [owner, target, rounds]。
// 伤害公式的系别读取源 = ws.view_elementalAttributes（克制倍数、785 微弱判定同源）；
// 每回合 ROUND_START（sync_workspace_from_on_stage 之后）对调双方视图。
// 已知近似：回合中段换宠会 perform_switch→sync 重基视图，当回合剩余时间的反转丢失。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_55_swap_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 2; ++i) {
        std::swap(ctx->ws.view_elementalAttributes[owner][i],
                  ctx->ws.view_elementalAttributes[opp][i]);
    }
    return EffectResult::kOk;
}

EffectResult effect_55_reverse_element(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 立即 tick 一次（effect 54 先例）：首出手方注册当回合的 ROUND_START 桶已跑过，
    // 本回合伤害结算（在 SKILL_EFFECT 之后）就要读反转后的视图。
    effect_55_swap_tick(ctx, EffectArgs(std::vector<int>{owner, 1 - owner}));
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 55, rounds,
                          {owner, 1 - owner}, &effect_55_swap_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 63 —— 将能力下降状态反馈给对手（不灭地威·萨瑞卡·大地战栗）
// Args: [owner, target]。
// 2026-10-01 人工裁定细化（交接档"裁定批量落档"§3）：两步 = ①解除自身下降；
// ②把解除的下降**同等级**附加给对手。整体成功 = 两步都成功：
//   · 自身下降被锁（1684 STAT_DROP_LOCK 通道）→ 无法解除 → 整体不触发、自身不清；
//   · 对手免弱（STAT_DROP）→ 镜像必全 IMMUNE → 整体不触发、自身不清（原子性）；
//   · 对手某槽已在 -6（stat_drop 返 AT_FLOOR）→ 以独立结果信号区分；
//     2026-10-25 口径：全槽 AT_FLOOR（对手全未实际下降）= **未触发/失败档**——下游
//     "反馈成功则…"类链消费信号 2 时须按未触发处理（同 1876 龙威语义，见场景 090 K2）。
// 结果信号（F9 审计槽惯例）：plugin_storage[63000063+owner] =
//   1=成功（有实际下降落地）/ 2=全 AT_FLOOR（对手未实际下降＝未触发档）/ 3=失败。
//   供"反馈成功则…"类后续链消费（crawford 1876 的龙威语义同源，见场景 090 K1/K2）。
// 无下降可反馈 = 整体不触发（crawford 1876 同款判据，去掉封技半边）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_63_reflect_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_drop
        || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // 快照自身负等级（解除前取值，镜像要用）
    int neg_slots[6];
    int neg_lvl[6];
    int neg_count = 0;
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            neg_slots[neg_count] = i;
            neg_lvl[neg_count] = ctx->ability_levels[owner][i];
            ++neg_count;
        }
    }
    if (neg_count == 0) {
        return EffectResult::kOk;   // 无下降可反馈
    }
    // 门槛一：自身下降被锁 → 无法解除 → 整体不触发（自身不清、对手不降）
    if (ctx->is_immune(owner, ImmunityType::STAT_DROP_LOCK, ctx->currentState)) {
        ctx->plugin_storage[63000063 + owner] = 3;
        return EffectResult::kOk;
    }
    // 门槛二：对手免弱 → 镜像必全 IMMUNE → 整体不触发（原子：自身不清）
    if (ctx->is_immune(opp, ImmunityType::STAT_DROP, ctx->currentState)) {
        ctx->plugin_storage[63000063 + owner] = 3;
        return EffectResult::kOk;
    }
    // 步骤①：解除自身下降
    (void)g_core->clear_stat_drops(ctx, owner);
    // 步骤②：镜像附加（免弱已预检 → 只会 SUCCESS / AT_FLOOR）
    bool any_landed = false;
    for (int i = 0; i < neg_count; ++i) {
        const auto r = g_core->stat_drop(ctx, opp, neg_slots[i], -neg_lvl[i], owner);
        if (r == StatDropResult::SUCCESS) {
            any_landed = true;
        }
    }
    // 结果信号：有实际落地=1；全 AT_FLOOR（对手未实际下降）=2
    ctx->plugin_storage[63000063 + owner] = any_landed ? 1 : 2;
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 64 —— 自身在烧伤、冻伤、中毒状态下造成的伤害反而加倍（启灵元神·天堂圣光）
// Args: [owner, target]。通用增伤（无"额外"措辞）→ 加算通道 +100%。
// 按次结算（SKILL_EFFECT 时查自身异常），异常消失后自然失效。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_64_status_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 插件读异常槽走 CoreApi（has_active_abnormal_status 非 inline，击杀线教训 #4）。
    const bool statused = (g_core && g_core->has_active_anomaly)
        && (g_core->has_active_anomaly(ctx, owner, static_cast<int>(AbnormalStatusId::Burn))
            || g_core->has_active_anomaly(ctx, owner,
                                          static_cast<int>(AbnormalStatusId::Frostbite))
            || g_core->has_active_anomaly(ctx, owner,
                                          static_cast<int>(AbnormalStatusId::Poison)));
    if (statused) {
        ctx->ws.damage_add_pct[owner] += 100.0f;   // "反而加倍"
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 66 —— 当次攻击击败对方出战精灵时恢复自身最大体力的1/{0}（咆哮裂地击 1/5）
// Args: [owner, target, denom]。击败时点探针（BATTLE_OPPONENT_DEFEAT_STATUS，本回合一次）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_66_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (batch_b_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;   // 本回合没有本方击败对手
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

EffectResult effect_66_heal_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    Effect e(66, 0, owner, 1, EffectArgs(std::vector<int>{owner, 1 - owner, args.int_args[2]}),
             &effect_66_kill_probe);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                 owner, /*duration=*/1, ctx->roundCount);
    ce->source_id_ = 66;
    ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, owner, std::move(ce));
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 67 —— 当次攻击击败对方时减少对方下次出战精灵的最大体力1/{0}（疲乏撕咬/狂决斗魇破）
// Args: [owner, target, denom]。两段式（zhakes 2017 同款）：
//   ① 击败探针置 arm；② 登场 watcher：arm 后对方**下一只**登场精灵 maxHP - max/{0}。
// 削减走 reduce_max_hp_flat（下限钳 1、不致死）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_67_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (batch_b_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kWatch67Arm + owner] = 1;
    return EffectResult::kOk;
}

EffectResult effect_67_cut_next_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = 1 - owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // ① 击败探针（本回合一次性）
    {
        Effect e(67, 0, owner, 1, EffectArgs(std::vector<int>{owner, opp}),
                 &effect_67_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                     owner, /*duration=*/1, ctx->roundCount);
        ce->source_id_ = 67;
        ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, owner, std::move(ce));
    }
    // ② 登场 watcher（幂等注册；arm 后下一只登场的对方精灵挨削减）
    if (!ctx->plugin_storage.count(kWatch67Watch + owner)) {
        const int denom = args.int_args[2];
        if (denom <= 0 || !g_core->reduce_max_hp_flat) {
            return EffectResult::kOk;
        }
        ctx->plugin_storage.emplace(kWatch67Watch + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, opp, denom](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != opp) {
                    return;
                }
                auto it = c->plugin_storage.find(kWatch67Arm + owner);
                if (it == c->plugin_storage.end()) {
                    return;
                }
                const int* armed = std::any_cast<int>(&it->second);
                if (!armed || *armed == 0) {
                    return;
                }
                c->plugin_storage[kWatch67Arm + owner] = 0;   // "下次出战"一次
                if (g_core && g_core->reduce_max_hp_flat) {
                    const int slot = c->on_stage[opp];
                    const int max_hp =
                        c->seerRobot[opp].elfPets[slot].numericalBase[NumericalPropertyIndex::HP];
                    (void)g_core->reduce_max_hp_flat(c, opp, slot, max_hp / denom, /*floor=*/1);
                }
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 89 —— {0}回合内每次造成伤害的1/{1}会恢复自己的HP（蜜桃甜甜弹/寒冰藤蔓/炼狱业舞）
// Args: [owner, target, rounds, denom]。EVENT_TAKE_DAMAGE watcher（actor 归因本方）。
// watcher 的回合窗口在回合扣减点结算，边界与 InRounds 约定近似（后手不顺延），记注释。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_89_lifesteal_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.amount <= 0) {
                return;
            }
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(c, owner, ev.amount / denom);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 94 —— 命中后{0}%令对手石化（石化之肤/石化之视）
// Args: [owner, target, chance]。现代施加（免疫/弹控/抗性/转化全链生效）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_94_petrify(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || chance <= 0) {
        return EffectResult::kOk;
    }
    (void)batch_b_apply_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Petrify), owner,
                                chance);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 102 —— 对手处于麻痹状态时，威力翻倍（电能引爆）
// Args: [owner, target]（0 参模板）。变威力：写 ws.skill_power_view（自动触发重算）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_102_paralyze_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Paralysis))) {
        return EffectResult::kOk;
    }
    const int view = ctx->ws.skill_power_view[owner];
    if (view > 0) {
        ctx->ws.skill_power_view[owner] = view * 2;   // 翻倍（写=声明变威力）
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 106 —— {0}回合内特殊攻击对自身必定miss（致虚守静/光涌之爪）
// Args: [owner, target, rounds]。
// MOVE_RIGHT 是共享时点（每回合一次、先于暴击掷点与命中率 roll）：窗口 tick 查对方
// 本回合选技是否 Special → 是则把攻方 hit_rate_mod 置 0（accuracy=0 → 必 miss）。
// 必中技能照官方口径绕过（cred.must_hit 跳过整个命中判定——与 effect 86 同性）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_106_special_miss_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int idx = batch_b_selected_skill_index(ctx, opp);
    if (idx < 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).skills[static_cast<std::size_t>(idx)].type == SkillType::Special) {
        ctx->ws.hit_rate_mod[opp] = 0.0f;   // ws 每回合 reset，窗口每回合重写
    }
    return EffectResult::kOk;
}

EffectResult effect_106_special_miss(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 首出手方：本回合的 MOVE_RIGHT 桶已跑过 → 立即查一次当回合（对方还没出手，
    // 命中判定在其 SKILL_EFFECT 之前的时点之后仍会读 hit_rate_mod）。后手方不补写
    // （对方已出手，写了也无消费点，留窗口管后续回合）。
    if (!ctx->is_second_mover(owner)) {
        effect_106_special_miss_tick(ctx, EffectArgs(std::vector<int>{owner, 1 - owner}));
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 106, rounds,
                          {owner, 1 - owner}, &effect_106_special_miss_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 128 —— {0}回合内将受到的攻击伤害转化为自身体力（纯光结界/异界之光）
// Args: [owner, target, rounds]。EVENT_TAKE_DAMAGE watcher（actor=攻方归因）。
// 口径：受击落地后恢复**2×实际落伤** = 净增"该次伤害值"（"伤害转化为体力"字面读法：
// 伤害被转化成等量回血，净效果 +amount）。若官方实测为净零（只抵消），把 heal 系数
// 从 2 改 1 即可——文件头记待拍板。已知近似：护盾吃掉的份额不转化（ev.amount=落伤）；
// 致死一击走死亡漏斗，watcher 救不回。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_128_damage_to_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor != 1 - owner || ev.amount <= 0) {
                return;
            }
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(c, owner, ev.amount * 2);   // 见文件头口径注
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 155 —— 恢复全部体力，消除所有能力下降，使自己睡眠{0}回合（打盹/沉眠之梦）
// Args: [owner, target, rounds]。恢复走 heal_amount（吃封回血）；睡眠时长 {0} 定值。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_155_rest_full(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner));
    }
    if (g_core->clear_stat_drops) {
        (void)g_core->clear_stat_drops(ctx, owner);
    }
    if (g_core->apply_anomaly) {
        (void)g_core->apply_anomaly(ctx, owner, static_cast<int>(AbnormalStatusId::Sleep),
                                    rounds, owner);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 165 —— {0}回合内每回合防御和特防等级+{1}（意念汇集/光影冲击）
// Args: [owner, target, rounds, delta]。ROUND_START 窗口 tick 走 stat_change 真原语
// （本体/视图同步、可被消强类效果响应）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_165_def_spdef_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[3];
    if (owner < 0 || owner > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(ctx, owner, /*防御=*/2, delta);
    (void)g_core->stat_change(ctx, owner, /*特防=*/3, delta);
    return EffectResult::kOk;
}

EffectResult effect_165_def_spdef_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 首出手方立即 +1（当回合桶已跑过；窗口只够 N-1 次）——首手 1+N-1 = N、后手 0+N = N，
    // 两个站位都恰好 N 次"每回合+1"。
    if (!ctx->is_second_mover(owner)) {
        effect_165_def_spdef_tick(ctx,
            EffectArgs(std::vector<int>{owner, 1 - owner, rounds, args.int_args[3]}));
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 165, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_165_def_spdef_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 185 —— 若击败{0}的对手则对方下只出战精灵{0}（千秋虔：击败臣服的对手→下只也臣服）
// Args: [owner, target, anomaly]（{0} = 异常状态 id，模板两处 {0} 同值）。
// 击败探针：归因本方击败 + 被击败者当时处于异常 {0} → arm；登场 watcher：arm 后对方
// 下一只登场精灵施加异常 {0}（现代施加，免控可挡）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_185_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (batch_b_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, opp, anomaly)) {
        return EffectResult::kOk;   // 被击败者不处于 {0} 状态 → 不触发
    }
    ctx->plugin_storage[kWatch185Arm + owner] = 1;
    return EffectResult::kOk;
}

EffectResult effect_185_carry_status_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = 1 - owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // ① 击败探针
    {
        Effect e(185, 0, owner, 1,
                 EffectArgs(std::vector<int>{owner, opp, args.int_args[2]}),
                 &effect_185_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OPPONENT_DEFEAT_STATUS,
                                                     owner, /*duration=*/1, ctx->roundCount);
        ce->source_id_ = 185;
        ctx->register_skill_effect(State::BATTLE_OPPONENT_DEFEAT_STATUS, owner, std::move(ce));
    }
    // ② 登场 watcher（幂等注册）
    if (!ctx->plugin_storage.count(kWatch185Watch + owner)) {
        const int anomaly = args.int_args[2];
        ctx->plugin_storage.emplace(kWatch185Watch + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, opp, anomaly](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != opp) {
                    return;
                }
                auto it = c->plugin_storage.find(kWatch185Arm + owner);
                if (it == c->plugin_storage.end()) {
                    return;
                }
                const int* armed = std::any_cast<int>(&it->second);
                if (!armed || *armed == 0) {
                    return;
                }
                c->plugin_storage[kWatch185Arm + owner] = 0;   // "下只"一次
                (void)batch_b_apply_anomaly(c, opp, anomaly, owner, 100);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 198 —— 随机使对手{0}种能力等级-{1}（无常夜/八卦断念斩）
// Args: [owner, target, count, amount]。走 stat_drop（查 ATTACH_BAN/免弱/钳 -6）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_198_random_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || count <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    for (const int slot : batch_b_pick_random_stats(count)) {
        (void)g_core->stat_drop(ctx, opp, slot, amount, owner);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 440 —— {0}回合内对手使用技能消耗的PP值变为{1}倍（万里龙吟/神之禁）
// Args: [owner, target, rounds, multiplier]。
// 内核 PP 消耗点 = AFTER_ACTION（consume_selected_skill_pp 读 ws.skill_pp_cost_multiplier，
// 消耗 = min(pp, multiplier)）；ROUND_START 窗口每回合重写（ws reset 后回默认 1）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_440_pp_cost_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int multiplier = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || multiplier <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.skill_pp_cost_multiplier[opp] = multiplier;
    return EffectResult::kOk;
}

EffectResult effect_440_pp_cost_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 立即写一次：对手本次技能的 PP 消耗在其 AFTER_ACTION（晚于本 SKILL_EFFECT），
    // 首出手方当回合就生效；后手方写了也无消费点（ws 随下回合 reset，无害）。
    effect_440_pp_cost_tick(ctx, EffectArgs(std::vector<int>{owner, 1 - owner, rounds,
                                                             args.int_args[3]}));
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 440, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_440_pp_cost_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 599 —— {0}回合内受到{1}伤害减少{2}%（致虚守静 物理100% / 撼地断岩拳 特殊25%）
// Args: [owner, target, rounds, category, pct]。{1} 类别码：0=物理攻击伤害 / 1=特殊攻击
// 伤害 / 2=全部伤害（撼地断岩拳 [1,1,25]=特殊25%、致虚守静 [2,0,100]=物理100% 实证）。
// 落点：damage_reduce_mul（乘算减伤槽，REDUCE_EXTRA 阶段）——首槽写 pct；类别 0/1 时
// 仅当对方本回合选技属于该类别才写（MOVE_RIGHT 时选技已定、不再变更 → 每回合精确生效）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_599_category_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int category = args.int_args[3];
    const int pct = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (category == 0 || category == 1) {
        const int idx = batch_b_selected_skill_index(ctx, opp);
        if (idx < 0) {
            return EffectResult::kOk;
        }
        const SkillType type =
            ctx->getPet(opp).skills[static_cast<std::size_t>(idx)].type;
        const bool physical = (type == SkillType::Physical);
        if ((category == 0) != physical) {
            return EffectResult::kOk;   // 类别不匹配：本回合不减伤
        }
    }
    int(&slots)[2][4] = ctx->ws.damage_reduce_mul;
    for (int i = 0; i < 4; ++i) {
        if (slots[owner][i] == 0) {
            slots[owner][i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_599_category_reduce(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 首出手方立即查一次（本回合对方攻击的减伤要在其伤害管线前就位）。
    if (!ctx->is_second_mover(owner)) {
        effect_599_category_reduce_tick(ctx,
            EffectArgs(std::vector<int>{owner, 1 - owner, rounds, args.int_args[3],
                                        args.int_args[4]}));
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 599, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3], args.int_args[4]},
                          &effect_599_category_reduce_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 620 —— {0}回合内致命一击率上升{1}/16（斗气护体/威慑镇压）
// Args: [owner, target, rounds, sixteenths]。
// ws.crit_rate_add 是加算百分点槽（rate = base×mod + add；1/16 = 6.25 个百分点）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_620_crit_up_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int sixteenths = args.int_args[3];
    if (owner < 0 || owner > 1 || sixteenths == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.crit_rate_add[owner] += static_cast<float>(sixteenths) * 100.0f / 16.0f;
    return EffectResult::kOk;
}

EffectResult effect_620_crit_up_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 首出手方立即写一次（当回合桶已跑过；本次攻击的暴击掷点在 SKILL_EFFECT 之前，
    // 补写只惠及窗口内的后续攻击——与官方"3回合内"的回合数对齐）。
    if (!ctx->is_second_mover(owner)) {
        effect_620_crit_up_tick(ctx,
            EffectArgs(std::vector<int>{owner, 1 - owner, rounds, args.int_args[3]}));
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 620, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_620_crit_up_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 626 —— 随机使自己{0}项能力+{1}（无晓天/羁绊缔结/幻灭速击）
// Args: [owner, target, count, delta]。stat_change（自身增益不查免弱）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_626_random_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    const int delta = args.int_args[3];
    if (owner < 0 || owner > 1 || count <= 0 || delta == 0) {
        return EffectResult::kOk;
    }
    for (const int slot : batch_b_pick_random_stats(count)) {
        (void)g_core->stat_change(ctx, owner, slot, delta);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 660 —— 消除{0}状态，消除成功恢复{1}%的体力（蛮横重锤/桓侯震天击）
// Args: [owner, target, code, pct]。{0} 类别码（4399 文案实证）：
//   0 = 消除**对手**能力上升状态（蛮横重锤"消除对手能力上升状态，消除成功恢复100%体力"）
//   1 = 解除**自身**能力下降状态（桓侯震天击"解除自身弱化，解除成功恢复所有体力"）
// 其它码保守 no-op（出现新引用再扩表）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_660_clear_and_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    int cleared = 0;
    if (code == 0) {
        cleared = g_core->clear_stat_boosts ? g_core->clear_stat_boosts(ctx, opp) : 0;
    } else if (code == 1) {
        cleared = g_core->clear_stat_drops ? g_core->clear_stat_drops(ctx, owner) : 0;
    } else {
        return EffectResult::kOk;
    }
    if (cleared > 0 && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner) * pct / 100);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 931 —— 吸取对手能力提升状态，吸取成功则对手{0}技能PP值归零（致命无存击/孑身诫）
// Args: [owner, target, slots]。吸取走 transfer_stat_boosts（0 项 = 失败）；
// 归零 = 随机 {0} 个不同技能槽（{0}>=5 → 全部），走 pp_zero_slot（发 PP 事件）。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_931_drain_then_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || slots <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败（无提升/被免消除强化挡）→ 不归零
    }
    batch_b_zero_random_pp_slots(ctx, opp, slots, owner);
    return EffectResult::kOk;
}


// ════════════════════════════════════════════════════════════════════
// 第二批 · 精灵专属效果（A：解锁 151~154 场景的精灵）——2026-09-27
// ════════════════════════════════════════════════════════════════════

// 868 用的"下回合先制-{0}"tick（与 jiyuan 867 tick 同形，本文件自持一份）
EffectResult jiyuan_priority_down_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int down = args.int_args[2];
    if (opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[opp] -= down;
    return EffectResult::kOk;
}

// ── 111 —— 附加额外伤害，自身等级越高，附加的伤害越高（星空火雨；全库 157 技）──
// Args: [owner, target]（0 参）。引擎无精灵等级维度（对局恒满级 100）→ 附加固伤取
// 100×2=200 定值近似（老赛尔号经典口径"等级×2"），记文件头待拍板。
EffectResult effect_111_level_scaled_extra(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->fixed_damage(ctx, opp, /*level 100 × 2=*/200);
    return EffectResult::kOk;
}

// ── 880 —— 消除对手回合类效果，消除成功则令自身下回合所有技能先制+{0}
// （龙腾寰宇 [3]；867 的镜像：清的是回合类、受益是自身）──
// Args: [owner, target, up]。SUCCESS 才给（NO_EFFECTS/IMMUNE 都不算"消除成功"）。
// "下回合" → NextRounds 窗口 + MOVE_RIGHT tick 写 preemptive_level（867 同款落点）。
EffectResult effect_880_priority_up_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    std::fprintf(stderr, "[dbg880] tick R%d owner=%d up=%d\n", ctx->roundCount,
                 args.int_args[0], args.int_args[2]);
    const int owner = args.int_args[0];
    const int up = args.int_args[2];
    if (owner < 0 || owner > 1 || up <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += up;
    return EffectResult::kOk;
}

EffectResult effect_880_break_rounds_priority_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int up = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || up <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // "消除成功"门槛
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 880,
                                /*rounds=*/1, {owner, opp, up}, &effect_880_priority_up_tick);
    return EffectResult::kOk;
}

// ── 884 —— 令对手下次使用的技能无效（千秋龙威；官方注明"包括必中技能"）──
// Args: [owner, target]。次数型 SEAL_ALL（count=1）：引擎门判定在命中判定**之后**
// （skills.cpp ②门），必中只跳过命中率 roll 不跳过盔 → "包括必中"天然满足。
// 可穿盔凭证照常能穿（与攻击免疫同族语义）。
EffectResult effect_884_seal_next_skill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/884,
                             /*attribute=*/true, /*attack=*/true, /*count=*/1,
                             /*duration_rounds=*/0, /*penetrable=*/true, /*source_slot=*/-1,
                             EffectScope::ON_STAGE, /*hit_invalid=*/false,
                             /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1039 —— {0}回合内每回合使用技能恢复自身最大体力的1/{1}，恢复体力时若自身
//    体力低于最大体力的1/{2}则直接回满（精灵之助/自然祝福 [4,3,2]）──
// Args: [owner, target, rounds, denom, low_denom]。AFTER_ACTION tick（used_skill 门，
// effect 927 批 8 同款）；低血判定在**回血前**求值（707 口径）。
EffectResult effect_1039_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    if (owner < 0 || owner > 1 || denom <= 0 || bucket_acting_side(ctx) != owner
        || !used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (low_denom > 0 && ctx->getPet(owner).hp < max_hp_of(ctx, owner) / low_denom) {
        (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner));   // 直接回满（clamp）
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

EffectResult effect_1039_heal_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, 1 - owner, rounds, args.int_args[3],
                                   args.int_args[4]};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, owner, 1039, rounds,
                          payload, &effect_1039_heal_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, owner, 1039, rounds,
                          payload, &effect_1039_heal_tick);
    return EffectResult::kOk;
}

// ── 788 —— 消除对手能力提升，消除成功{0}回合内免疫异常状态（羁绊缔结 [1]）──
// Args: [owner, target, rounds]。窗口型 ANOMALY 票（Modern 层，coverage 全时点）。
EffectResult effect_788_clear_then_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_boosts || g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/788,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/788);
    return EffectResult::kOk;
}

// ── 606 —— 若体力{0}对手，技能威力翻倍（迅捷浮影等；{0}：0=高于 / 1=低于）──
// Args: [owner, target, code]。变威力（写 skill_power_view）。其它码保守 no-op。
EffectResult effect_606_hp_compare_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int code = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int mine = ctx->getPet(owner).hp;
    const int theirs = ctx->getPet(opp).hp;
    bool hit = false;
    if (code == 0) {
        hit = mine > theirs;    // "若体力高于对手"（迅捷浮影/黑兽掠草击实证）
    } else if (code == 1) {
        hit = mine < theirs;    // "若体力低于对手"（雷鸣天荒闪实证）
    } else {
        return EffectResult::kOk;
    }
    if (!hit) {
        return EffectResult::kOk;
    }
    const int view = ctx->ws.skill_power_view[owner];
    if (view > 0) {
        ctx->ws.skill_power_view[owner] = view * 2;   // 翻倍（写=声明变威力）
    }
    return EffectResult::kOk;
}

// ── 1176 —— {0}%概率伤害为{1}倍，未触发则附加{2}点固定伤害（道法自然冰/破敌冰锥击）
// Args: [owner, target, chance, multiplier, fixed]。
// "伤害为{1}倍" → 非通用乘算通道（AMP_EXTRA 槽写 ({1}-1)×100）；未触发 → 附加固伤。
EffectResult effect_1176_chance_multiply(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int multiplier = args.int_args[3];
    const int fixed = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        if (multiplier > 1) {
            int(&slots)[2][4] = ctx->ws.damage_add_extra_mul;
            for (int i = 0; i < 4; ++i) {
                if (slots[owner][i] == 0) {
                    slots[owner][i] = (multiplier - 1) * 100;   // 2倍=+100%、4倍=+300%
                    break;
                }
            }
        }
        return EffectResult::kOk;
    }
    if (fixed > 0 && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, fixed);
    }
    return EffectResult::kOk;
}

// ── 1010 —— 消除对手能力提升状态，消除成功则下{0}回合造成的伤害提高{1}%
// （幻灭速击 [2,100]）──
// Args: [owner, target, rounds, pct]。"下{0}回合" → NextRounds 窗口 + ROUND_START
// tick 写 damage_add_pct（通用增伤加算通道，"造成的伤害提高"无"额外"措辞）。
EffectResult effect_1010_damage_up_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    return EffectResult::kOk;
}

EffectResult effect_1010_clear_then_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_boosts || g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1010, rounds,
                                {owner, opp, rounds, args.int_args[3]},
                                &effect_1010_damage_up_tick);
    return EffectResult::kOk;
}

// ── 868 —— 消除对手能力提升状态，消除成功则令对手下回合所有技能先制-{0}
// （银雾之翼 [2]；867 的姊妹：清的是能力提升）──
// Args: [owner, target, down]。
EffectResult effect_868_break_boosts_priority_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int down = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_boosts || g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 868,
                                /*rounds=*/1, {owner, opp, down},
                                &jiyuan_priority_down_tick);
    return EffectResult::kOk;
}

// ── 864 —— 本回合未击败对手则下{0}回合反弹受到伤害的1/{1}（无上天命剑 [1,2]）──
// Args: [owner, target, rounds, denom]。未击败在 AFTER_ACTION 判（1218/984 同款归位）；
// 反弹 = 受击 watcher（EVENT_TAKE_DAMAGE）按落伤 1/{1} 固定伤害弹回攻方。
// watcher 窗口边界按注册回合近似（"下{0}回合"从下一回合起算 → 先空转一回合再挂）。
EffectResult effect_864_deferred_reflect_arm(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, denom}（由"下{0}回合"窗口在每回合 ROUND_START 调用；
    // 窗口本身保证回合范围，这里不再做回合守卫）
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/false,
        [owner, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor != 1 - owner || ev.amount <= 0) {
                return;
            }
            if (g_core && g_core->fixed_damage) {
                (void)g_core->fixed_damage(c, 1 - owner, ev.amount / denom);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult effect_864_reflect_if_no_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    // 未击败判定挂 AFTER_ACTION（伤害落地后可知）
    const std::vector<int> payload{owner, 1 - owner, rounds, denom};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(864, 0, owner, -1, EffectArgs(payload),
                     [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                         if (!c || !a.int_args || a.int_count < 4) {
                             return EffectResult::kOk;
                         }
                         const int self = a.int_args[0];
                         if (bucket_acting_side(c) != self
                             || batch_b_defeated_foe_slot(c, self) >= 0) {
                             return EffectResult::kOk;   // 击败了 → 不反弹
                         }
                         // "下{0}回合"：NextRounds 窗口，每回合开始挂当回合的受击反弹
                         register_next_rounds_effect(
                             c, State::BATTLE_ROUND_START, self, 864, a.int_args[2],
                             {self, 1 - self, a.int_args[3]},
                             &effect_864_deferred_reflect_arm);
                         return EffectResult::kOk;
                     });
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 864;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 807 —— 附加对手上次造成伤害数值的固定伤害（无上天命剑）──
// Args: [owner, target]。"对手上次造成伤害"：无现成台账 → 本文件维护 ledger（watcher
// 记 EVENT_TAKE_DAMAGE 里 actor=对手 的落伤，plugin_storage 持久）；首次使用前无记录
// → 附加 0（无伤害）。damage 值跨回合保留（"上次"不受回合边界限制）。
EffectResult effect_807_ledger_watch(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp}；一次性动作：把最近一笔对手造成的伤害记入台账
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // 由 807 主处理器注册的 watcher 调用：payload[2] = amount
    if (args.int_count >= 3) {
        ctx->plugin_storage[kLedger807Base + owner] = args.int_args[2];
    }
    return EffectResult::kOk;
}

EffectResult effect_807_fixed_from_foe_last(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // 台账 watcher（幂等注册一场一次）：对手造成伤害 → 记账
    if (!ctx->plugin_storage.count(kWatch807Armed + owner)) {
        ctx->plugin_storage.emplace(kWatch807Armed + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, opp](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != opp || ev.amount <= 0) {
                    return;
                }
                Effect ledger(807, 0, owner, -1,
                              EffectArgs(std::vector<int>{owner, opp, ev.amount}),
                              &effect_807_ledger_watch);
                ledger.logic(c, ledger.args);   // 直调记账（不进桶）
            },
            WatcherScope::TEAM);
    }
    int last = 0;
    if (const auto it = ctx->plugin_storage.find(kLedger807Base + owner);
        it != ctx->plugin_storage.end()) {
        if (const int* v = std::any_cast<int>(&it->second)) {
            last = *v;
        }
    }
    if (last > 0 && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, last);
    }
    return EffectResult::kOk;
}

// ── 861 —— {0}回合内若对手使用属性技能则令对手所有技能降低{1}点PP值（挚金命轮 [4,2]）
// Args: [owner, target, rounds, down]。条件是"对手使用属性技能" → 判定点在对手行动完
// 之后：ROUND_END 桶（execute(-1) 双侧都跑、roundChoice 仍未清）读对手本回合选技。
EffectResult effect_861_pp_down_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->pp_reduce) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int down = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 只认属性技能（实际物化类型，判据统一 2026-10-03）
    }
    (void)g_core->pp_reduce(ctx, opp, down);
    return EffectResult::kOk;
}

EffectResult effect_861_pp_down_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 861, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_861_pp_down_tick);
    return EffectResult::kOk;
}

// ── 756 —— 命中后{0}%令对手{1}（挚金命轮 [70,20=失明]；现代参数化施加模板族）
// Args: [owner, target, chance, anomaly]。
EffectResult effect_756_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || chance <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    (void)batch_b_apply_anomaly(ctx, opp, anomaly, owner, chance);
    return EffectResult::kOk;
}


// ════════════════════════════════════════════════════════════════════
// 第二批 · 精灵专属效果（B：解锁 155~159 场景的精灵）——2026-09-27
// ════════════════════════════════════════════════════════════════════

// ── 438 —— {0}%的概率恢复自身体力的1/{1}（圣念 [10,10]）──
EffectResult effect_438_chance_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        (void)heal_max_hp_fraction(ctx, owner, denom);
    }
    return EffectResult::kOk;
}

// ── 765 —— {0}回合对手无法使自身能力出现提升状态（神之禁 [3]）──
// 引擎无"禁提升"原语（stat_change 不查任何禁令）→ 事件回退近似：EVENT_STAT_CHANGED
// watcher（duration={0}）看到对手**正方向**等级变化立即 stat_change 回退。近似偏差：
// 强化先落地再被撤（中间态可见、事件会各发一次）；等级视图片段的中间结算不受影响。
EffectResult effect_765_boost_ban(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp || ev.level_delta <= 0 || !g_core
                || !g_core->stat_change) {
                return;
            }
            (void)g_core->stat_change(c, opp, ev.stat_index, -ev.level_delta);   // 回退
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1500 —— 1回合做{0}-{1}次攻击，自身处于护盾状态下连击上限为{2}（涤罪 [3,5,8]）──
// 连击次数区间已由内核 combo_arg_indices 从参数装入（combo_min/max）；本处理器只补
// "护盾状态下连击上限"：SKILL_EFFECT 时自身有护盾且已掷连击 > 上限 → 钳到上限。
EffectResult effect_1500_combo_cap(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || cap <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).shield_bank_.total() <= 0) {
        return EffectResult::kOk;   // 无护盾：连击区间照常
    }
    const int cur = ctx->ws.combo_view[owner];
    if (cur > cap) {
        ctx->ws.combo_view[owner] = cap;   // 写=声明变威力（触发重算）
    }
    return EffectResult::kOk;
}

// ── 1528 —— 附加{0}点真实伤害（星辉幻尘 [50]）──
EffectResult effect_1528_true_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_true_damage(ctx, opp, amount, owner);
    return EffectResult::kOk;
}

// ── 1529 —— 消除对手能力提升状态，消除成功则附加{0}点真实伤害（涤罪 [200]）──
EffectResult effect_1529_clear_then_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_boosts || g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->deal_true_damage) {
        (void)g_core->deal_true_damage(ctx, opp, amount, owner);
    }
    return EffectResult::kOk;
}

// ── 1530 —— 遇到天敌时附加{0}点真实伤害（虚灵一闪 [200]）──
EffectResult effect_1530_predator_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (!is_predator(ctx, owner)) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_true_damage(ctx, opp, amount, owner);
    return EffectResult::kOk;
}

// ── 1235 —— {0}回合内每回合{1}%闪避对手攻击，未触发则使对手随机{2}项技能PP归零
// （残星稀落 [3,70,1]）──
// 立即 roll 一次（effect 54 先例：首手当回合桶已跑过）+ ROUND_START 窗口逐回合 roll。
EffectResult effect_1235_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[3];
    const int slots = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        ctx->ws.dodge_rate[owner] = 1.0f;   // 本回合闪避（ws reset 自清）
        return EffectResult::kOk;
    }
    batch_b_zero_random_pp_slots(ctx, opp, slots, owner);
    return EffectResult::kOk;
}

EffectResult effect_1235_dodge_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    effect_1235_dodge_tick(ctx, EffectArgs(std::vector<int>{
        owner, 1 - owner, rounds, args.int_args[3], args.int_args[4]}));
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 1235, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3], args.int_args[4]},
                          &effect_1235_dodge_tick);
    return EffectResult::kOk;
}

// ── 1655 —— {0}回合内每回合结束后恢复自身所有技能{2}点PP值（天之赐 [3,0,2]；{1} 占位）
// Args: [owner, target, rounds, _unused, points]。ROUND_END（execute(-1)）tick。
EffectResult effect_1655_pp_restore_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->pp_restore) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int points = args.int_args[4];
    if (owner < 0 || owner > 1 || points <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->pp_restore(ctx, owner, points, /*only_empty=*/false);
    return EffectResult::kOk;
}

EffectResult effect_1655_pp_restore_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1655, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3], args.int_args[4]},
                          &effect_1655_pp_restore_tick);
    return EffectResult::kOk;
}

// ── 1469 —— {0}回合内自身造成的固定伤害、百分比伤害提升{1}%（宙之哲 [3,30]）──
// 粉伤管线 AMP 阶段（来源方桶）。管线注册条目无回合生命周期 → fn 内置截止回合
// （registration round + {0}），过期后自禁用（切换/断回合时由 valid_id 清理）。
EffectResult effect_1469_pink_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || pct == 0) {
        return EffectResult::kOk;
    }
    const int end_round = ctx->roundCount + rounds - 1;
    ctx->pink_damage_pipeline_.register_effect(
        PinkDamagePhase::AMP, owner,
        [owner, pct, end_round](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || c->resolvedPink.actor != owner) {
                return;
            }
            if (c->roundCount > end_round || c->resolvedPink.final <= 0) {
                return;
            }
            c->resolvedPink.final = c->resolvedPink.final * (100 + pct) / 100;
        },
        ctx->round_effect_valid_id[owner]);
    return EffectResult::kOk;
}

// ── 565 —— 先出手时恢复1/{0}体力（群星救赎 [4]）──
EffectResult effect_565_first_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0 || !batch_b_is_first_side(ctx, owner)) {
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

// ── 148 —— 后出手时，{1}%改变对方{0}等级{2}（群星救赎 [5,100,-3]=命中-3）──
EffectResult effect_148_second_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int stat = args.int_args[2];
    const int chance = args.int_args[3];
    const int delta = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || stat < 0 || stat > 5
        || batch_b_is_first_side(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance) && delta != 0) {
        (void)g_core->stat_drop(ctx, opp, stat, std::abs(delta), owner);
    }
    return EffectResult::kOk;
}

// ── 2145 —— 解除并附加给对手自身所处的异常状态，解除成功则对手{0}回合内技能
//    命中效果失效（律理虚浮 [2]）──
// 属性技能半边：seal_skill(hit_invalid=true) 回合型原生支持；攻击技能半边无"回合型
// 命中失效"原语（③层只按次）→ ROUND_START 窗口逐回合 re-grant count=1 近似（覆盖键
// 刷新不叠加），记文件头。
EffectResult effect_2145_hit_invalid_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->hit_effect_invalid) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    if (opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    g_core->hit_effect_invalid(ctx, opp, HitInvalidMode::kEffectsOnly, /*count=*/1,
                               /*is_attribute_skill=*/false, /*source_id=*/2145);
    return EffectResult::kOk;
}

EffectResult effect_2145_transfer_anomalies(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 先收集自身生效中的异常（0..kOfficialAbnormalStatusMaxId），再解除、再逐个附加
    std::vector<int> mine;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly && g_core->has_active_anomaly(ctx, owner, id)) {
            mine.push_back(id);
        }
    }
    if (mine.empty() || !g_core->dispel_active_anomalies) {
        return EffectResult::kOk;
    }
    g_core->dispel_active_anomalies(ctx, owner);
    bool landed = false;
    if (g_core->apply_anomaly) {
        for (const int id : mine) {
            if (batch_b_apply_anomaly(ctx, opp, id, owner, 100)) {
                landed = true;
            }
        }
    }
    if (!landed) {
        return EffectResult::kOk;
    }
    // 属性技能：回合型命中失效；攻击技能：窗口逐回合 re-grant
    if (g_core->seal_skill) {
        (void)g_core->seal_skill(ctx, owner, opp, /*effect_id=*/2145,
                                 /*attribute=*/true, /*attack=*/false, /*count=*/0,
                                 /*duration_rounds=*/rounds, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/true, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 2145, rounds,
                          {owner, opp, rounds}, &effect_2145_hit_invalid_tick);
    return EffectResult::kOk;
}

// ── 761 —— {0}回合内对手恢复体力时恢复效果减少{1}%（天宙告缚 [3,30]）──
// 落点 = ctx->heal_mod_pct[opp]（heal 原语同通道相加）。capture-restore：授予时记原值，
// ROUND_START 窗口（{0}+1 格）每 tick 按回合重设/到期还原（上下文级字段、换宠自清）。
EffectResult effect_761_heal_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    // payload = {owner, opp, rounds, pct, orig, arm_round}
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    const int orig = args.int_args[4];
    const int arm = args.int_args[5];
    if (opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ctx->heal_mod_pct[opp] =
        (ctx->roundCount <= arm + rounds - 1) ? (orig - pct) : orig;
    return EffectResult::kOk;
}

EffectResult effect_761_heal_reduce(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int orig = ctx->heal_mod_pct[opp];
    ctx->heal_mod_pct[opp] = orig - pct;   // 授予即生效（首手当回合）
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 761, rounds + 1,
                          {owner, opp, rounds, pct, orig, ctx->roundCount},
                          &effect_761_heal_reduce_tick);
    return EffectResult::kOk;
}

// ── 561 —— 先出手时当回合对手使用技能后若自身体力为0，则令自身体力等于最大体力
// （归星化魄；0 参）──
// 死亡拦截器（残留免死类 sees_hp_consume=false）：本方死亡且本方先手 → reset_hp(100)
// 拦下。按"当回合"口径：SKILL_EFFECT 注册 + OLD_ROUND_END_1 一次性注销。
EffectResult effect_561_dodge_death(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!g_core->register_death_interceptor || !g_core->reset_hp) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kIntercept561Arm + owner] = ctx->roundCount;   // 本回合 armed
    // 幂等重注册（同 source 覆盖）
    g_core->register_death_interceptor(
        ctx, DeathInterceptor{/*source_effect_id=*/561, /*owner=*/owner,
                              /*sees_hp_consume=*/false,
                              [owner](BattleContext* c, int side, int slot, int actor,
                                      DefeatCause cause) -> bool {
                                  (void)actor;
                                  (void)cause;
                                  if (side != owner || !batch_b_is_first_side(c, owner)) {
                                      return false;
                                  }
                                  // 仅"本回合 armed"（SKILL_EFFECT 注册、回合末注销）
                                  const auto it = c->plugin_storage.find(kIntercept561Arm + owner);
                                  if (it == c->plugin_storage.end()) {
                                      return false;
                                  }
                                  const int* arm = std::any_cast<int>(&it->second);
                                  if (!arm || *arm != c->roundCount) {
                                      return false;
                                  }
                                  if (!g_core->reset_hp) {
                                      return false;
                                  }
                                  (void)g_core->reset_hp(c, owner, 100);
                                  return true;
                              }});
    // 本回合结束注销（一次性动作）
    Effect off(561, 0, owner, -1, EffectArgs(std::vector<int>{owner}),
               [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                   if (c && g_core && g_core->remove_death_interceptors && a.int_args
                       && a.int_count >= 1) {
                       g_core->remove_death_interceptors(c, 561);
                   }
                   return EffectResult::kOk;
               });
    auto ce = std::make_unique<ContinuousEffect>(off, State::BATTLE_OLD_ROUND_END_1, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 561;
    ctx->register_skill_effect(State::BATTLE_OLD_ROUND_END_1, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 727 —— 后出手时将自身能力等级返回至上一回合结束时（归星化魄；0 参）──
// 快照机：SKILL_EFFECT 首次使用时挂 ROUND_END 常驻小窗（ON_STAGE，随下场作废）每回合
// 记 (round, 六槽等级)；后手使用时若快照恰为上一回合 → 恢复（本体+视图直写）。
EffectResult effect_727_snapshot_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kSnap727Round + owner] = ctx->roundCount;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx->plugin_storage[kSnap727Base + owner * 10 + i] = ctx->ability_levels[owner][i];
    }
    return EffectResult::kOk;
}

EffectResult effect_727_restore_levels(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 快照窗口（每回合 ROUND_END 记账）首次使用时挂上。直构 ContinuousEffect：
    // registered_round = 本回合（绕开 register_round_effect 的后手顺延——记账必须
    // 从本回合结束开始，否则首次使用的当回合结束没有快照）。
    if (!ctx->getPet(owner).on_stage_storage.count(kSnap727Armed + owner)) {
        ctx->getPet(owner).on_stage_storage[kSnap727Armed + owner] = 1;
        Effect snap(727, 0, owner, 999, EffectArgs(std::vector<int>{owner}),
                    &effect_727_snapshot_tick);
        auto ce = std::make_unique<ContinuousEffect>(snap, State::BATTLE_ROUND_END, owner,
                                                     /*duration=*/999, ctx->roundCount);
        ce->source_id_ = 727;
        ctx->register_skill_effect(State::BATTLE_ROUND_END, owner, std::move(ce));
    }
    // 后手 → 恢复到"上一回合结束时"快照（快照回合 == 本回合-1 才有效）
    if (!batch_b_is_first_side(ctx, owner)) {
        const auto rit = ctx->plugin_storage.find(kSnap727Round + owner);
        if (rit != ctx->plugin_storage.end()) {
            if (const int* snap_round = std::any_cast<int>(&rit->second)) {
                if (*snap_round == ctx->roundCount - 1) {
                    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                        const auto sit = ctx->plugin_storage.find(kSnap727Base + owner * 10 + i);
                        if (sit == ctx->plugin_storage.end()) {
                            continue;
                        }
                        if (const int* v = std::any_cast<int>(&sit->second)) {
                            // 快照恢复钳 ±6（官方等级域；防脏存储越界，night fuzz #9 家族）
                            const int lv = std::clamp(*v, -6, 6);
                            ctx->ability_levels[owner][i] = lv;
                            ctx->ws.view_levels[owner][i] = lv;
                        }
                    }
                }
            }
        }
    }
    return EffectResult::kOk;
}

// ── 587 —— {0}回合内若被对手击败则对手损失{1}点体力（谜空净魂 [3,500]）──
// "造成致命伤害时对手剩余1点体力"半句主语不明（4399 原文如此），保守只实现前半，
// 见文件头待拍板。击败判定：EVENT_DEATH（target=本方、actor=对手）→ 对手扣真实伤害。
EffectResult effect_587_defeat_punish(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || amount <= 0
        || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, amount](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor != 1 - owner) {
                return;
            }
            if (g_core && g_core->deal_true_damage) {
                (void)g_core->deal_true_damage(c, 1 - owner, amount, 1 - owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 524 —— {0}回合内若被对手击败则对手疲惫{1}回合（谜空净魂 [3,3]）──
EffectResult effect_524_defeat_fatigue(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int fatigue_rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || fatigue_rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, fatigue_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor != 1 - owner) {
                return;
            }
            if (g_core && g_core->apply_anomaly) {
                (void)g_core->apply_anomaly(c, 1 - owner,
                                            static_cast<int>(AbnormalStatusId::Fatigue),
                                            fatigue_rounds, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1140 —— {0}回合内对手使用攻击技能则{1}（无尽曙光 [3,7=疲惫]；{1}=异常码）──
// 判定点 ROUND_END（对手行动完；861 同款落点）。
EffectResult effect_1140_attack_punish_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (!executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 只认攻击技能（实际物化类型，判据统一 2026-10-03）
    }
    (void)batch_b_apply_anomaly(ctx, opp, anomaly, owner, 100);
    return EffectResult::kOk;
}

EffectResult effect_1140_attack_punish(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1140, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_1140_attack_punish_tick);
    return EffectResult::kOk;
}

// ── 877 —— 先出手时全属性+{0}（卸甲归钺 [1]）──
EffectResult effect_877_first_all_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    if (owner < 0 || owner > 1 || delta == 0 || !batch_b_is_first_side(ctx, owner)) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, delta);
    return EffectResult::kOk;
}

// ── 1207 —— 双倍吸取对手能力提升状态，吸取成功则使对手下{0}回合先制-{1}
// （涤罪之锋 [1,3]）──
// "双倍"口径：吸取（整体搬运）成功后，自身再把获得的每档等级**再+一次**（等效双倍收益）。
EffectResult effect_1207_double_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    // 快照自身等级 → 吸取 → 差值再补一次（双倍收益）
    int before[BattleContext::kAbilityLevelSlotCount] = {0};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        before[i] = ctx->ability_levels[owner][i];
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败
    }
    if (g_core->stat_change) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int gained = ctx->ability_levels[owner][i] - before[i];
            if (gained > 0) {
                (void)g_core->stat_change(ctx, owner, i, gained);
            }
        }
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1207,
                                /*rounds=*/1, {owner, opp, down},
                                &jiyuan_priority_down_tick);
    return EffectResult::kOk;
}

// ── 1171 —— 未击败对手则使自身下{0}回合受到的伤害减少{1}点（圣龙甩尾 [1,300]）──
// AFTER_ACTION 探针（未击败可知）→ NextRounds 窗口：ROUND_START 写 damage_reduce_flat 首空槽。
EffectResult effect_1171_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    int(&slots)[2][4] = ctx->ws.damage_reduce_flat;
    for (int i = 0; i < 4; ++i) {
        if (slots[owner][i] == 0) {
            slots[owner][i] = amount;
            break;
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_1171_reduce_if_no_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, 1 - owner, rounds, amount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1171, 0, owner, -1, EffectArgs(payload),
                     [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                         if (!c || !a.int_args || a.int_count < 4) {
                             return EffectResult::kOk;
                         }
                         const int self = a.int_args[0];
                         if (bucket_acting_side(c) != self
                             || batch_b_defeated_foe_slot(c, self) >= 0) {
                             return EffectResult::kOk;
                         }
                         register_next_rounds_effect(
                             c, State::BATTLE_ROUND_START, self, 1171, a.int_args[2],
                             {self, 1 - self, a.int_args[3]}, &effect_1171_reduce_tick);
                         return EffectResult::kOk;
                     });
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1171;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 126 —— {0}回合内每回合自身攻击和速度提高{1}个等级（斗志高昂 [3,1]）──
EffectResult effect_126_atk_speed_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[3];
    if (owner < 0 || owner > 1 || delta == 0) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(ctx, owner, /*攻击=*/0, delta);
    (void)g_core->stat_change(ctx, owner, /*速度=*/4, delta);
    return EffectResult::kOk;
}

EffectResult effect_126_atk_speed_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {   // 首手立即 +1（165 同款补首跳）
        effect_126_atk_speed_tick(ctx, EffectArgs(std::vector<int>{
            owner, 1 - owner, rounds, args.int_args[3]}));
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 126, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_126_atk_speed_tick);
    return EffectResult::kOk;
}

// ── 187 —— {0}回合内若对手使用属性技能，则自身恢复1/{1}最大体力值（磐石之力 [3,3]）──
EffectResult effect_187_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 只认属性技能（实际物化类型，判据统一 2026-10-03）
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

EffectResult effect_187_heal_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 187, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_187_heal_tick);
    return EffectResult::kOk;
}

// ── 502 —— {0}回合内所有直接攻击都将降低对手1/{1}的HP（不动神威 [3,4]）──
// ROUND_END tick：本方用过攻击技且造成过红伤 → 对手扣 max/{1} 固定伤害。
EffectResult effect_502_direct_cut_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (!used_skill_this_round(ctx, owner) || ctx->ws.raw_attack_damage[owner] <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, max_hp_of(ctx, opp) / denom);
    }
    return EffectResult::kOk;
}

EffectResult effect_502_direct_cut_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 502, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_502_direct_cut_tick);
    return EffectResult::kOk;
}

// ── 1154 —— {0}回合内使用技能附加{1}点固定伤害，若自身体力低于对手则翻倍
// （憾地之力/无上荣光 [3,200]；side_effect.arg_count=2 → {2} 条件码切片不可得，
//   两个已知引用文案都是"体力低于对手翻倍" → 硬编码该条件，见文件头）──
EffectResult effect_1154_extra_fixed_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    int dmg = amount;
    if (ctx->getPet(owner).hp < ctx->getPet(opp).hp) {
        dmg *= 2;   // "若自身体力低于对手则造成的固定伤害翻倍"（{2}=0 条件码）
    }
    if (g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, dmg);
    }
    return EffectResult::kOk;
}

EffectResult effect_1154_extra_fixed_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1154, rounds,
                          {owner, 1 - owner, rounds, args.int_args[3]},
                          &effect_1154_extra_fixed_tick);
    return EffectResult::kOk;
}

// ── 1351 —— 获得{0}点护罩，护罩消失时使对手下{1}次攻击技能无效（斗志激昂 [350,1]）──
// 护罩消失检测：粉伤管线耗尽护罩**不发** EVENT_SHIELD_BROKEN（那只响红伤破盾）→
// 监听 EVENT_TAKE_PINK_DAMAGE（每段粉伤后自查那笔余额，quantity_of 归零即判消失）。
EffectResult effect_1351_hood_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    const int times = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0 || times <= 0) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).hood_bank_.add(/*priority=*/0, amount, /*source_id=*/1351,
                                      /*duration=*/0);
    constexpr int kWatch = kKeyBase + 13510;
    if (ctx->plugin_storage.count(kWatch + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(kWatch + owner, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, times](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner) {
                return;
            }
            if (c->getPet(owner).hood_bank_.quantity_of(1351) > 0) {
                return;   // 我那笔护罩还在
            }
            if (g_core && g_core->seal_skill) {
                const int grant = g_core->seal_skill(c, owner, opp, /*effect_id=*/1351,
                                                     /*attribute=*/false, /*attack=*/true,
                                                     /*count=*/times,
                                                     /*duration_rounds=*/0,
                                                     /*penetrable=*/true, /*source_slot=*/-1,
                                                     EffectScope::ON_STAGE,
                                                     /*hit_invalid=*/false,
                                                     /*chance_pct=*/100,
                                                     /*consumed_when_pierced=*/false);
                (void)grant;
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1138 —— 消除对手回合类效果，消除成功则{0}%令对手{1}，未触发则{2}%令对手{3}
// （尖角突袭 [50,6,100,7]）──
EffectResult effect_1138_break_branch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) == BreakResult::SUCCESS) {
        (void)batch_b_apply_anomaly(ctx, opp, args.int_args[3], owner, args.int_args[2]);
    } else {
        (void)batch_b_apply_anomaly(ctx, opp, args.int_args[5], owner, args.int_args[4]);
    }
    return EffectResult::kOk;
}

// ── 871 —— 下{0}回合，每回合攻击技能造成的伤害不少于{1}（流沙封魂阵 [2,280]）──
EffectResult effect_871_floor_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int floor = args.int_args[2];
    if (owner < 0 || owner > 1 || floor <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_floor[owner] = floor;   // FLOOR 阶段抬保底（ws 每回合 reset → 窗口重写）
    return EffectResult::kOk;
}

EffectResult effect_871_floor_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 871, rounds,
                                {owner, 1 - owner, args.int_args[3]},
                                &effect_871_floor_tick);
    return EffectResult::kOk;
}

// ── 540 —— 若后出手则下{0}回合攻击必定致命一击（圣武天战决 [2]/流沙封魂阵 [1]）──
// SKILL_EFFECT 时先手权已定：后手 → 立即写 must_crit（本次攻击暴击掷点在 SKILL_EFFECT
// 之前的 ON_SKILL_HIT 已掷——补写只惠及后续攻击，见 skills.cpp 1532 注）+ NextRounds
// 窗口覆盖后续 {0} 回合。当前攻击的必暴需要"先手权已知且掷点未发生"的时点，执行期
// 处理器到不了 → 记文件头待拍板。
EffectResult effect_540_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner >= 0 && owner <= 1) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

EffectResult effect_540_second_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0 || batch_b_is_first_side(ctx, owner)) {
        return EffectResult::kOk;   // 仅后手触发
    }
    ctx->ws.must_crit[owner] = true;   // 尽力覆盖本次攻击（掷点已过则惠及后续）
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 540, rounds,
                                {owner, 1 - owner, rounds}, &effect_540_must_crit_tick);
    return EffectResult::kOk;
}

// ── 723 —— 后出手时令对手下{0}回合先制-{1}（流沙封魂阵 [1,3]）──
EffectResult effect_723_second_priority_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0 || down <= 0) {
        return EffectResult::kOk;
    }
    if (batch_b_is_first_side(ctx, owner)) {
        return EffectResult::kOk;   // 仅后手
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 723, rounds,
                                {owner, opp, down}, &jiyuan_priority_down_tick);
    return EffectResult::kOk;
}

// ── 834 —— {0}回合内自身能力提升状态被消除或吸取时，{1}回合内对手属性技能无效
// （气定神闲 [4,2]）──
// 提升被清除的检测 = EVENT_STAT_CHANGED（等级由正 → ≤0 的下降变化；吸取消强同走
// write_ability_levels）。近似：不区分"被消除/被吸取"与"被反转"，凡正→非正的下降都算。
EffectResult effect_834_boost_lost_watch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int seal_rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0 || seal_rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, owner, ctx->roundCount, rounds, /*once=*/false,
        [owner, opp, seal_rounds](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.level_delta >= 0) {
                return;
            }
            const int before = ev.level_after - ev.level_delta;
            if (before <= 0 || ev.level_after > 0) {
                return;   // 只认"正等级 → 非正"（提升被消除/吸取）
            }
            if (g_core && g_core->seal_skill) {
                (void)g_core->seal_skill(c, owner, opp, /*effect_id=*/834,
                                         /*attribute=*/true, /*attack=*/false, /*count=*/0,
                                         /*duration_rounds=*/seal_rounds,
                                         /*penetrable=*/true, /*source_slot=*/-1,
                                         EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                         /*chance_pct=*/100,
                                         /*consumed_when_pierced=*/false);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1770 —— 开启战魂附体：免疫对手下1次攻击技能造成的伤害，若对手使用的攻击技能
//    PP值为满则自身额外免疫下1次受到的固定伤害和百分比伤害（战魂重临；0 参）──
// 半支①：DAMAGE 次数票。半支②：EVENT_HIT watcher（攻方命中时 PP 尚未消耗——PP 扣点
// 在 AFTER_ACTION）→ 对手所选用技 pp==maxPP → 授 PINK_DAMAGE 次数票。
EffectResult effect_1770_war_soul(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/1770,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/1, /*source_effect_id=*/1770);
    constexpr int kWatch = kKeyBase + 17700;
    if (!ctx->plugin_storage.count(kWatch + owner)) {
        ctx->plugin_storage.emplace(kWatch + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount, /*duration_rounds=*/9,
            /*once=*/false,
            [owner, opp](BattleContext* c, const BattleEvent& ev) {
                if (!c || ev.actor != opp || ev.target != owner) {
                    return;
                }
                const int idx = batch_b_selected_skill_index(c, opp);
                if (idx < 0) {
                    return;
                }
                const Skills& used = c->getPet(opp).skills[static_cast<std::size_t>(idx)];
                if (used.pp != used.maxPP) {
                    return;   // "PP 值为满"条件不满足
                }
                c->grant_immunity(owner, ImmunityType::PINK_DAMAGE, coverage_all(),
                                  /*anomaly_mask=*/0, /*duration_rounds=*/0,
                                  /*source_id=*/1770, /*soul_immunity=*/false,
                                  EffectScope::ON_STAGE, /*counts=*/1,
                                  /*source_effect_id=*/1770);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 1025 —— 吸取对手能力提升状态，吸取成功则恢复自身最大体力的1/{0}（圣裂冲 [3]）──
EffectResult effect_1025_drain_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, opp, owner) <= 0) {
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

// ── 983 —— 未击败对手则自身全属性+{0}（先驱战拳 [1]）──
EffectResult effect_983_all_up_if_no_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    if (owner < 0 || owner > 1 || delta == 0 || bucket_acting_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (batch_b_defeated_foe_slot(ctx, owner) >= 0) {
        return EffectResult::kOk;   // 击败了 → 不强化
    }
    all_stats_up(ctx, owner, delta);
    return EffectResult::kOk;
}

EffectResult effect_983_register_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{owner, 1 - owner, args.int_args[2]};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(983, 0, owner, -1, EffectArgs(payload), &effect_983_all_up_if_no_kill);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 983;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1099 —— 消除对手能力提升状态，消除成功则自身免疫下{0}次受到的异常状态
// （再战天荒 [1]）──
EffectResult effect_1099_clear_then_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_boosts || g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/1099,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/1099);
    return EffectResult::kOk;
}

// ── 799 —— 恢复自身最大体力的1/{0}并给对手造成等量百分比伤害，自身体力低于1/{1}
//    时效果翻倍（再战天荒 [4,2]）──
EffectResult effect_799_heal_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    int heal = max_hp_of(ctx, owner) / denom;
    if (low_denom > 0 && ctx->getPet(owner).hp < max_hp_of(ctx, owner) / low_denom) {
        heal *= 2;   // "效果翻倍"
    }
    if (g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    const int healed = ctx->ws.last_heal_amount[owner];
    if (healed > 0 && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1568 —— 无视护盾效果（星澜/几曾识干戈；0 参）──
EffectResult effect_1568_ignore_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.ignore_shield[owner] = true;   // 本次攻击无视护盾/护罩响应
    return EffectResult::kOk;
}

// ── 1113 —— 技能命中后消除自身能力提升状态（滞道销心；0 参）──
EffectResult effect_1113_clear_self_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->clear_stat_boosts(ctx, owner);
    return EffectResult::kOk;
}

// ── 2144 —— 双方下{0}回合无法主动切换精灵（昊法同愚 [2]）──
EffectResult effect_2144_switch_lock_both(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    for (const int target : {0, 1}) {
        ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/2144, target,
                                            rounds, ctx->roundCount);
    }
    return EffectResult::kOk;
}

// ── 1603 —— {0}%降低对手所有PP值{1}点（无魂归 [100,2]）──
EffectResult effect_1603_chance_pp_down(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->pp_reduce) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        (void)g_core->pp_reduce(ctx, opp, amount);
    }
    return EffectResult::kOk;
}

// ── 1463 —— 先出手时本回合若自身死亡则立刻恢复自身所有体力并解除自身的异常状态
//    和能力下降状态（无圣显；0 参）──
// 死亡拦截器（残留免死类）：本方死亡且本方先手 → 回满 + 解异常 + 清下降。"本回合"
// 口径：SKILL_EFFECT 注册、OLD_ROUND_END_1 注销（561 同款）。
EffectResult effect_1463_dodge_death(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->register_death_interceptor) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kIntercept1463Arm + owner] = ctx->roundCount;
    g_core->register_death_interceptor(
        ctx, DeathInterceptor{/*source_effect_id=*/1463, /*owner=*/owner,
                              /*sees_hp_consume=*/false,
                              [owner](BattleContext* c, int side, int slot, int actor,
                                      DefeatCause cause) -> bool {
                                  (void)slot;
                                  (void)actor;
                                  (void)cause;
                                  if (side != owner || !batch_b_is_first_side(c, owner)) {
                                      return false;
                                  }
                                  const auto it = c->plugin_storage.find(kIntercept1463Arm + owner);
                                  if (it == c->plugin_storage.end()) {
                                      return false;
                                  }
                                  const int* arm = std::any_cast<int>(&it->second);
                                  if (!arm || *arm != c->roundCount) {
                                      return false;
                                  }
                                  if (!g_core->heal_amount || !g_core->dispel_active_anomalies
                                      || !g_core->clear_stat_drops) {
                                      return false;
                                  }
                                  (void)g_core->heal_amount(c, owner, max_hp_of(c, owner));
                                  (void)g_core->dispel_active_anomalies(c, owner);
                                  (void)g_core->clear_stat_drops(c, owner);
                                  return c->getPet(owner).hp > 0;
                              }});
    Effect off(1463, 0, owner, -1, EffectArgs(std::vector<int>{owner}),
               [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                   if (c && g_core && g_core->remove_death_interceptors && a.int_args
                       && a.int_count >= 1) {
                       g_core->remove_death_interceptors(c, 1463);
                   }
                   return EffectResult::kOk;
               });
    auto ce = std::make_unique<ContinuousEffect>(off, State::BATTLE_OLD_ROUND_END_1, owner,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 1463;
    ctx->register_skill_effect(State::BATTLE_OLD_ROUND_END_1, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 2163 —— 消除对手回合类效果，若对手回合类效果无法被消除则对手{0}技能PP值归0
// （孑身诫 [5]；{0}>=5 → 全部槽）──
EffectResult effect_2163_break_or_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || slots <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) == BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    batch_b_zero_random_pp_slots(ctx, opp, slots, owner);
    return EffectResult::kOk;
}

// ── 1711 —— 解除自身能力下降状态，解除成功则{0}%令对手{1}（千秋虔 [100,31=臣服]）──
EffectResult effect_1711_clear_drops_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (!g_core->clear_stat_drops || g_core->clear_stat_drops(ctx, owner) <= 0) {
        return EffectResult::kOk;
    }
    (void)batch_b_apply_anomaly(ctx, opp, anomaly, owner, chance);
    return EffectResult::kOk;
}

// ── 1868 —— 若对手当回合切换精灵则{0}%令对手{1}，未触发则{2}%令对手{3}
// （千秋虔 [100,31,100,28=束缚]）──
EffectResult effect_1868_switch_branch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[opp][0] == 0 /*CHOOSE_PET*/) {
        (void)batch_b_apply_anomaly(ctx, opp, args.int_args[3], owner, args.int_args[2]);
    } else {
        (void)batch_b_apply_anomaly(ctx, opp, args.int_args[5], owner, args.int_args[4]);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（共享批）─────────────────────────────────────────────────
SKILL_EFFECT(1, effect_1_drain_half);
SKILL_EFFECT(38, effect_38_reduce_max_hp);
SKILL_EFFECT(55, effect_55_reverse_element);
SKILL_EFFECT(63, effect_63_reflect_drops);
SKILL_EFFECT(64, effect_64_status_amp);
SKILL_EFFECT(66, effect_66_heal_on_kill);
SKILL_EFFECT(67, effect_67_cut_next_on_kill);
SKILL_EFFECT(89, effect_89_lifesteal_window);
SKILL_EFFECT(94, effect_94_petrify);
SKILL_EFFECT(102, effect_102_paralyze_power);
SKILL_EFFECT(106, effect_106_special_miss);
SKILL_EFFECT(128, effect_128_damage_to_hp);
SKILL_EFFECT(155, effect_155_rest_full);
SKILL_EFFECT(165, effect_165_def_spdef_window);
SKILL_EFFECT(185, effect_185_carry_status_on_kill);
SKILL_EFFECT(198, effect_198_random_drops);
SKILL_EFFECT(440, effect_440_pp_cost_window);
SKILL_EFFECT(599, effect_599_category_reduce);
SKILL_EFFECT(620, effect_620_crit_up_window);
SKILL_EFFECT(626, effect_626_random_boosts);
SKILL_EFFECT(660, effect_660_clear_and_heal);
SKILL_EFFECT(931, effect_931_drain_then_pp_zero);

// ── 效果注册（第二批 A：151~154 精灵解锁）───────────────────────────
SKILL_EFFECT(111, effect_111_level_scaled_extra);
SKILL_EFFECT(880, effect_880_break_rounds_priority_up);
SKILL_EFFECT(884, effect_884_seal_next_skill);
SKILL_EFFECT(1039, effect_1039_heal_window);
SKILL_EFFECT(788, effect_788_clear_then_anomaly_immune);
SKILL_EFFECT(606, effect_606_hp_compare_power);
SKILL_EFFECT(1176, effect_1176_chance_multiply);
SKILL_EFFECT(1010, effect_1010_clear_then_damage_up);
// SKILL_EFFECT(868, effect_868_break_boosts_priority_down);  // 去重：两版语义等价，保留 batch_a 版（先落地）
SKILL_EFFECT(864, effect_864_reflect_if_no_kill);
SKILL_EFFECT(807, effect_807_fixed_from_foe_last);
SKILL_EFFECT(861, effect_861_pp_down_window);
// SKILL_EFFECT(756, effect_756_chance_anomaly);  // 去重：保留 batch_a 版（apply_anomaly_checked 申报原语，亮节闸门管辖——本文件 batch_b_apply_anomaly 未走闸门）

// ── 效果注册（第二批 B：155~159 精灵解锁）───────────────────────────
SKILL_EFFECT(438, effect_438_chance_heal);
SKILL_EFFECT(765, effect_765_boost_ban);
SKILL_EFFECT(1500, effect_1500_combo_cap);
SKILL_EFFECT(1528, effect_1528_true_damage);
SKILL_EFFECT(1529, effect_1529_clear_then_true);
SKILL_EFFECT(1530, effect_1530_predator_true);
SKILL_EFFECT(1235, effect_1235_dodge_window);
// SKILL_EFFECT(1655, effect_1655_pp_restore_window);  // 去重：保留 batch_a 版（族群血源官方文本"降低对手所有技能{1}点PP并恢复自身{2}点"——effect_info 模板截断，本版按模板字面漏了对手 PP 削减半边）
SKILL_EFFECT(1469, effect_1469_pink_amp);
SKILL_EFFECT(565, effect_565_first_heal);
SKILL_EFFECT(148, effect_148_second_drop);
SKILL_EFFECT(2145, effect_2145_transfer_anomalies);
SKILL_EFFECT(761, effect_761_heal_reduce);
// SKILL_EFFECT(561, effect_561_dodge_death);  // 去重：保留 batch_a 版（官方"自身体力等于最大体力"=回满+状态语义 sees_hp_consume；本版 sees_hp_consume=false 漏"消耗体力"路径）
SKILL_EFFECT(727, effect_727_restore_levels);
SKILL_EFFECT(587, effect_587_defeat_punish);
SKILL_EFFECT(524, effect_524_defeat_fatigue);
SKILL_EFFECT(1140, effect_1140_attack_punish);
SKILL_EFFECT(877, effect_877_first_all_up);
SKILL_EFFECT(1207, effect_1207_double_drain);
SKILL_EFFECT(1171, effect_1171_reduce_if_no_kill);
SKILL_EFFECT(126, effect_126_atk_speed_window);
SKILL_EFFECT(187, effect_187_heal_window);
SKILL_EFFECT(502, effect_502_direct_cut_window);
SKILL_EFFECT(1154, effect_1154_extra_fixed_window);
SKILL_EFFECT(1351, effect_1351_hood_seal);
SKILL_EFFECT(1138, effect_1138_break_branch);
SKILL_EFFECT(871, effect_871_floor_window);
SKILL_EFFECT(540, effect_540_second_must_crit);
SKILL_EFFECT(723, effect_723_second_priority_down);
SKILL_EFFECT(834, effect_834_boost_lost_watch);
SKILL_EFFECT(1770, effect_1770_war_soul);
SKILL_EFFECT(1025, effect_1025_drain_heal);
SKILL_EFFECT(983, effect_983_register_probe);
SKILL_EFFECT(1099, effect_1099_clear_then_immune);
SKILL_EFFECT(799, effect_799_heal_percent);
SKILL_EFFECT(1568, effect_1568_ignore_shield);
SKILL_EFFECT(1113, effect_1113_clear_self_boosts);
SKILL_EFFECT(2144, effect_2144_switch_lock_both);
SKILL_EFFECT(1603, effect_1603_chance_pp_down);
SKILL_EFFECT(1463, effect_1463_dodge_death);
SKILL_EFFECT(2163, effect_2163_break_or_pp_zero);
SKILL_EFFECT(1711, effect_1711_clear_drops_anomaly);
SKILL_EFFECT(1868, effect_1868_switch_branch);
