#include <utils/sbs_rand.h>
#ifndef MOVES_LIB_COMMON_H
#define MOVES_LIB_COMMON_H

// moves_lib 各家族文件的共享助手（2026-09-17 从 lib_1.cpp 按家族拆分，行为不变）。
// g_core 是 **inline 变量**：同一 .so 内所有编译单元共享一份（插件不链接 sim_core，
// 原语一律经它的函数指针槽调用）。helpers 全部 inline（头内定义、链接器合并）。

#include <effects/effect.h>
#include <effects/continuousEffect.h>
#include <fsm/battleContext.h>
#include <plugin/plugin_interface.h>
#include <plugin/core_api.h>
#include <abnormal-system/abnormal-types.h>

#include <cstdlib>
#include <algorithm>
#include <memory>
#include <vector>
#include <any>

// ── storage 安全读取（2026-10-02 全库 any_cast 治理）─────────────────────────
// 抛式 std::any_cast<int>(any) 在类型不符时直接炸进程（fuzz any_cast_fault 家族，
// seed 446/2397 等）：plugin_storage / soulmark_storage 是共享键空间（守卫布尔、
// 别家窗口可能落进同前缀键位），类型不符 = "不是你的" → 必须按缺失语义回退，
// 绝不允许抛。取值形式全库禁用，读 int 一律走 any_int_or（指针形式 + fallback）。
inline int any_int_or(const std::any& a, int fallback = 0) {
    if (const int* p = std::any_cast<int>(&a)) {
        return *p;
    }
    return fallback;
}


// core 初始化时传入的原语函数指针：inline 变量，同一 .so 内所有 TU 共享一份，
// 由 lib_1.cpp 的 skill_plugin_register 注入。效果侧原语调用一律走它的函数指针槽。
inline const CoreApi* g_core = nullptr;

// 843/499 共用的先制累加时点效果体（MOVE_RIGHT 桶：往 ws.preemptive_level 加 bonus）。
inline EffectResult effect_move_right_priority_bonus(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[2];
    if (owner < 0 || owner > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// 843/499 共用的先制累加时点效果体（MOVE_RIGHT 桶：往 ws.preemptive_level 加 bonus）。

// 本回合选中技能是否为**攻击技能**（物理/特殊；属性=否）。
// 用途：481「下N回合自身**攻击**先制+X」/ 681「下N回合自身**攻击技能**必定致命、必定命中」
// 的**攻击限定**——两者官方模板都带"攻击"字样（图鉴转述有时省略，以 effect_info 模板为准）。
// 读选择期结果 roundChoice（MOVE_RIGHT 时已写入）：非 SELECT_SKILL 一律按"不是攻击技能"。
// ⚠️ 判据统一（2026-10-03，用户裁定）：本助手是**选择期语义**（读静态 type）——只允许在
//    技能尚未执行的语境用（条件先制等 MOVE_RIGHT 判定、"下回合攻击技"的预判门）。
//    技能已执行（SKILL_EFFECT/ON_SKILL_HIT 之后、行动末/回合末/事件回调）的"用了攻击技吗"
//    一律改用 executed_skill_is_attack（读 skill_type_view 实际物化类型，变型技/替换技正确）。
inline bool selected_skill_is_attack(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(owner).skills.size())) {
        return false;
    }
    return ctx->getPet(owner).skills[static_cast<std::size_t>(idx)].type != SkillType::Attribute;
}

// ── 执行期判据（2026-10-03 判据统一，用户裁定：**使用 = 成功使用出来，被控不算**）──
// "本回合是否成功执行过技能"：skill_use_seq（resolve_skill_execution 主流程 ++；
// 被控/嗑药/NONE **不动**；miss/被盔算使用）较回合初基线（ws.skill_use_seq_round_start，
// FSM 回合初快照）有增量。⚠️ 有增量只说明"用了技能"，类型用 executed_skill_is_attack 判。
inline bool executed_skill_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

