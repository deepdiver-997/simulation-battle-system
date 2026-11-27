#include <utils/sbs_rand.h>
// 选择期先制/微弱奖励(610/2000/499/785/2030/2490) / 封属与技能无效(52/695/700/781/936/
// 1217/1222/1236/1237/2006/2126/2501) / 消强补偿(999900) / PP 全恢复(87)。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

constexpr int kActionSelectSkill = 1;  // roundChoice[side][0] 的 SELECT_SKILL（battleFsm ActionType）

namespace {

EffectResult effect_skill_2006(BattleContext* context, const EffectArgs& args) {
    if (!context || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }

    const int self = args.int_args[0];
    // ⚠️ 被拦方**自己推导**，不信 args[1]：这条盔永远是"施放方拦住对手的攻击"，
    //    `1 - self` 是语义上唯一的答案。少一个可被外部 args 影响的入口。
    const int opp = (self == 0) ? 1 : 0;
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }

    // 1) 次数盔：拦对手的**攻击技能**，1 次，可穿；带后续子句 → 被穿也消耗。
    //    返回**授予句柄** —— 下面用它精确匹配"是不是我这条盔被结算了"。
    const int grant_id = g_core->seal_skill(
        context, /*source=*/self, /*target=*/opp, /*effect_id=*/2006,
        /*attribute=*/false, /*attack=*/true, /*count=*/1,
        /*duration_rounds=*/0, /*penetrable=*/true,
        /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
        /*chance_pct=*/100, /*consumed_when_pierced=*/true);
    if (grant_id <= 0) {
        return EffectResult::kOk;
    }

    // 2) 监听**自己这条盔的结算信号**（真正生效 或 被穿）：
    //    · blocked=true  → 免疫成功 → 令对手全属性+1
    //    · blocked=false → 被穿 → **不执行子句**（"被穿之后子句自然没有了"）
    //    两种情况都**自删监听器** —— 这是关键：被穿的盔绝不能留下野监听器，否则
    //    下一次同 effect_id 的盔生效时它会跟着触发，子句多执行一次。
    //    ⚠️ 按 **grant_id** 匹配而不是 effect_id：同 effect_id 的多条盔（如连续两次技能无效）
    //       靠句柄区分，否则旧监听器会认错盔。
    //    ⚠️ once=false：事件对"任何盔结算"都发，once=true 会被别人的盔消耗掉。
    auto watcher_id = std::make_shared<int>(-1);
    *watcher_id = context->event_center_.register_watcher(
        EventType::EVENT_SKILL_ARMOR_RESOLVED,
        self,
        context->roundCount,
        0,      // 永久窗口（靠结算时自删兜底）
        false,  // once
        [self, grant_id, watcher_id](BattleContext* ctx, const BattleEvent& ev) {
            if (!ctx || ev.grant_id != grant_id) {
                return;   // 不是自己这条盔 → 不动
            }
            ctx->event_center_.remove_watcher(*watcher_id);   // 无论生效还是被穿，都该走了
            if (!ev.blocked) {
                return;   // 被穿 → 子句不触发
            }
            // 被拦方 = 施放方的对手，回调内现算（少一个捕获值）
            all_stats_up(ctx, self == 0 ? 1 : 0, +1);   // 免疫成功 → 令对手全属性+1
        }
    );

    return EffectResult::kOk;
}

/**
 * Effect 843 - 下{0}回合令自身所有技能先制+{1}
 * Info: "下{0}回合令自身所有技能先制+{1}"
 * Args: [self(0), target(1), rounds, bonus]
 *
 * 在 BATTLE_FIRST_MOVE_RIGHT 时点注册一个持续 rounds 回合的 ContinuousEffect，
 * 每回合开始时给自己 ws.preemptive_level[self] += bonus。
 * 纯 ContinuousEffect：Effect 持有 843 的模板函数指针 + 参数。
 */

