// 空元之诗（空元行者 4586）—— 5 条子效果：2086 渍 / 2087 镀 / 2088 柱 / 2089 烙 / 2090 均
//
// 官方 effect_icon 1791（pet 4586）原文：
//   「空元之诗：空元行者技能中的空元之诗效果**不因原本的行动而触发**，每次行动结束后，若本回合
//     所选技能的空元之诗效果生效且条件满足，空元行者会抹除一篇空妄诗章，抹除成功后进行**一次
//     额外行动触发空元之诗效果**」
//
// 落到引擎：这 5 条的**注册时点**被挪到 `BATTLE_FIRST_EXTRA_ACTION`（见
// `src/entities/skills.cpp` 的 `effect_register_state`），于是"不因原本的行动而触发"是结构性
// 保证（普通行动里注册不进来），"额外行动要执行什么"由既有桶模型承载。
// **注册分支**决定"哪一趟注册"（2026-09-20 改版，用户口径"之诗效果都是**固有效果**"）：
//   2086 走 SKILL_INVALID——它的条件就是"若技能无效"；2087~2090 走 **INHERENT**
//   （固有效果档：HIT 与 SKILL_INVALID 两趟都注册）→ 被封属 / miss / 打在盔上照常触发，
//   唯"封效果"类机制可压（引擎暂无该封印种；命中效果失效趟沿用逐节点 nullify 过滤）。
// **条件求值在魂印侧、抹除之前**（`soulmark_2175_poem_declare` 的条件表——先判条件、
// 再看诗章、抹除成功才声明）；本文件 payload 里的同款条件是**复核**（两个时点紧邻、
// 结论必然一致，留着作为直调桶时的护栏）。**"抹除诗章 → 声明额外行动"**由魂印 2175 在
// AFTER_ACTION_END 做——本文件只负责"被声明之后执行什么"。
//
// 宿主映射（全库唯一宿主，各挂在 4586 一把技能上）：
//   2086 渍 → 阅世 36998（先制+3）  2087 镀 → 敕界归元 28534  2088 柱 → 告命诗途 28535
//   2089 烙 → 神忏福音之章 37004   2090 均 → 遗典·妄世律裁 37005
//
// 5 条都是**零参数**（`effect_info.args_num = 0`；宿主的 `side_effect_arg` 里没有它们的位）——
// 数字（50 点 / 50%）写死在文本里，故本文件用常量。
//
// ⚠️⚠️ **"次元·龙系伤害" = 属性伤害**（《赛学必修16—伤害类型》，`reference/scrape/out/all_posts.jsonl` idx=21）：
//   伤害分两大类：**技能伤害**（攻击伤害 / 属性伤害）与**追加伤害**（固定 / 百分比 / 真实）。
//   「**属性伤害**：由属性技能造成的技能伤害…通常会于效果中直接声明造成伤害的**系别与点数**，
//     如"附加 300 点电系伤害"…**往往是固定的数值乘以克制倍数**，若未提及则不会涉及攻防值」
//   → 数值 = **点数 × 克制倍数**，**有系别**（固定/百分比/真实官方明确"不存在系别概念"），
//     且**以红伤形式结算**（用户 2026-09-18 口径："只吃克制关系，最后以红伤形式结算"）
//     → 走 `deal_attribute_damage` 原语（内部 `DamageKind::NORMAL`）：
//       **吃护盾/增伤/减伤/锁伤/挡伤与对应免疫**，emit EVENT_TAKE_DAMAGE、进死亡漏斗。
//       既不是固定伤害（那是粉伤、且无系别），也不是真伤（真伤"无法减免"）。
//   ⚠️ **别退回 `fixed_damage`**——那会同时丢掉克制系数与系别（曾如此近似过一版）。
//
//   《赛学扩展2——属性》（同表 idx=273）补充系别取用：检测类效果读的是**技能 type 与精灵本身
//   type**；属性技能的 type 默认 = 8（普通系）。本文件的系别是**效果级声明**（见下），
//   与宿主技能系别无关。
//
// 收口点：全文件只有 `zhi_shi_typed_damage()` 一处调伤害原语。
#include "common.h"