// "本次成功执行的技能是否**攻击技**"（执行期/行动末/回合末/事件回调语境）：
// 读 ws.skill_type_view——每次使用在 resolve_skill_execution 物化**实际执行技能**的类型
// （替换技经 resolve_executing_skill、变型技如天启星魂 2210 在 SKILL_EFFECT 改写视图，
// 伤害公式/特性门/FSM 属性门消费的都是它）。⚠️ 仅在本回合确有执行（executed_skill_
// this_round 或 ON_SKILL_HIT 之后的回调）时调用才有意义；选择期判"选中的技能"仍用
// selected_skill_is_attack（此时视图是上一回合的旧物化）。
inline bool executed_skill_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

inline bool roll_percent(int percent) {
    if (percent <= 0) return false;
    if (percent >= 100) return true;
    return (sbs_rand() % 100) < percent;
}

// Helper: 天敌判定——**对手的固有属性克制自身固有属性**（倍数 > 1）。
// 官方实测口径（用户 2026-09-13 游戏内确认，见 docs/02-效果系统/官方机制理解与引擎缺口对照.md §五）：
//   ① 看**固有属性**（pet.elementalAttributes），不是属性视图——用龙琰改系别**不影响**天敌判定；
//   ② **不看技能属性**（与克制伤害计算"技能系别 vs 防御方系别"是两回事）；
//   ③ 双属性按官方组合相乘（calculateRestraintMultiples 已实现双向查表）。
// 落点：opp 克制 self → self 遇到天敌。
inline bool is_predator(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    if (!g_core || !g_core->restraint_multiplier) {
        return false;
    }
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& opp = ctx->getPet(1 - owner);
    // 克制表是运行时数据、符号在 sim_core 里 → 必须走 CoreApi（插件不能直调 Calculation）。
    return g_core->restraint_multiplier(opp.elementalAttributes.data(),
                                        self.elementalAttributes.data()) > 1.0;
}

/**
 * Effect 4 - Self stat modification
 * Info: "技能使用成功时，{1}%改变自身{0}等级{2}"
 * Args: [attackerId(0), targetId(1), statIndex, probability, levelChange]
 *
 * Used by: 高速移动 (20017) - self speed +2
 *
 * 走 stat_change 真原语（**不是** stat_drop）：自身增益/自降不吃免弱，免弱只挡对手施加的弱化。
 * 顺带拿到本体/视图同步——原先手抄 clamp 只改 pet.levels，ws.view_levels 不同步 →
 * 自身速度+2 当回合不参与先手权比较、自身攻+2 当回合不加伤害（视图是那些公式的读取源）。
 */

// Helper: 全属性等级 +N（**六项能力提升**：攻击/特攻/防御/特防/速度/**命中**）
// 用户 2026-09-18 口径：「全属性提升含命中，但凡提到等级提升下降的，都是指包含命中的
// 不是包含体力的 6 种」——旧注释写的"不含 HP"是把**能力等级**与**基础数值**搞混了：
// 能力提升官方就是 6 种（无体力等级），槽 5 是命中（见 battleContext.h 的长注释）。
// 走 stat_change 真原语：本体/视图一并同步（手抄 clamp 只改本体，视图会分裂）。
inline void all_stats_up(BattleContext* ctx, int owner, int delta) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (g_core && g_core->stat_change) {
            g_core->stat_change(ctx, owner, i, delta);
        }
    }
}