// Effect 2000 - 若对手处于能力提升状态则先制+1且必定命中（大雪纷飞/烬灭神咒剑）。
// 选择期条件先制：loadSkills 特例进 selection_effects_，on_selected 注册到 MOVE_RIGHT 桶，
// 先手权比较前生效（handle_BattleFirstMoveRight 先执行 MOVE_RIGHT 桶再比 preemptive_level）。
// 插件用 inline：ws.preemptive_level 累加 + 设当前使用技能 must_hit。
EffectResult effect_2000_conditional_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    bool opponent_boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[target][i] > 0) {
            opponent_boosted = true;
            break;
        }
    }
    if (!opponent_boosted) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += 1;  // 条件先制
    // 必定命中 → 写**本回合必中凭证**，由 materialize_attack_credential 并进
    // AttackCredential::must_hit（用户 2026-09-13 口径：必中是每回合凭证，不该只看技能写死字段）。
    // ⚠️ 原先这里直接改 `ctx->getPet(owner).skills[idx].must_hit = true` —— 那是**永久**改技能对象
    //    （这场仗之后该技能永远必中，切宠/跨回合都不复原）。本效果体跑在 MOVE_RIGHT
    //    （ws reset 之后、出手之前），正是"本回合生效"的正确窗口。
    ctx->ws.must_hit_grant[owner] = true;
    return EffectResult::kOk;
}

// Effect 2126 - 技能无效时，消除对手回合类效果、能力提升效果，令对手焚烬（烬灭神咒剑）。
// SKILL_INVALID 分支（default_branch_for_effect 特例）。断回合走引擎 inline 入口；
// 焚烬走 apply_anomaly 真原语。
// ⚠️ 2026-09-20 修正：旧实现注释写"焚烬"却直写的是**烧伤**槽（id=2）——把两种异常混为一谈
//    （当时异常扩表未落地）。官方文案是"焚烬"（Incinerate 22，控制类，结束后衍化为烧伤+
//    命中-1），且这是效果施加、应走现代通道全检查链（免疫/弹控/抗性），不该绕过。
EffectResult effect_2126_skill_invalid_compensation(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int target = 1 - owner;
    // 消除对手回合类效果（O(1) 断回合：版本号递增 + 两桶计数器归零 + 事件）。
    // invalidate_all_round_effects 是引擎侧 inline 入口，插件可直接调。
    ctx->invalidate_all_round_effects(target);
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_BREAK, owner, target});
    // 消除对手能力提升效果：走真原语（查免消除强化 STAT_CLEAR + 本体/视图同步）。
    // 原先手抄"正等级清零"循环会绕过保护、且不同步 ws.view_levels。
    if (g_core && g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, target);
    }
    // 令对手焚烬（Incinerate 22）：现代施加通道（免疫/弹控/抗性/转化全检查）。
    if (g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target,
                              static_cast<int>(AbnormalStatusId::Incinerate),
                              /*duration=*/-1, owner);
    }
    return EffectResult::kOk;
}

// effect 781 - 秩序之助："消除对手回合类效果，消除成功则 {n} 回合内令对手使用的属性技能无效"。
// 官方该封属的游戏内表现是**属性技能命中效果失效**——技能照常命中、效果不注册、**不触发**
// SKILL_INVALID 无效补偿（区别于普通封属）。故挂 SEAL_ATTRIBUTE_HIT（SkillInvalidCenter hit 语义）。
EffectResult effect_781_zhixu_zhizhu(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];  // {n}: 属性技能无效回合数
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->break_round_effects || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) == BreakResult::SUCCESS) {
        // 统一语义：source=owner(施放方，生命周期锚)、target=opp(被封方)。
        g_core->seal_skill(ctx, owner, opp, /*effect_id=*/781, /*attribute=*/true, /*attack=*/false,
                           /*count=*/1, /*duration_rounds=*/rounds, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/true,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// 消强后必先：清除对手能力提升（消强）成功(清掉>0) → 下一回合必定先出手(必先, tier1)。
// 对应"第五技能消强成功→下回合必先"。走 CoreApi 真原语(g_core)。
EffectResult effect_xiaoqiang_bixian(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opp = owner == 0 ? 1 : 0;
    if (g_core && g_core->clear_stat_boosts(ctx, opp) > 0) {  // 消强成功
        g_core->grant_guaranteed_first(ctx, owner, /*tier=*/1);  // 下一回合必先
    }
    return EffectResult::kOk;
}

// ── Effect 610 - 遇到天敌时先制+{0}（璨灵圣光）──
// Args: [self, target, bonus]（选择期效果：由 loadSkills 路由进 selection_effects_）
// 天敌 = 对手**固有属性**克制自身固有属性（不看技能属性、不看属性视图，见 is_predator）。
EffectResult effect_610_predator_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[2];
    if (self < 0 || self > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    if (is_predator(ctx, self)) {
        ctx->ws.preemptive_level[self] += bonus;
    }
    return EffectResult::kOk;
}

// ── Effect 695 - {0}回合内{1}%令对手使用的属性技能无效（圣灵悲魂曲）──
// Args: [self, target, rounds, chance]
// 回合型封属（duration>0）+ 概率门（每次响应时掷，见 seal_skill 的 chance_pct）。
EffectResult effect_695_chance_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || chance <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->seal_skill) {
        g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/695,
                           /*attribute=*/true, /*attack=*/false, /*count=*/1,
                           /*duration_rounds=*/rounds, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/chance, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── Effect 700 - 先出手时降低对手所有PP{0}点（圣灵悲魂曲）──
// Args: [self, target, amount]
EffectResult effect_700_first_move_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_second_mover(self)) {
        return EffectResult::kOk;   // "先出手时"——后出手不触发
    }
    if (g_core && g_core->pp_reduce) {
        g_core->pp_reduce(ctx, opp, amount);
    }
    return EffectResult::kOk;
}