namespace {

// ── 之诗子效果的公共常量（文本里写死的数字）──
constexpr int kTypedDamagePerUnit = 50;   // "附加50点次元·龙系伤害"
constexpr int kDrainPctOfMaxHpDiff  = 50; // "附加双方体力上限差值50%的次元·龙系伤害"
constexpr int kFifthSkillSlot = 4;        // "第五技能" = 面板槽位下标 4（与 2260 同口径）
constexpr int kAnomalyImmunityCounts = 2; // 镀："免疫下2次受到的异常状态"

// 本回合该 owner 所选技能的**剩余 PP**（"此技能每剩余1点PP值"）。
// ⚠️ 读 `roundChoice[owner][1]` 取**槽位**（本回合选择跨 ROUND_START 保留，额外行动同回合内仍可读）。
// ⚠️ 无限 PP 技能（pp < 0）按 0 计——"每剩余1点"对无限 PP 无意义，且避免 -1×50 变负。
int selected_skill_remaining_pp(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return 0;
    }
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return 0;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return 0;
    }
    const int pp = skills[static_cast<std::size_t>(slot)].pp;
    return pp > 0 ? pp : 0;
}

// 目标身上是否有**控制类异常**（镀的条件）。
// ⚠️ 判"还在生效"必须比 `roundCount < end_round`——`abnormal_status_end_round` 里存的是
//    **绝对结束回合**，自然过期**不会把槽写回 0**（没有任何代码做回写）。所以
//    `end != 0` 那种写法（`effect_unit.cpp` 的 TargetHasAnomaly、`stat_dispel.cpp` 的效果 1001
//    都在用）会把**已自然过期的异常**判成"还在"→ 假阳性。这里按
//    `has_active_abnormal_status` 的语义内联（它实现在 battleContext.cpp、非 inline，
//    插件链接不到——CLAUDE.md 3.9 插件边界只能调头内 inline）。
bool target_has_control_anomaly(BattleContext* ctx, int target) {
    if (!ctx || target < 0 || target > 1) {
        return false;
    }
    const auto& ends = ctx->abnormal_status_end_round[target];
    for (std::size_t id = 0; id < ends.size(); ++id) {
        if (ctx->roundCount < ends[id]
            && is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            return true;
        }
    }
    return false;
}

// 目标方**技能 PP 不为满**的技能个数（柱的条件与倍率）。
// ⚠️ 无限 PP（pp < 0 或 maxPP < 0）不算"不为满"——见 signature_true_damage.cpp 同款守卫。
int count_skills_with_lost_pp(const ElfPet& pet) {
    int n = 0;
    for (const Skills& sk : pet.skills) {
        if (sk.pp < 0 || sk.maxPP < 0) {
            continue;
        }
        if (sk.pp < sk.maxPP) {
            ++n;
        }
    }
    return n;
}

// 之诗声明的伤害系别 = **次元·龙**（效果文本写死"50点**次元·龙系**伤害"）。
// ⚠️ 这**不是**宿主技能的系别——五把宿主的系别并不一致（敕界归元/告命诗途 = 8 普通，
//    阅世/神忏福音之章/遗典·妄世律裁 = 120 次元龙），而 2088(柱) 挂在**普通系**的告命诗途上
//    却照样声明"次元·龙系伤害" → 印证官方"属性伤害会于效果中**直接声明**系别与点数"，
//    系别是**效果级**声明，不是技能属性。
// 组件 id 取自 `skill_types`：120 = 次元 龙（en = ["dimension","dragon"]）→ 拆成 {17, 15}。
constexpr int kDimension = 17;   // 组件：次元（"dimension"）
constexpr int kDragon = 15;      // 组件：龙（"dragon"）

// ── "附加 {points} 点**次元·龙系**伤害" ─────────────────────────────────────
// 唯一落点（全族口径的收口处）：走 `deal_attribute_damage` 原语
// → 数值 = 点数 × 克制倍数（对象 = 次元·龙 vs 承受方的**系别视图**），**以红伤形式结算**。
void zhi_shi_typed_damage(BattleContext* ctx, int attacker, int target, int points) {
    if (!ctx || !g_core || !g_core->deal_attribute_damage || points <= 0) {
        return;
    }
    // 组件对的**顺序**必须与 skill_types.en 一致（dimension 在前、dragon 在后）。
    // J328（2026-11-06）勘误：本注释曾称"克制表按位置两两相乘"——J322 已把克制聚合
    // 改为**攻守组件全组合倍率的算术平均**（官方口径，场景 7860 三例锁定；任一侧
    // {0,0} 空视图退化为 1.0）。均值对分量顺序不敏感，但组件集本身必须与
    // skill_types 映射一致（拆错组件 = 查错格 → 倍率错），顺序约定照旧保留。
    const int element[2] = {kDimension, kDragon};
    g_core->deal_attribute_damage(ctx, target, points, element, attacker);
}