/**
 * Effect 2006 - 技能无效时，免疫下1次对手的攻击，免疫成功则令对手全属性+1
 * Info: "技能无效时，免疫下1次对手的攻击，免疫成功则令对手全属性+1"
 * Args: [self(0), target(1)]
 *
 * 触发条件：本技能执行结果为 SKILL_INVALID（miss/被无效）。
 *
 * ⚠️ **建模口径（2026-09-13 从"伤害管线免伤"改成 SEAL 盔）**：
 *   官方实测依据（用户游戏内确认）：无念归空净 37087 的这条盔**能被穿盔凭证穿透**——
 *   而穿盔凭证（`ignore_attack_immunity`）只作用在 RuleCenter 的 SEAL 条目上，
 *   管线挡伤根本不在它的作用面。旧实现走 BLOCK 阶段挡伤：表现上"打下来不扣血"一样，
 *   但**被穿 / 断回合 / 强制执行**三种行为全不对。
 *
 * 两段式（两个不同信号）：
 *   1) 挂一条**可穿**的次数盔（count=1）：source=self（生命周期锚=施放方），
 *      target=对手（谁的攻击被拦）。`consumed_when_pierced=true`——本模板**带后续子句**
 *      （"免疫成功则…"），被穿时消耗次数（用户 2026-09-13 实测口径，与 2270「触发成功则…」同类）。
 *   2) 监听 `EVENT_SKILL_ARMOR_RESOLVED`（每条盔**被结算**时发：真正生效 或 被穿），
 *      **按 grant_id 匹配**自己那条 → blocked=true 时令对手全属性+1。
 *
 * ⚠️ **为什么被穿也要收信号**（用户 2026-09-14 指出）：被穿的盔必须能把它的监听器一起撤掉。
 *   否则：盔A被穿（监听器留下）→ 之后盔B生效 → A/B 两个监听器都触发 → 子句**多执行一次**。
 *   所以两种结算都自删监听器，只是 blocked=false 时不执行子句
 *   ——这才是"被穿之后子句自然没有了"的正确实现。
 */

// ════════════════════════════════════════════════════════════════════
// 批 3：圣灵谱尼（经典 5000）8 个专属技能 / 19 个 effect
//   璨灵圣光 24319  : 525 57 610 694 776
//   落芳天华 17912  : 85 497（批 2 已做）
//   圣灵悲魂曲 18006: 695 700
//   神灵之触 31140  : 1217 906
//   圣光吟诵 25678  : 936 1236 693
//   神圣复苏 31141  : 1237 1005
//   光荣之梦 25679  : 1238 677 1239 843（843 批 2 已做）
//   神圣启示歌 31142: 1222 499
// ════════════════════════════════════════════════════════════════════

// ── helper：注册"N 回合窗口"技能效果（每到一个时点跑一次，到期由桶清理）──
// 窗口起点**必须**走 round_effect_start_round 折算，否则后出手方会少结算一回合
// （官方回合数判定，见 docs/02-效果系统/官方机制理解与引擎缺口对照.md §五.0）。
inline int register_round_effect(BattleContext* ctx, State trigger, int owner, int effect_id,
                          int rounds, std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || rounds <= 0 || !fn) {
        return 0;
    }
    Effect e(effect_id, 0, owner, rounds, EffectArgs(std::move(ints)), fn);
    const EffectWindowKind window_kind = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(effect_id)
        : EffectWindowKind::InRounds;
    const int start_round = ctx->round_effect_start_round(owner, rounds, window_kind);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, rounds, start_round);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
    return start_round;
}

// ── helper：注册"N 回合窗口"且窗口属**下N回合**（NextRounds）的效果 ──
// 与 register_round_effect 的唯一差别是窗口起点**显式**取 NextRounds（本回合不算，
// 先/后出手都从下一回合起算）——不走 effect_window_kind 家族查询，因为该查询读的是
// 认证数据层 custom_effect_overrides 的声明，而 481/681 尚未在那张表登记
//（跨 agent 共享的只读数据件不宜由插件改动；将来补声明后可改用 register_round_effect）。
inline int register_next_rounds_effect(BattleContext* ctx, State trigger, int owner, int effect_id,
                                       int rounds, std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || rounds <= 0 || !fn) {
        return 0;
    }
    Effect e(effect_id, 0, owner, rounds, EffectArgs(std::move(ints)), fn);
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, rounds, start);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
    return start;
}

// ── helper：PP 归零（原语只提供"减 N"，归零传一个超过任何 max_pp 的值）──
inline void pp_zero_all(BattleContext* ctx, int target) {
    if (g_core && g_core->pp_reduce) {
        g_core->pp_reduce(ctx, target, 999);
    }
}