// ── Effect 1217 - 消除对手能力提升状态，消除成功则对手下{0}次攻击技能无效（神灵之触）──
// Args: [self, target, times]
EffectResult effect_1217_clear_boost_then_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || times <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->clear_stat_boosts || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // clear_stat_boosts 内查免消除强化(STAT_CLEAR)：被免则返回 0 → 不触发后续封攻击。
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }
    g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/1217,
                       /*attribute=*/false, /*attack=*/true, /*count=*/times,
                       /*duration_rounds=*/0, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── Effect 936 - {0}%令对手{1}，未触发则令对手{2}回合内使用的属性技能无效（圣光吟诵）──
// Args: [self, target, chance, anomaly_id, rounds]
EffectResult effect_936_anomaly_or_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly_id = args.int_args[3];
    const int rounds = args.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    bool applied = false;
    if (roll_skill_percent(*ctx, 936, 0, self, opp, chance) && g_core && g_core->apply_anomaly) {
        // 走真原语：免疫/抗性/反弹/转化全在 core 侧按序处理，这里只看"最终有没有中"。
        applied = g_core->apply_anomaly(ctx, opp, anomaly_id, random_anomaly_duration(), self)
                  == ApplyAnomalyResult::SUCCESS;
    }
    if (applied) {
        return EffectResult::kOk;   // "未触发则…"——中了就不封属
    }
    if (rounds > 0 && g_core && g_core->seal_skill) {
        g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/936,
                           /*attribute=*/true, /*attack=*/false, /*count=*/1,
                           /*duration_rounds=*/rounds, /*penetrable=*/true,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

// ── Effect 1236 - {0}回合内若对手使用攻击技能则造成伤害前附加对手最大体力1/{1}的百分比伤害（圣光吟诵）──
// Args: [self, target, rounds, denom]
// 落点 = 伤害管线的 DETECT 阶段（防御方桶）：此刻红伤已算出但**尚未扣血**（apply_resolved_damage
// 在管线之后），所以对攻击方造成的粉伤确实在"造成伤害前"。DETECT 也是本引擎给
// "受击时惩罚/弹伤"一族预留的阶段（见 DamageEffectCategory::DETECT）。
// 用 d.isRed && d.final>0 判"对手用的是攻击技能"（属性技能不会产生红伤）。
EffectResult effect_1236_punish_attacker(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::DETECT,
        self,
        DamageEffectCategory::DETECT,
        [self, start, expire, denom](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;
            }
            const DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || damage.attackerId < 0 || damage.attackerId > 1) {
                return;
            }
            if (!damage.isRed || damage.final <= 0) {
                return;   // 对手用的不是攻击技能（无红伤）
            }
            const int max_hp = max_hp_of(c, damage.attackerId);
            if (g_core && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(c, damage.attackerId, max_hp / denom,
                                         DamageKind::PERCENT_VALUE, self);
            }
        });
    return EffectResult::kOk;
}

// ── Effect 1237 - 使双方精灵所有技能PP值归零；消除双方能力提升、下降状态以及回合类效果（神圣复苏）──
// Args: 无
// 顺序按用户口径（2026-09-13）：**先断回合再清除**——先断掉对面的"免消除强化"保护，
// 再消强才可能真的消掉（对面免断+免清才真清不掉，这时 clear_stat_boosts 内部查 STAT_CLEAR 会返回 0）。
// 清理弱化**不查任何免疫**（清弱化对目标有利）。
EffectResult effect_1237_purge_both_sides(BattleContext* ctx, const EffectArgs& args) {
    (void)args;
    if (!ctx) {
        return EffectResult::kOk;
    }
    for (int side = 0; side <= 1; ++side) {
        if (g_core && g_core->break_round_effects) {
            g_core->break_round_effects(ctx, side);
        }
        if (g_core && g_core->clear_stat_boosts) {
            g_core->clear_stat_boosts(ctx, side);   // 查免消除强化(STAT_CLEAR)
        }
        if (g_core && g_core->clear_stat_drops) {
            g_core->clear_stat_drops(ctx, side);    // 不查任何免疫
        }
        pp_zero_all(ctx, side);
    }
    return EffectResult::kOk;
}