// ════════════════════════════════════════════════════════════════════
// 2086 渍 —— 「若技能无效，则消除对手回合类效果、能力提升效果，消除成功任意一项则令对手诅咒」
// 注册分支 = SKILL_INVALID（`default_branch_for_effect(2086)`）→ 走到这里即"技能无效"已成立；
// 另一个前提"自己身上必须有空妄诗章"由魂印侧在声明时判（见 vanish_kongyuan.cpp）。
// "消除成功**任意一项**" = 断回合 SUCCESS 或 消强数 > 0，两者取或。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2086_zi(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }

    bool cleared_any = false;
    if (g_core && g_core->break_round_effects) {
        cleared_any |= (g_core->break_round_effects(ctx, target) == BreakResult::SUCCESS);
    }
    if (g_core && g_core->clear_stat_boosts) {
        cleared_any |= (g_core->clear_stat_boosts(ctx, target) > 0);
    }
    if (cleared_any && g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target, static_cast<int>(AbnormalStatusId::Curse),
                              random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2087 镀 —— 「若对手处于控制类异常状态，则己方免疫下2次受到的异常状态」
// "下2次" = **次数型**异常免疫（`grant_immunity` 的 counts 参数；命中一次扣一次、扣到 0 注销），
// 与 effect 1221 同款。不是回合数窗口——用 duration_rounds=0 + counts=2。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2087_du(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!target_has_control_anomaly(ctx, target)) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/2087,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/kAnomalyImmunityCounts, /*source_effect_id=*/2087);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2088 柱 —— 「若对手处于回合类效果，则对手每有1个技能PP值不为满附加50点次元·龙系伤害
//             且对手下回合无法主动切换精灵」
// ★ 2026-09-20 补上"无法主动切换"半边：机制已由莫塔里安线落地
//   （`RuleCategory::SWITCH_LOCK`，纯查询票——`grant_switch_lock` / `is_switch_locked`，
//    查询口在 `battleFsm.cpp` 的主动切换校验，见 `mortarion_skills.cpp` 的 1478 先例）。
//   同族 17 条（1478/1667/1783/1906/1971/1994/2088/2140…）统一走这张票，本次接第 2 条。
//   "**下**回合" → NextRounds 窗口（本回合不算，从下一回合起算）→ 正是"锁住对手下一次
//   操作期的切换"。作用域 ON_STAGE（施放方离场即撤——票随 source_valid_id 走）。
//   窗口 = 1 回合（官方写"下回合"，单数）。
// ════════════════════════════════════════════════════════════════════
constexpr int kZhuSwitchLockRounds = 1;

EffectResult effect_2088_zhu(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!ctx->has_round_effects(target)) {
        return EffectResult::kOk;   // "若对手处于回合类效果"
    }
    const int n = count_skills_with_lost_pp(ctx->getPet(target));
    zhi_shi_typed_damage(ctx, owner, target, n * kTypedDamagePerUnit);
    // 伤害落地后锁切（两个半边同属"若…则…"的同一成立条件，故一起结算）。
    // 票**绑定对手**（binding_side=target）：对手切换/被断回合都失效（2026-09-21 口径）——
    // source_valid_id 传被锁方当前 epoch 快照，断回合 bump 后 invalidate_stale 删票。
    const int start = ctx->round_effect_start_round(owner, kZhuSwitchLockRounds,
                                                   EffectWindowKind::NextRounds);
    ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/2088, target,
                                        kZhuSwitchLockRounds, start, EffectScope::ON_STAGE,
                                        /*source_valid_id=*/ctx->round_effect_valid_id[target]);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2089 烙 —— 「若自身为先出手，则此技能每剩余1点PP值附加50点次元·龙系伤害，