// ── helper：本回合该 owner 是否"真的点了技能"（每回合使用技能类效果的触发条件）──
// roundChoice 在选择期写入、跨 ROUND_START 保留（battleFsm 的 preserved_round_choice），
// 到 AFTER_ACTION 时仍是 -1 表示这回合没出招（被控/未提交）。
// 本回合该 owner 是否"真的点了技能"（每回合使用技能类效果的触发条件）。
// roundChoice[owner][0] 存 ActionType（battleFsm.h：CHOOSE_PET=0/SELECT_SKILL=1/
// USE_MEDICINE=2/NONE=3）：只有 SELECT_SKILL 才算"使用技能"。USE_MEDICINE 是**嗑药**
// ——完全跳过出手（用户 2026-09-17 口径）："每回合使用技能恢复最大体力1/2"这类效果在
// 嗑药回合**不结算**；NONE（被控但点了技能）同理不算用过技能。
// ⚠️ 插件不 include battleFsm.h（会拖进 boost 线程池头）→ 用字面值 1 锚 ActionType::SELECT_SKILL。
// roundChoice 在选择期写入、跨 ROUND_START 保留，到 AFTER_ACTION 时仍可读。
// ⚠️ 判据统一（2026-10-03）：本助手是**选择位语义**（被控/未执行也返回 true）——执行期
//    判"成功使用过技能"用 executed_skill_this_round（skill_use_seq 增量，被控不算）。
inline bool used_skill_this_round(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    return ctx && owner >= 0 && owner <= 1
        && ctx->roundChoice[owner][0] == kActionSelectSkill;
}

// ── helper：把"攻击伤害 +pct%"写进 ws（AMP 阶段读；ws 每回合 reset → 回合效果每回合写）──
inline EffectResult effect_set_damage_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);
    return EffectResult::kOk;
}

// ── helper：把"**非通用**增伤"写进 ws（AMP_EXTRA 阶段读，**乘法**通道）──
// 官方 L352：「通用增伤……**所有的通用增伤加法计算，而非通用增伤全部乘法计算**」，
// 判据是技能文本里的"**额外**提升X%"（如 693 圣光吟诵）。
// 抢第一个空槽（4 槽）→ 同回合多个来源各占一槽、互不覆盖；ws 每回合 reset 归零后重挑。
inline EffectResult effect_set_damage_amp_extra(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_add_extra_mul[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── helper：必先写入（MOVE_RIGHT 时点置 ws.guaranteed_first）──
inline EffectResult effect_set_guaranteed_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int tier = args.int_args[2];
    if (owner < 0 || owner > 1 || tier <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.guaranteed_first[owner] = tier;
    return EffectResult::kOk;
}

// ── helper：最大体力 ──
// ⚠️ `pet.hp` 是 `numericalProperties[HP]` 的**引用别名**（elf-pet.h），即
// numericalProperties[HP] 是**当前**体力而不是最大值；最大体力在 numericalBase[HP]
// （引擎侧 heal_impl / deal_damage 的 PERCENT 换算也都读 numericalBase）。
inline int max_hp_of(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return 1;
    }
    return std::max(1, ctx->getPet(owner).numericalBase[NumericalPropertyIndex::HP]);
}

// ── helper：恢复最大体力的 1/denom，返回**实际**恢复量（吃封回血/恢复修正）──
// 实际恢复量 = 恢复前后 HP 差（heal_amount 只返回 ok/param 三态，且受封回血与恢复%修正影响）。
inline int heal_max_hp_fraction(BattleContext* ctx, int owner, int denom) {
    if (!ctx || owner < 0 || owner > 1 || denom <= 0 || !g_core || !g_core->heal_amount) {
        return 0;
    }
    const int before = ctx->getPet(owner).hp;
    g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner) / denom);
    return ctx->getPet(owner).hp - before;
}