// ── Effect 1222 - 先出手时使对手随机{0}个技能PP值归零（神圣启示歌）──
// Args: [self, target, count]
EffectResult effect_1222_first_move_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_second_mover(self)) {
        return EffectResult::kOk;   // "先出手时"
    }
    ElfPet& pet = ctx->getPet(opp);
    // 只从"还有 PP 且非无限 PP"的技能里随机挑（pp == -1 = 官方无限 PP，不参与）。
    std::vector<ChanceOption> candidates;
    for (int i = 0; i < static_cast<int>(pet.skills.size()); ++i) {
        if (pet.skills[i].pp > 0) {
            candidates.push_back({i, 1});
        }
    }
    for (int i = 0; i < count && !candidates.empty(); ++i) {
        const int slot = choose_skill_slot(*ctx, 1222, 0, self, opp, candidates);
        pet.skills[slot].pp = 0;
        candidates.erase(std::find_if(candidates.begin(), candidates.end(),
                                      [slot](const ChanceOption& option) { return option.id == slot; }));
    }
    return EffectResult::kOk;
}

// ── Effect 499 - 后出手时下回合所有技能先制+{0}（神圣启示歌）──
// Args: [self, target, bonus]
// "下回合" → 窗口只覆盖 R+1：NextRounds 折算起点 = R+1，duration=1 → 生效 [R+1, R+1]。
EffectResult effect_499_second_move_next_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[2];
    if (self < 0 || self > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(self)) {
        return EffectResult::kOk;   // "后出手时"
    }
    // 复用 843 的时点效果体（把 bonus 累加进 ws.preemptive_level），但窗口手工指定为
    // "下一回合且仅一回合"，故不走 register_round_effect 的家族查询。
    Effect e(499, 0, self, /*left_round=*/1,
             EffectArgs(std::vector<int>{self, 1 - self, bonus}), &effect_move_right_priority_bonus);
    const int start_round = ctx->round_effect_start_round(self, 1, EffectWindowKind::NextRounds);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_MOVE_RIGHT, self,
                                                 /*duration=*/1, start_round);
    ce->source_id_ = 499;
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, self, std::move(ce));
    return EffectResult::kOk;
}

// ── Effect 854 - 令对手下1次使用的威力高于{0}的攻击技能无效 ──
// 技能：天叶领域 24560（side_effect=[532,191,854]）/ 余温之辰 26170（[191,1065,854]），
//   两处 854 的参数都是 [120] → 阈值 = args[0]。
//
// 这是**条件盔**：`RuleCenter` 的 `RuleTicket::condition` 已经支持，而 `notify` 对条件不满足的
// 条目是**跳过、不消耗** —— 于是三件事同时免费成立：
//   · 对手用威力 ≤120 的攻击技能 → 不拦、**也不扣次数**（下次用大威力技能仍被拦）✓
//   · 对手用属性技能 → `responds_to(false)` 直接跳过，同样不消耗 ✓
//   · 对手本回合**没出手**（被控/换宠）→ notify 根本不为它跑，自然不消耗 ✓
// ★ 这正好回答了我们 2026-09-14 记的疑问（"要不要确保对手本回合会出手才注册当回合盔"）：
//   **不需要那套选择期监听器 + 当回合狮盔 + 事件撤销**——把判定放进条件里、让规则中心在
//   "对手真正用技能时"求值即可，既不白挂也不残留。"选完技能后检查面板威力"那版可以丢掉。
// ★ 判据用 **面板威力**（`Skills::power`，由 `notify` 传入），**不是** `ws.skill_power_view`——
//   官方口径：威力宝石/变威力类效果改的是视图，条件盔看的是面板（用户 2026-09-13）。
// ⚠️ `penetrable=false`：与代码里"条件盔/龙威 = false"的既有分类一致（待实测复核）——
//    被 699 穿盔凭证也照样拦。
constexpr int k854ThresholdArgIndex = 2;   // args = [owner, target, {0}=阈值]