//             下次击败对手后恢复自身全部体力与PP值」
// "若自身为先出手" = 本 handler 被镜像到 FIRST/SECOND 两侧时点，读 currentState 判（同
// `acting_side_is_first`，与技能自身先制高低无关——是**实际出手顺序**）。
// "下次击败对手后恢复自身全部体力与PP值" = 一次性监听：`once=true` 的 watcher。
// ⚠️ 用 **EVENT_DEATH + actor/target 过滤**。J328（2026-11-06）勘误：本注释曾称
//    `EVENT_OPPONENT_DEFEATED`"全项目从未 emit"——J323（2026-11-05）已在击败漏斗
//    补发（battle_primitives.cpp，与 EVENT_DEATH **同点成对**、载荷同口径：
//    actor=击败方、target=被击败方）。本实现**仍用 EVENT_DEATH**：换事件不在
//    验收单范围（watcher 事件语义另审，不能借注释校正顺手换事件），击杀方归因
//    由死亡漏斗携带（场景 071 D 组锁定），卪（vanish_kongyuan.cpp）的
//    "击败对手后"也是同款判据。
// ⚠️ scope 用 TEAM：这是"下一次"的跨回合承诺，期间换宠/被断回合都不该作废它。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2089_lao(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }

    const int pp = selected_skill_remaining_pp(ctx, owner);
    zhi_shi_typed_damage(ctx, owner, target, pp * kTypedDamagePerUnit);

    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.type != EventType::EVENT_DEATH
                || ev.actor != owner || ev.target != 1 - owner) {
                return;
            }
            // 恢复自身全部体力：按**缺口**喂 heal_amount，避免"满血还灌 N 点"的过量语义
            // （走真原语 → 吃封回血/恢复%修正，与"恢复"口径一致）。
            if (g_core && g_core->heal_amount) {
                const int max_hp = max_hp_of(c, owner);
                const int deficit = max_hp - c->getPet(owner).hp;
                if (deficit > 0) {
                    g_core->heal_amount(c, owner, deficit);
                }
            }
            // 恢复自身全部 PP（pp<0 = 无限 PP，跳过；直写是引擎既有写法，见 selection_seal.cpp:635）
            for (Skills& sk : c->getPet(owner).skills) {
                if (sk.maxPP >= 0) {
                    sk.pp = sk.maxPP;
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 2090 均 —— 「附加双方体力上限差值50%的次元·龙系伤害，自身体力上限高于对手时额外吸取对手
//              第五技能剩余的PP值，自身体力上限低于对手时附加伤害翻倍」
// 分支按**双方体力上限比较**（用 `max_hp_of` = numericalBase[HP]；`pet.hp` 是当前体力别名）。
// ⚠️ "吸取对手第五技能剩余的PP值"的**归属**（吸取来的 PP 补给谁）文本没说死：
//    本实现按"**对手第五技能 PP 归零 + 自身第五技能恢复等量**"落（"吸取"= 一来一去），
//    并在场景里锁定这个口径。若实测是"自身全技能各恢复"或"只扣不给"，改这里一处。
// ════════════════════════════════════════════════════════════════════
EffectResult effect_2090_jun(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }

    const int own_max = max_hp_of(ctx, owner);
    const int opp_max = max_hp_of(ctx, target);
    int amount = std::abs(own_max - opp_max) * kDrainPctOfMaxHpDiff / 100;
    if (own_max < opp_max) {
        amount *= 2;   // "自身体力上限低于对手时附加伤害翻倍"
    }
    zhi_shi_typed_damage(ctx, owner, target, amount);

    if (own_max > opp_max) {
        // "额外吸取对手第五技能剩余的PP值"
        auto& opp_skills = ctx->getPet(target).skills;
        auto& own_skills = ctx->getPet(owner).skills;
        if (kFifthSkillSlot < static_cast<int>(opp_skills.size())) {
            Skills& opp_fifth = opp_skills[kFifthSkillSlot];
            const int stolen = opp_fifth.pp > 0 ? opp_fifth.pp : 0;   // 无限 PP(-1) 不参与
            if (opp_fifth.maxPP >= 0) {
                opp_fifth.pp = 0;
            }
            if (stolen > 0 && kFifthSkillSlot < static_cast<int>(own_skills.size())) {
                Skills& own_fifth = own_skills[kFifthSkillSlot];
                if (own_fifth.maxPP >= 0) {
                    own_fifth.pp = std::min(own_fifth.maxPP, own_fifth.pp + stolen);
                }
            }
        }
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
// 时点/分支都由 core 的表决定（skills.cpp 的 effect_register_state / default_branch_for_effect），
// 这里只提供"执行什么"。
SKILL_EFFECT(2086, effect_2086_zi);
SKILL_EFFECT(2087, effect_2087_du);
SKILL_EFFECT(2088, effect_2088_zhu);
SKILL_EFFECT(2089, effect_2089_lao);
SKILL_EFFECT(2090, effect_2090_jun);

}  // namespace