// ═══════════════════════════════════════════════════════════════════════════
// 异常双通道验收件（2026-09-16）：古早施加族 / 古代纯免 / 现代弹控 / 现代参数化施加
//
// 数据库证据（详见 docs_local 交接文档 2026-09-16 段）：
//   - 古早施加 = "写死异常名"模板族 effect 10/11/12/14/15（"命中后{0}%令对方麻痹/中毒/
//     烧伤/冻伤/害怕"，168 个技能在用，瞬雷天闪 10176 = effect 10、5%）→
//     走 apply_anomaly_ancient（主动毒：只被 Ancient 层免疫挡，无视弹控/抗性/转化）；
//   - 现代施加 = 参数化模板族"{0}%令对手{1}"（{1}=battle_effects 异常码；月下伏 37526
//     = effect 2189、args 100/22 → 100% 焚烬）→ 走 apply_anomaly（全检查链）；
//   - 古代纯免 = effect 48"「{N}回合内免疫所有受到的异常状态"（意志模组·怒 29025；
//     1308 个技能在用）→ Ancient 层 ANOMALY 票；
//   - 现代弹控 = effect 191"「{N}回合内免疫并反弹所有受到的异常状态"（1131 个技能）
//     → Modern 层 ANOMALY 票 + REFLECT 条目（弹控族对主动毒整体不可见，免疫半边也
//     只按 Modern 层授予）。
// ═══════════════════════════════════════════════════════════════════════════

// 古早"命中后{0}%令对方XX"族共用体：Args=[owner,target,chance]，异常名写死在各 wrapper。
// 古早"命中后{0}%令对方XX"族共用体：Args=[owner,target,chance]，异常名写死在各 wrapper。
// ⚠️ 概率**申报给原语**（2026-09-22 概率闸门管道）：掷骰搬进原语的 [2.2] 步，位置在
//    亮节族闸门改写**之后**——官方"以实际概率为准"判的是本次实际值，必须在掷骰前问。
//    本路径除这一次掷点外没有别的 rand 消费者（`random_anomaly_duration()` 是常量），
//    所以搬移只影响"目标已死"那条早退分支的 rand 消耗。
//    （对照组：common_trait.cpp 的主动毒/被动毒用"闸门后本地掷骰"——那里搬移会把
//      [duration rand] 与 [概率 rand] 互换次序 = 整场随机序列改动，故刻意不搬。）
inline ApplyAnomalyResult apply_ancient_hit_status(BattleContext* ctx, const EffectArgs& args,
                                            AbnormalStatusId status) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->apply_anomaly_ancient_checked) {
        return ApplyAnomalyResult::INVALID_PARAM;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return ApplyAnomalyResult::INVALID_PARAM;
    }
    return g_core->apply_anomaly_ancient_checked(ctx, target, static_cast<int>(status),
                                                 random_anomaly_duration(), owner, chance,
                                                 ChanceSource::Skill);
}

// ── 先/后手判定：本 handler 被镜像到 FIRST/SECOND 两侧时点，currentState 即出手侧 ──
// （与 effect_unit.cpp 的 is_first_side_state 同判据；插件侧不能引用其匿名命名空间实现）
inline bool acting_side_is_first(BattleContext* ctx) {
    if (!ctx) {
        return false;
    }
    const int v = static_cast<int>(ctx->currentState);
    return v >= static_cast<int>(State::BATTLE_FIRST_ACTION_START)
        && v <= static_cast<int>(State::BATTLE_FIRST_MOVER_DEATH);
}

#endif // MOVES_LIB_COMMON_H

// ── 效果参数解包守卫（2026-09-28 用户定稿：解包不达预期禁止静默 return）──
// 用法：handler 开头把 `if (!ctx || args.int_count < 4) return kOk;` 换成
//   EFFECT_ARG_GUARD(ctx != nullptr, effect_id, args, "需要 ctx");
//   EFFECT_ARG_GUARD(args.int_count >= 4, effect_id, args, "需要 [owner,占位,step,cap]");
// 不满足 → 登记 ctx->report_effect_arg_fault（参数内存排布快照 + 说明，追加式）
// 并 return kOk（效果跳过，行为同旧静默路径，但检查点会写报告/触发 fail-fast）。
#define EFFECT_ARG_GUARD(cond, effect_id, args, note)                                \
    do {                                                                             \
        if (!(cond)) {                                                               \
            if (ctx != nullptr) {                                                    \
                ctx->report_effect_arg_fault((effect_id), (args).int_args,           \
                                             (args).int_count, (note));              \
            }                                                                        \
            return EffectResult::kOk;                                                \
        }                                                                            \
    } while (0)