EffectResult effect_854_conditional_armor(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count <= k854ThresholdArgIndex) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int threshold = args.int_args[k854ThresholdArgIndex];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int opponent = 1 - self;
    // 条件：**攻击技能**且**面板威力** > 阈值。SEAL_ATTACK 的 responds_to 已经兜了"攻击技能"
    // 那一层，这里显式写出来是为了让条件自解释。
    auto condition = [threshold](BattleContext*, int, int power, bool is_attribute_skill) {
        return !is_attribute_skill && power > threshold;
    };
    ctx->rule_center_.grant_seal(
        /*source_owner=*/self, /*source_slot=*/-1, /*source_effect_id=*/854,
        /*target=*/opponent, SealKind::SEAL_ATTACK,
        /*counts=*/1, /*rounds=*/0,
        /*penetrable=*/false,              // 条件盔 → 龙威一侧（不可穿）
        /*scope=*/EffectScope::ON_STAGE,   // 随切换失效（施放方换宠即清）
        /*condition=*/condition,
        /*source_valid_id=*/0);            // 不是回合类效果 → 不随断回合作废
    return EffectResult::kOk;
}

// ── Effect 2490 - 此技能微弱对手时技能先制+3、获得本系属性加成并以神灵系进行伤害结算 ──
// 技能：薇尔诗·乐园之初诞 38421（args_num=0 → 先制数与"神灵系"都写死在文案里）。
//
// **选择期效果**：core 把它放进 `Skills::selection_effects_`，`on_selected` 统一注册到
// `BATTLE_FIRST_MOVE_RIGHT`（早于先手权结算），与 610 天敌 / 2000 条件先制同路。
//
// Args: `[self, target, 技能静态系别0, 技能静态系别1]`
//   —— core 在 `loadSkills` 里把**技能槽位的静态系别**追加进 args[2]/args[3]。
// ★ 判"是否微弱"必须用**技能静态系别**，**不能读 `ws.skill_element_view`**：视图会被
//   "以XX系别进行伤害结算"这类效果改写，读视图等于把改写后的系别喂回判定（用户 2026-09-14 口径）。
//   对手系别读 `ws.view_elementalAttributes[target]`（对手"改系别"后按当前状态判）。
//
// 触发后三件事：
//   ① `ws.preemptive_level[self] += 3`（先制，先手权结算前生效）
//   ② **授予**"以神灵系进行伤害结算"（写 `skill_element_grant` 而不是直接写
//      `skill_element_view`）—— 直接写视图会被 `resolve_skill_execution` 在 ON_SKILL_HIT
//      的物化覆盖（MOVE_RIGHT 早于 ON_SKILL_HIT）；同 `must_hit_grant` 的教训。
//   ③ `ws.force_involve[self] = true`（"获得本系属性加成"）—— `calculateDamage` 里 `involve()`
//      读的是**技能真实系别**，改元素视图拿不到本系加成，故走显式开关。
constexpr int k2490DeityElement = 223;   // 神灵系（skill_types.id=223，skillType.json "deity"）
constexpr int k2490PriorityBonus = 3;

// 选择期"微弱→先制"通用体（785/2030/2490 共用）：
// `atk_elem` = **判据系别**，由调用方给（785 给精灵系别视图、2030/2490 给技能静态系别）；
// 对手系别读 `ws.view_elementalAttributes[target]`（对手被"改系别"后按当前状态判）。
// 返回是否判定为微弱。
bool weak_priority(BattleContext* ctx, int self, int target, const int atk_elem[2], int bonus) {
    if (!ctx || !atk_elem || self < 0 || self > 1 || target < 0 || target > 1) {
        return false;
    }
    const int* defender_elem = ctx->ws.view_elementalAttributes[target];
    const double restraint = (g_core && g_core->restraint_multiplier)
        ? g_core->restraint_multiplier(atk_elem, defender_elem)
        : 1.0;
    if (!(restraint < 1.0)) {
        return false;   // 不微弱 → 不触发
    }
    if (bonus != 0) {
        ctx->ws.preemptive_level[self] += bonus;
    }
    return true;
}

// ── Effect 2030 - 若本次技能微弱对手则先制+1 ──
// 技能：孤烛迷徒录 37210 / 影月散华 37768 等（args_num=0 → +1 写死在文案里）。
// **选择期效果**（core → `selection_effects_` → BATTLE_FIRST_MOVE_RIGHT）。
// 判据 = **本次技能**的系别 → 用 core 追加进 args[2]/args[3] 的**技能静态系别**。
constexpr int k2030PriorityBonus = 1;

EffectResult effect_2030_skill_weak_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int skill_elem[2] = {args.int_args[2], args.int_args[3]};
    weak_priority(ctx, args.int_args[0], args.int_args[1], skill_elem, k2030PriorityBonus);
    return EffectResult::kOk;
}

// ── Effect 785 - 若自身攻击对手时克制关系为微弱则先制+2 ──
// 技能：怒不可揭 19024 / 逆命龙魂 24163 / 上古恶志 24750 等（args_num=0 → +2 写死在文案里）。
// **选择期效果**，与 2030/2490 同路。
// ★ 用户 2026-09-14 口径：785 的"**自身**"指**精灵系别**（不是技能系别）——2030「本次技能」/
//   2490「此技能」才判技能系别。故此处读 `ws.view_elementalAttributes[self]`（精灵系别**视图**，
//   这样"属性反转"这类改系别效果会被计入；判"此技能"的那两条才读技能槽位静态值以防自污染）。
constexpr int k785PriorityBonus = 2;

EffectResult effect_785_self_weak_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int* self_elem = ctx->ws.view_elementalAttributes[self];
    weak_priority(ctx, self, args.int_args[1], self_elem, k785PriorityBonus);
    return EffectResult::kOk;
}

// ── Effect 2490 - 此技能微弱对手时技能先制+3、获得本系属性加成并以神灵系进行伤害结算 ──
EffectResult effect_2490_weak_reward(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int skill_elem[2] = {args.int_args[2], args.int_args[3]};
    if (!weak_priority(ctx, self, target, skill_elem, k2490PriorityBonus)) {
        return EffectResult::kOk;   // 不微弱 → 本条不触发
    }
    ctx->ws.skill_element_grant[self][0] = k2490DeityElement;
    ctx->ws.skill_element_grant[self][1] = 0;
    ctx->ws.skill_element_grant_valid[self] = true;
    ctx->ws.force_involve[self] = true;
    return EffectResult::kOk;
}

// ── Effect 2501 - 技能无效时，重新进行伤害结算且自身每有{0}点特攻值威力翻倍1次（薇尔诗·乐园之初诞）──
// 原文：「技能无效时，重新进行伤害结算且自身每有260点特攻值威力翻倍1次」（args_num=0 → 260 写死在文案里）
//
// **SKILL_INVALID 分支**（`default_branch_for_effect(2501)` 已登记）：技能被盔/威/封属拦下时，
// 本效果在 `BATTLE_*_SKILL_EFFECT` 时点照常执行（技能效果无效 ≠ 本效果不跑）。
// 它改写 **视图威力** → 写入即置"变威力"标记 → 被 `handle_*_AttackDamage` 早退出口的
// `run_attack_damage_from_invalid_skill` 抓到 → 隔着盔重算并打出红伤。
// 官方 L95：「相当于打盔会重新计算攻击伤害，类似于变威力技能的重新计算，**但是技能效果还是无效，
//           只有攻击伤害**」。
//
// ⚠️ 特攻读**当前视图值**（`getTempAbilityValue`，含能力等级）——与伤害公式同源。
//    若官方实际按**面板值**（`numericalProperties`）算，改这里一行即可。
// ⚠️ "翻倍1次" = ×2 累乘 N 次（每满 260 点翻一倍）：N=2 → 威力 ×4。
// Args: [self, target]（本效果无参数）
constexpr int k2501SpecialAttackPerDouble = 260;

EffectResult effect_2501_recalc_on_invalid(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int sp_atk =
        ctx->ws.getTempAbilityValue(self, NumericalPropertyIndex::SPECIAL_ATTACK);
    const int doublings = sp_atk / k2501SpecialAttackPerDouble;
    if (doublings <= 0) {
        return EffectResult::kOk;   // 不足一次 → 不产生变威力标记 → 技能无效就是零伤害
    }
    int power = static_cast<int>(ctx->ws.skill_power_view[self]);
    if (power < 0) {
        power = 0;   // 未物化（理论上不会：resolve_skill_execution 已打过底）
    }
    for (int i = 0; i < doublings; ++i) {
        power *= 2;
    }
    ctx->ws.skill_power_view[self] = power;   // ★ 写入即置"变威力"标记 → 触发第二次结算
    return EffectResult::kOk;
}

// ── effect 87 「恢复自身所有PP值」—— 永恒 20608 ──
// Args = [owner, target]。⚠️ 目前**没有恢复 PP 的原语**（只有 `pp_reduce`），
// 且 PP 是 pet 上的公开字段、插件可直接写——这里直写并注明；
// 若将来 PP 恢复也要走"封回PP / 修正"一类内核，再补原语（与 heal 对称）。
EffectResult effect_87_restore_all_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    for (Skills& sk : ctx->getPet(owner).skills) {
        if (sk.pp < 0) {
            continue;   // 无限 PP 技能不动
        }
        sk.pp = sk.maxPP;
    }
    return EffectResult::kOk;
}


// ── effect 52 「{0}回合若本方先手攻击，使得对方的技能失效」—— 虚无 20498（Args=[2]）──
// Args = [owner, target, 回合数]。
// ★ 判据 = **本回合是不是我先出手**。实现上**不查 `ws.preemptive_right`**：
//   ① `handle_BattleFirstMoveRight` 是"先把 pr 清成 NONE → 跑桶 → 再算 pr"，
//      MOVE_RIGHT 的桶跑的时候先手权**还没算出来**（查了只会恒为 NONE）；
//   ② 更干净的做法是**借桶的执行面**：`BATTLE_FIRST_*` 时点的桶**只跑先手方的桶**
//      （`execute_registered_actions(first_mover_id, ...)`），所以把 tick 挂在
//      `BATTLE_FIRST_BEFORE_SKILL_HIT` 上——**它被执行本身就等于"我这一回合先手"**，
//      后手方的同名条目根本不会运行。零条件、零先手权读取。
//   "使得对方的技能失效" = 龙威（封攻击+属性，**不可穿**），1 回合。
// ⚠️ 未区分"先手**攻击**"里的"攻击"二字：先手方无论用攻击还是属性技能都触发。
//    若实测口径要求"必须是攻击技能"，在 tick 里查 `roundChoice[owner]` 对应的技能类型即可。
constexpr int kEffect52Ticks = 52;
EffectResult effect_52_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 能跑到这里就说明本回合是 owner 先出手（桶只跑先手方），无需再判先手权。
    if (g_core && g_core->seal_skill) {
        g_core->seal_skill(ctx, /*source=*/owner, /*target=*/1 - owner, /*effect_id=*/kEffect52Ticks,
                           /*attribute=*/true, /*attack=*/true, /*count=*/1,
                           /*duration_rounds=*/1, /*penetrable=*/false,
                           /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                           /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

EffectResult effect_52_first_move_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 挂在 FIRST_BEFORE_SKILL_HIT（只跑先手方的桶）——见 effect_52_tick 的注释。
    register_round_effect(ctx, State::BATTLE_FIRST_BEFORE_SKILL_HIT, owner, kEffect52Ticks,
                          rounds, {owner, 1 - owner}, &effect_52_tick);
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    SKILL_EFFECT(2006, effect_skill_2006); // 技能无效→免疫攻击→全属性+1（事件链）
    SKILL_EFFECT(2000, effect_2000_conditional_priority);      // 条件先制+必中
    SKILL_EFFECT(2126, effect_2126_skill_invalid_compensation); // 技能无效补偿
    SKILL_EFFECT(999900, effect_xiaoqiang_bixian);            // 消强成功→下一回合必先
    SKILL_EFFECT(781, effect_781_zhixu_zhizhu);               // 消回合+命中失效封属(秩序之助)
    SKILL_EFFECT(610, effect_610_predator_priority);          // 璨灵圣光: 遇天敌先制+{0}(选择期)
    SKILL_EFFECT(695, effect_695_chance_seal_attribute);      // 圣灵悲魂曲: N回合内{X}%封属性
    SKILL_EFFECT(700, effect_700_first_move_pp);              // 圣灵悲魂曲: 先出手降对手所有PP
    SKILL_EFFECT(1217, effect_1217_clear_boost_then_seal_attack); // 神灵之触: 消强成功→下N次攻击技能无效
    SKILL_EFFECT(936, effect_936_anomaly_or_seal);            // 圣光吟诵: {X}%异常/未触发则封属性
    SKILL_EFFECT(1236, effect_1236_punish_attacker);          // 圣光吟诵: 受攻击→伤害前附加百分比
    SKILL_EFFECT(1237, effect_1237_purge_both_sides);         // 神圣复苏: 双方PP归零+净化
    SKILL_EFFECT(1222, effect_1222_first_move_random_pp_zero); // 神圣启示歌: 先出手随机N个PP归零
    // 乐园之初诞 38421；SKILL_INVALID 分支（default_branch_for_effect 已登记）——
    // 技能无效时改写视图威力 → 隔着盔打出攻击伤害（技能效果仍无效）。
    SKILL_EFFECT(2501, effect_2501_recalc_on_invalid);
    // 乐园之初诞 38421；**选择期**（core 放进 selection_effects_ → BATTLE_FIRST_MOVE_RIGHT）。
    SKILL_EFFECT(2490, effect_2490_weak_reward);
    // 同族的另两条选择期"微弱→先制"（区别只在判据系别：785 判精灵、2030 判技能）。
    // 天叶领域 24560 / 余温之辰 26170：条件盔（面板威力 > 阈值的攻击技能无效）。
    SKILL_EFFECT(854, effect_854_conditional_armor);
    SKILL_EFFECT(785, effect_785_self_weak_priority);
    SKILL_EFFECT(2030, effect_2030_skill_weak_priority);
    SKILL_EFFECT(499, effect_499_second_move_next_priority);  // 神圣启示歌: 后出手下回合先制+N
    SKILL_EFFECT(52, effect_52_first_move_seal);        // 虚无: 先手则令对手技能失效
// ════════════════════════════════════════════════════════════════════
// 批 6：消回合成功族（2026-09-17，220 技）——break_round_effects 结果分支：
//   SUCCESS（真的清掉了东西）才给后续子句，NO_EFFECTS（对手本来就没有）不给。
//   797(128技) 消除成功 → 对手 {0} 回合内无法通过自身技能恢复体力（封回血 HEAL_BLOCK）
//   889(92技)  消除成功 → 令对手异常 {0}（{0}=battle_effects 异常码，走现代通道）
// ════════════════════════════════════════════════════════════════════

// effect 797 —— 消除对手回合类效果，消除成功{0}回合内对手无法通过自身技能恢复体力
// Args=[owner,target,rounds]。封回血挂在**对手**身上（HEAL_BLOCK 免疫票，heal_impl 入口查）。
EffectResult effect_797_break_then_heal_block(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 对手本来就没有回合类效果 → 子句不触发
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/797,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/797);
    return EffectResult::kOk;
}

// effect 889 —— 消除对手回合类效果，消除成功则令对手{0}（{0}=异常码）
// Args=[owner,target,anomaly]。参数化"令对手{状态}"族 → 走**现代**施加通道（全检查链）。
EffectResult effect_889_break_then_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 没有可清的回合类效果 → 不给异常
    }
    g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), self);
    return EffectResult::kOk;
}

// ── Effect 39 - {0}%降低对手所有技能{1}点PP值，然后若对手所选择的技能PP值为0
//               则当回合对手无法行动（孑身诫 37458）──
// Args=[owner,target,percent,drop]。与控制异常的区别（用户 2026-09-22 口径）：
// 无法行动跳过主流程且**压制对手额外行动声明**；回合完成自动清。
// 判定"对手所选择的技能"：对手 roundChoice 若是 SELECT_SKILL，看它当前 PP——
// 本效果的降 PP 恰好先行执行（同链内 931 先吸强化归零 / 本条再降点），
// 所以"选了技能且 PP 被打到 0"就会命中。
EffectResult effect_39_lower_pp_then_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int percent = args.int_args[2];
    const int drop = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || drop < 0) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->getPet(opp);
    if (roll_skill_percent(*ctx, 39, 0, self, opp, percent)) {
        for (int i = 0; i < 5; ++i) {
            Skills& sk = pet.skills[i];
            if (sk.pp > 0) {
                sk.pp = (sk.pp > drop) ? sk.pp - drop : 0;
            }
        }
    }
    // "对手所选择的技能PP值为0"：只看对手本回合**已选择**的技能（后手场景 roundChoice
    // 已落；先手场景对手还没选 → 无法行动只对后手对手生效，与官方实测一致）。
    if (ctx->roundChoice[opp][0] == kActionSelectSkill) {  // SELECT_SKILL
        const int chosen = ctx->roundChoice[opp][1];
        if (chosen >= 0 && chosen < 5 && pet.skills[chosen].pp == 0) {
            ctx->suppress_extra_action(opp);
        }
    }
    return EffectResult::kOk;
}

    // ── 批 6：消回合成功族（2026-09-17）──
    SKILL_EFFECT(797, effect_797_break_then_heal_block);  // 消回合成功→对手N回合封回血
    SKILL_EFFECT(889, effect_889_break_then_anomaly);     // 消回合成功→令对手异常{0}
    SKILL_EFFECT(87, effect_87_restore_all_pp);         // 永恒: 恢复自身所有PP
    SKILL_EFFECT(39, effect_39_lower_pp_then_lock);     // 降PP→所选技能PP=0→当回合无法行动

}  // namespace
