#ifndef BATTLE_WORKSPACE_H
#define BATTLE_WORKSPACE_H

#include <ostream>
#include <effects/effect.h>
#include <effects/pink_damage_pipeline.h>
#include <entities/numerical-properties.h>

// Forward declare instead of include to break circular dependency
// class BattleContext;

enum class PreemptiveRight {
    SEER_ROBOT_1,
    SEER_ROBOT_2,
    NONE
};

struct DamageSnapshot {
    int attackerId = -1;
    int defenderId = -1;
    int base = 0;
    int afterAdd = 0;
    int afterMul = 0;
    int final = 0;
    int addPct = 0;
    double mulCoef = 1.0;
    bool isRed = true;
    bool isDirect = false;
    bool isFixed = false;
    bool isTrueDamage = false;
    bool isWhiteNumber = false;
    bool isCrit = false;
    // 本次攻击的连击次数（"1回合做 x~y 次攻击"的 N；非连击技能为 1）。
    // `×N` 已经乘进 base/final，这里只作日志与调试用（不参与任何运算）。
    // NSDMI 默认 1；`stage_simple_attack_damage` 每次**显式**赋值，不要依赖默认值。
    int hitCount = 1;
    // ── 属性伤害专用（《赛学必修16—伤害类型》；用户 2026-09-18 口径）──────────
    // 本笔结算是否**跳过"常规挡伤"** = core 默认 BLOCK 回调用 `ImmunityType::DAMAGE` 票
    // （"免疫下N次攻击伤害"一族）把整段归零的那条。
    // 口径：「属性直伤…**常规挡伤不会触发**，只有像岚那种**非本系抵挡**的挡伤才可以」
    //   → 属性伤害置 true：常规挡伤不响应，但**系别条件性**的抵挡要能响应。
    // ⚠️ 只作用于**常规**挡伤（core 那条默认回调）：插件自己注册的 BLOCK 条目
    //    （如岚"免疫非本系技能伤害"）**不看**这个标志，请自行按 `attribute_element` 判——
    //    这正是"只有系别抵挡才可以"的落法。
    bool skip_routine_block = false;
    // 本笔若是**属性伤害**，记它声明的系别（组件对，如 次元·龙 = {17,15}）；否则 {0,0}。
    // 供"非本系抵挡"一类**系别条件**的 BLOCK 条目作判据。
    int attribute_element[2] = {0, 0};
};

/**
 * SkillView - 技能视图槽（带"变威力"触发标记）。**威力**与**连击次数**共用这一个类型。
 *
 * **写入即触发变威力重算**——官方对"威力"的定义就是这个规则本身：
 *   「威力：当命中效果对技能威力值进行修改时，**会以当前状态重新进行伤害结算覆盖原本的伤害值**」
 *   （reference L128《官方名词解释—1》）
 * 关键是触发条件是"威力被**改过**"，而不是"威力变了"：**提升 0 点威力也算变威力**
 * （L453《机制解析—变威力》：「提升0点威力也属于变威力」；L173 同旨"就算是0连击，他也是变威力效果"）。
 * 所以不能做前后值比较，必须记录"有人写过"。连击同理——官方把"n次连击"与"威力提升n%/n点"
 * **并列为变威力效果**（L453），所以写连击数也是"声明这是变威力"。
 *
 * 因此这里不用裸 int + "记得调 helper"：`view[owner] = x` / `view[owner] += x` **自动**置
 * `rewritten`，插件侧零纪律成本（少写一行就是 CLAUDE.md §5.10 那种"改了没反应"的静默失败）。
 * 引擎自己的写入走 `materialize()`（不打标记），见下。
 *
 * 消费路径：`handle_Battle{First,Second}AttackDamage` 在时点桶跑完、伤害管线开跑**之前**
 * 检查（`apply_variable_power_recalc`）——命中就"推翻第一次、以当前状态（含已归零的双防 /
 * 当前能力等级 / 当前克制系数 / 连击次数）重算第二次"，第二次整体覆盖第一次。
 *
 * `value` 语义按槽各自约定：威力视图 **-1 = 未物化**（`calculateDamage` 回退 `skill.power`），
 * **0 是合法值**（强制执行打盔故意置 0 → "只有效果、没有红伤"，用户 2026-09-13 口径）；
 * 连击视图默认 **1**（单段），>1 才算连击。
 */
struct SkillView {
    int value = -1;
    bool rewritten = false;

    // 效果侧写入（`ws.skill_power_view[owner] = x` / `+= x`）→ 自动打"变威力"标记
    SkillView& operator=(int v) { value = v; rewritten = true; return *this; }
    SkillView& operator+=(int v) { value += v; rewritten = true; return *this; }
    // 读点零改动（`int v = ws.skill_power_view[i]` / `== n` / `<< view`）
    operator int() const { return value; }

    // 引擎物化打底（resolve_skill_execution 写 skill.power）：**不是**效果改写，不打标记
    void materialize(int v) { value = v; }
    // 收尾消费：重算之后清标记（每次技能执行在 resolve_skill_execution 末尾也会清一次，防泄漏）
    void consume() { rewritten = false; }
};

inline std::ostream& operator<<(std::ostream& os, const SkillView& v) {
    return os << v.value;
}

/**
 * SkillReplaceSource - 技能替换来源（kExecOnly 载体：ws 描述符）
 *
 * 两级替换拆两个载体，按**生命周期**分家（docs/02-效果系统/技能判定流程与无效效果体系.md §七）：
 * - kExecOnly（艾欧丽娅式，本结构，住在 ws.skill_effect_source）：执行期由效果注入
 *   （ROUND_START 之后的时点），随 ws 每回合 reset 自动失效，"仅本次技能生效"是结构性保证。
 *   只换执行期效果，PP 与 selection_effects_（先制等固有效果）仍走玩家点选的原槽位。
 * - kFull（米修莉式，BattleContext::pending_skill_replacement，住在 context）：必须跨越
 *   "选择期（on_selected）→ ROUND_START(reset) → 执行期（ON_SKILL_HIT）"两个阶段，ws 载体
 *   会在中途被 reset 清掉，故住 context；用后即耗（技能结算完消费）、换宠即清
 *   （invalidate_on_stage_effects，"印记绑定对手，下场不保留"）。kFull 同时剥夺原技能的
 *   固有先制（on_selected 跑在替换技能上，"失去天生先制"），且一切凭证物化都晚于替换
 *   → "没有什么东西可以避开替换技能"。
 *
 * ⚠️ 不要用"改写技能对象内存 + 下个时点还原"的实现：本游戏存在**跳过时点**语义
 *    （星皇之怒），还原可能被跳过 → 替换变永久。两个载体都是"主动消费/自动失效"，
 *    不是"还原"。
 */
struct SkillReplaceSource {
    bool active = false;
    int source_owner = 0;  // 替换技能所在方（0/1；跨精灵替换：艾欧丽娅=施放方）
    int slot = -1;         // 替换槽位（0..4）
};

/**
 * BattleWorkspace - 回合临时数据层
 *
 * 存放本回合内需要用到的中间变量，每回合开始时重置。
 * 所有效果执行都通过 BattleContext.stateEffects 管理，不存在这里。
 */
struct BattleWorkspace {
    //========== 重置策略（2026-09-19，随"Skills 布局 UB"排查一并治理）==========
    // ⚠️ **全成员都有 NSDMI**（默认初值），reset() 用"从全默认实例逐成员拷贝赋值"实现。
    // 旧实现是 `memset(this, 0, sizeof(BattleWorkspace))`：
    //   ① 本类型含非平凡成员（numerical_properties 有自定义构造/析构）→ memset 是正式 UB；
    //   ② 它会把 DamageSnapshot/PinkDamageResolved 的 -1 "无效"哨兵抹成 0（= 合法方 id），
    //     `resolve_owner_from_args` 一类 `>=0` 判定在本回合还没打过伤害时误判
    //     （soul_lib/common.h），preemptive_right 也会被清成 0（= SEER_ROBOT_1 而非 NONE）。
    // NSDMI 之外每回合真正需要"非零默认"的字段在 reset() 尾部循环里显式恢复。
    // 新增字段必须给 NSDMI —— 否则 `BattleWorkspace(kAllDefault)` 模板实例里它是垃圾，
    // reset 后仍是垃圾（旧 memset 时代"新字段忘了恢复"的同款坑，但这次编译期可见性更好）。
    BattleWorkspace() { reset(); }
    void reset() {
        *this = BattleWorkspace(AllDefaultTag{});
        // 恢复非零默认（语义同旧实现；部分字段与 NSDMI 重复，留着做显式口径）。
        for (int i = 0; i < 2; i++) {
            hit_rate_mod[i] = 1.0f;
            crit_rate_mod[i] = 1.0f;
            must_crit[i] = false;
            attribute_must_miss[i] = false;
            cached_crit_damage[i] = 200;  // 默认暴击2倍
            skill_exec_result[i] = SkillExecResult::SKILL_INVALID;
            skill_resolution_flags[i] = SkillResolutionFlags{false, false};
            skill_pp_cost_multiplier[i] = 1;
            restraint_view[i] = -1.0;  // 未设置 → 按元素计算
            // 未物化哨兵：必须恢复 -1，"没物化过就按 skill.power 算"的回退分支才走得到。
            skill_power_view[i].value = -1;
            skill_power_view[i].rewritten = false;
            // 连击次数默认 1（单段）。0 段是非法值 → 必须显式恢复。
            combo_view[i].value = 1;
            combo_view[i].rewritten = false;
            skill_effect_source[i] = SkillReplaceSource{};  // 未替换 → 用本槽位技能
        }
    }

private:
    struct AllDefaultTag {};
    // 全成员 NSDMI → 本构造产物就是"reset 后"的状态；reset() 以它为拷贝模板。
    explicit BattleWorkspace(AllDefaultTag) {}

public:
    //========== 操作选择 ==========
    int roundChoice[2][2]{};     // [方数][操作类型, 参数索引]
    int lastActionType[2]{};     // 上一次操作类型
    int lastActionIndex[2]{};    // 上一次操作参数

    //========== 先手权 ==========
    PreemptiveRight preemptive_right = PreemptiveRight::NONE;
    int preemptive_level[2]{};   // 先制等级，数值越大优先级越高
    int guaranteed_first[2]{};   // 必先等级（0=无；>0 越高越先）。在先手权时点由"必先"回合效果置位，
                                 // 先于先制/速度判定。每回合先手权处 memset 复位。

    //========== 伤害计算 ==========
    DamageSnapshot pendingDamage;   // 加算减伤后
    DamageSnapshot resolvedDamage;  // 最终伤害

    //========== 粉伤结算（独立管线）==========
    // 粉伤不走 resolvedDamage —— 两者输入完全不同（粉伤没有攻击方/技能/克制/暴击）。
    // 每段粉伤（每次 deal_pink_damage）重新填一次并跑一遍 PinkDamagePipeline。
    // 全 POD，`ws.reset()` 的 memset 天然归零。
    PinkDamageResolved resolvedPink;

    //========== 行动开始异常扣血（逐段分档结算）==========
    // 官方 effect_des kind=2 逐条给出时点与档位，语料 idx=472《机制讲解—挡伤/锁伤，miss/异常》归纳为三档
    // （真实百分比 / 百分比 / 固定）→ **逐条独立结算**，不合并成一个总量
    // （同 deal_damage 的"一次调用 = 一段"约定：各自取整、各自过护罩、各自发事件）。
    // 存"异常 id + 已算好的数值"：数值在 stage 期**冻结**（本时点该扣多少就扣多少，
    // 不受随后 ACTION_START 桶里效果改体力上限/解异常的影响）；档位在 settle 期由
    // abnormal_damage_profile(id) 现查（它是不变的数据表，无需求冻结）。
    // ⚠️ 定长数组：加 std::vector 一类非平凡成员会被"逐成员拷贝"的 reset 拷出问题
    //    （旧 memset 时代是野指针）。8 槽够用：行动开始档的异常全表只有 6 条
    //    （中毒/烧伤/冻伤/寄生/流血/混乱）。
    int  action_start_abnormal_damage_ids[2][8]{};
    int  action_start_abnormal_damage_amounts[2][8]{};
    int  action_start_abnormal_damage_count[2]{};
    bool action_start_abnormal_damage_pending[2]{};

    //========== 异常状态相关的"本回合"标志 ==========
    // 砥砺(37)「受到真实伤害后**若本回合未执行过附加异常状态的效果**则增加此伤害值80%的体力」
    // 的条件位：下标 = **被施加方**（"对方对我方执行过附加异常"）。
    // 口径（语料 idx=90/157）："执行过"= 效果**被执行过**（不要求真落地）；**主动毒/被动毒不算**、
    // **来源必须是对方**、第三方效果不算 → 由 `apply_anomaly_impl` 在 **Modern 通道**入口置位。
    // ⚠️ 只放**一个 bool**、不放"真伤数值"槽：官方是"受到真伤后**实时**结算"，
    //    回血量在 `deal_damage(TRUE)` 落地那一刻就可得（见那里的就地钩）。
    // ⚠️ ws 每回合 reset → 天然就是"本回合"语义，无需手动清。
    bool anomaly_applied_by_opponent[2]{};

    //========== 减伤槽位 ==========
    // 官方减伤区顺序（L402）：「**点数减伤——百分比减伤——伤害锁定——伤害免疫**」。
    // REDUCE_FLAT 阶段先扣点数，REDUCE_PCT 阶段再算百分比（加算求和钳 ±100 + 乘算连乘）。
    int damage_reduce_flat[2][4]{};   // **点数**减伤（4槽位，求和后从 final 扣，不为负）
    int damage_reduce_add[2][4]{};    // 加算减伤百分比(4槽位)
    int damage_reduce_mul[2][4]{};    // 乘算减伤百分比(4槽位)

    //========== 临时属性修正 ==========
    float dodge_rate[2]{};         // 闪避率
    // **速度忽略点数**（套装线，2026-09-19 晨曦之星 447）："当回合忽略对手10点速度"——
    // [该方] = 该方的速度在先手权比较中被对手忽略的点数。套装 ROUND_START 节点写入
    // （条件成立时 += 点数），先手权速度比较（handle_BattleFirstMoveRight）消费：
    // 有效速度 = getTempAbilityValue(SPEED) − speed_ignore_points[side]。
    // ws 每回合 reset → 每回合由条件重写（与 damage_add_pct 同套路）。
    int speed_ignore_points[2]{};
    // **防御/特防忽略%**（套装线，2026-09-19 腐蚀者 387）："所有攻击技能忽略对手
    // 防御值和特防值的15%"——[该方] = 该方的防御在**被攻击**时被忽略的百分比。
    // 套装 ROUND_START 节点写入（恒定条款），伤害公式（Calculation::calculateDamage）
    // 消费：Defense × (100 − defense_ignore_pct[defender]) / 100。
    int defense_ignore_pct[2]{};
    float hit_rate_mod[2] = {1.0f, 1.0f};   // 命中率修正倍率
    float crit_rate_mod[2] = {1.0f, 1.0f};  // 暴击率修正（乘算；效果"下N回合暴击率提升"每回合写它）
    // **暴击率加算**（百分点）："{0}回合攻击击中对象要害概率增加1/16"（effect 32 蓄气族）——
    // +1/16 = +6.25 个百分点，是**加法**不是乘算（crit_rate_mod 表达不了"加 1/16"）。
    // 直接参与判定率：rate = base × mod + add；add>0 也是"引爆效果"（base=0 的技能
    // 有了 add 照样能暴）。每回合 reset → 窗口效果每回合重写（同 must_crit 套路）。
    float crit_rate_add[2]{};
    // **技能效果附带的秒杀概率**（千分点；官方 effect 584 族"{0}回合内自己的所有攻击技能
    // 都附有{1}%的秒杀概率"，2026-09-26 湮灭之主·咤克斯 4762 线）：下标 = 攻击方。
    // 由 584 的回合窗口效果每回合重写（与 damage_add_pct 同款"ws reset→回合效果重写"套路），
    // 消费点 = `trait_instant_kill_zero_hook`（红伤落地后）：与特性概率**加法合并成同一个
    // 掷点**（用户拍板"一次攻击判两次秒杀太怪"——2026-09-26 从独立双掷点改并），命中走
    // 一次 `force_hp_to_zero`（秒杀转化/咒怨计层等秒杀族口径由原语与事件自然承接）。
    // ⚠️ 它是**技能效果**不是特性：份额不受亮节降零/proc_forced 管理（特性份额单独判），
    //    也不查 is_trait_suppressed——为什么不并入 trait_state_ 见交接文档
    //    （本质：回合作用域的状态放 ws、长生命周期身份放 trait_state_，两套生命周期别混）。
    // 每回合 reset 归零 → 窗口外自动失效；合计 0 不消耗 rand（既有场景随机序列不变）。
    int instant_kill_permille_bonus[2]{};
    // **必定致命一击**（"下N回合自身攻击技能必定打出致命一击"，effect 58 圣光气）。
    // ⚠️ 为什么不能复用 `crit_rate_mod`：那是**乘算**修正，技能自身 `crit_rate == 0`
    //    时 `0 × 任何数 = 0`——表达不了"必定"（`crit_rate==0` 就是"永不必暴"）。
    // 每回合 reset → 效果用"回合类"在 ROUND_START 重写（"下N回合"的标准套路）。
    bool must_crit[2]{};
    // **属性攻击对自身必定 miss**（"N回合内属性攻击对自身必定miss"，effect 86 圣洁）。
    // [被保护方]；攻击方出手时查 `attribute_must_miss[1 - attacker]`。
    // ⚠️ 是"必定 **miss**"（技能打空），不是"失效"——miss 会照常消费对方的次数类盔
    //    （文档 §2.3），而"失效"走 SKILL_INVALID 补偿分支，两者不可混。
    bool attribute_must_miss[2]{};
    // 本回合效果授予的"必中"凭证（如 2000「对手处于能力提升则先制+1**且必中**」这类
    // **条件必中固有效果**）：效果体在 MOVE_RIGHT 时点置位（与条件先制同一个效果体），
    // `materialize_attack_credential` 再把它并进 `AttackCredential::must_hit`。
    // ⚠️ 为什么必须走 ws 而不是直接改 `skill.must_hit`：改技能对象是**永久**的
    //    （那场仗之后该技能永远必中）；而写在 on_selected 又会被 ROUND_START 的 ws reset 冲掉
    //    —— MOVE_RIGHT 是"reset 之后、出手之前"的唯一正确窗口。
    bool must_hit_grant[2]{};
    //========== 增伤两通道（官方 L352：**通用增伤加法、非通用增伤乘法**）==========
    // 判据是措辞：「造成攻击伤害提升X%」= 通用 → 加法槽；「造成的攻击伤害**额外**提升X%」
    // = 非通用 → 乘法槽。两者落在不同阶段（AMP / AMP_EXTRA），所以 693 排在通用增伤之后。
    float damage_add_pct[2]{};        // 通用增伤·加算百分比（AMP 阶段求和后一次性施加）
    int   damage_add_flat[2]{};       // 通用增伤·固定值
    int   damage_add_extra_mul[2][4]{}; // **非通用增伤·乘算**（AMP_EXTRA 阶段逐槽 final*(100+v)/100）
    // **保底伤害**（"造成的伤害不少于{0}"，effect 447 族）：FLOOR 阶段把红伤抬到至少此值。
    // 攻击技能的效果体在 SKILL_EFFECT 时点**赋值**（覆盖语义——额外行动二次出手时按当次技能重算），
    // 管线跑完即无人再读；打盔/miss 不跑管线，天然不触发（与"打盔不消耗点数减伤"同理由）。
    int   damage_floor[2]{};
    // ⚠️ 全部靠 `reset()` 的 memset 每回合归零 → 回合类效果必须**每回合重写**
    //    （见 effect_set_damage_amp 的套路：注册成 BATTLE_ROUND_START 回合桶效果）。
    numerical_properties battle_attrs[2];        // 本回合视角的数值属性，受到效果修正但不改变真实属性
    int  view_levels[2][6]{};         // 本回合能力提升/下降等级，受到视强为弱、示弱为强效果修正，但是不会改变真实能力上升/下降等级
                                    // 槽位 0..5（**5=命中**，2026-09-18 口径更正；宽度须与
                                    // BattleContext::kAbilityLevelSlotCount 一致）。命中只作用于精度公式，
                                    // 不是 NumericalPropertyIndex 的一项——battle_attrs 的第 6 项是**体力**。
    // **命中等级"视为"修正**（额外加减档，不落 view_levels 本体）：2026-09-20 为
    // 神觉·米斯蒂克 4676 的蚩庸之锁引入——"对手每有 1 层[锁]，自身视为命中等级**额外** -1"。
    // 与 view_levels[.][kAbilityLevelIndexHit] 相加后进 `hit_level_accuracy_pct()`（官方口径：
    // 与命中强化相互抵消——3 层锁恰好被命中+3 抵消；负档共用同一档位表，超出 -6 由表钳 25%）。
    // ⚠️ ws 每回合 memset 归零 → 持有方必须**每回合重写**（ROUND_START 桶，同 battle_attrs 套路）。
    int hit_level_extra[2]{};
    int view_elementalAttributes[2][2]{};

    //========== 伤害抗性有效视图（本回合计算用）==========
    // 伤害计算一律读这里，不直接读 pet.damage_resist——因为临时 buff 可以修改抗性
    // （如混元天尊死亡 buff：己方精灵抗性**被视为 100%**，3 回合后恢复）。
    // 基线由 sync_damage_resist_view 在回合开始 / 换宠时从 pet.damage_resist 重基；
    // 临时 buff 在本回合内直接改视图（回合 reset 后由 buff 效果重新施加）。
    int eff_crit_resist_pct[2]{};     // 暴击伤害抗性%（有效值）
    int eff_fixed_resist_pct[2]{};    // 固定伤害抗性%（有效值）
    int eff_percent_resist_pct[2]{};  // 百分比伤害抗性%（有效值）

    //========== 回合内状态 ==========
    bool has_attacked[2]{};           // 本回合是否已攻击
    bool skill_used[2]{};             // 本回合技能使用标记

    // **命中前全局锚点已发**（EVENT_BEFORE_SKILL_HIT 去重位，2026-09-23）：
    // 一回合内先手方/后手方的 BEFORE_SKILL_HIT 只有**第一次**发锚点事件
    // （battleFsm 两个命中前 handler 开头检查并置位）。ws 每回合 reset → 天然逐回合复位。
    bool before_hit_anchor_fired{};

    //========== 额外行动（通用机制；2026-09-18）==========
    // 官方口径：effect_des 331「精灵的行动结束之后，可以根据效果，进行一次追加的行动，
    //   追加的行动在未声明的场合下不为技能，故而没有威力、类型等技能属性」；
    //   effect_des 317「一回合内，当双方的行动、额外行动均结束之后，战斗阶段结束」。
    // 时点：BATTLE_{FIRST,SECOND}_EXTRA_ACTION（线性序里紧跟同名 AFTER_ACTION_END 之后，
    //   且两个都在 BATTLE_ROUND_END 之前 → 317 的排序要求天然满足）。
    //
    // **内容的载体 = 在该时点注册的效果节点**（沿用"时点桶 = 效果"的既有模型）：
    //   声明方（魂印/技能）把"要执行什么"注册到 BATTLE_*_EXTRA_ACTION 桶，不另设
    //   payload 结构——否则要再造一套生命周期/顺序/过期规则，且与桶的语义重复。
    // **触发开关 = pending**：官方措辞是"**可以根据效果**进行一次追加的行动"，效果是前提，
    //   故没人声明时该时点桶**不跑**（该状态同时是"跳过主流程"——嗑药/换宠/被控/死宠——
    //   的跳转目标，从那条路进来时桶必须空跑，默认 false 天然满足）。
    // 生命周期：ws 每回合 reset 自动清零（声明与消费都在同一回合内完成）。
    bool extra_action_pending[2]{}; // 本回合是否已**声明**一次额外行动（效果侧置位，FSM 消费）
    int extra_action_count[2]{};      // 本回合**已执行**的额外行动次数（FSM 消费时自增；多次上限将来加）

    SkillExecResult skill_exec_result[2] = {SkillExecResult::SKILL_INVALID,
                                            SkillExecResult::SKILL_INVALID};
    SkillResolutionFlags skill_resolution_flags[2] = {{false, false}, {false, false}};
    bool skill_resolution_ready[2]{};
    int skill_pp_cost_multiplier[2] = {{1}, {1}};
    bool skill_pp_cost_consumed[2]{};

    //========== 攻击穿透凭证（本次攻击临时物化，每回合 reset 自动清）==========
    // "下N次攻击无视免疫/伤害限制"的物化槽：攻击时由 materialize_attack_credential
    // 现算合并（技能自带 697/699 + 次数授予），query_usage 门判定读它；回合结束 reset 清除。
    struct AttackCredential {
        bool valid = false;                  // 本次攻击是否携带凭证（穿透或强制）
        bool ignore_attack_immunity = false; // 穿"攻击免疫/狮盔"（官方 699）
        bool ignore_damage_limit = false;    // 穿"伤害限制"（官方 697，本轮只存不消费）
        bool force_execute = false;          // 强制执行：必定命中 + 无视命中效果失效（官方 2380/2474/魂印2260）
        // 本次技能**必定命中**（合成自三处，见 materialize_attack_credential）：
        //   ① `skill.must_hit`（官方 moves.must_hit 固有必中）；
        //   ② `ws.must_hit_grant[owner]`（**条件必中固有效果**本回合授予，如 effect 2000）；
        //   ③ `force_execute`（强制执行隐含必定命中）。
        // ⚠️ 判"是否必中"一律读这个凭证，**不要**只读 skill 里写死的字段——
        //    条件必中类效果是靠 ② 在出手前授予的（用户 2026-09-13 口径）。
        bool must_hit = false;
        int  level = 0;                      // 穿透等级：0=无, 1=可穿盔（等级比较留 SkillInvalidCenter）
    };
    AttackCredential attack_credential[2];   // 按攻击方索引

    // 本次攻击无视护盾/护罩响应（如无极圣武魂印"自身攻击无视护盾承伤效果"）。
    // 效果在 ROUND_START/SKILL_EFFECT 置位；deal_damage 据此跳过对应银行的吸收。
    bool ignore_shield[2]{};   // 按攻击方索引

    // ── 精灵王线（2026-09-24）────────────────────────────────────────
    // 护盾承伤乘区（混地封邪之嶂"每有1层印记护盾承伤值提升10%"）：下标 = 持盾方。
    // deal_damage 吸收时每点盾等效抵挡 (100+pct)/100 点伤害；ws 每回合 reset，
    // 由混地魂印程序按层数重写（damage_add_pct 同套路）。
    int shield_absorb_bonus_pct[2]{};
    // 先制失效区（火电 2479"对手体力低于X%则所有技能先制效果失效"）：下标 = 被失效方。
    // 与束缚(28)同语义（battleFsm 先制结算点清零先制），但它是**非异常**条件区，
    // 由技能效果按条件重写；束缚失效不受它影响（各自独立清零一次，幂等）。
    bool priority_nullified[2]{};
    // 当回合该方是否受到过攻击伤害（水王 2343 战斗阶段结束分支的检测旗，2026-09-24）：
    // 由 EVENT_TAKE_DAMAGE watcher（魂印侧）置位、BATTLE_ROUND_END 节点消费后清零。
    bool nutao_took_attack_damage[2]{};
    // 混地 2411（2026-09-24）：破盾补偿 pending 旗（下次受攻伤减 50%）与
    // "回合开始无提升 → 阶段末吸取"分支旗。均由魂印程序读写，ws reset 自动清。
    bool diwang_pending_reduce[2]{};
    bool diwang_phase_drain_arm[2]{};
    // 圣光莫妮卡 2492 最优系别视图（K7）：下标=攻击方。每回合由魂印程序按剩余
    // 回合数重写（ws reset 套路）；伤害 snapshot 处消费（候选取最优 → restraint_view）。
    int optimal_elem_rounds[2]{};
    // 圣光莫妮卡 2492 非同系攻击免疫门（2026-09-25 待落①，用户口径）：下标=**被攻击方**。
    // true → 攻击方**攻击技能**（属性技能不进门）的系别（skill_element_view，ON_SKILL_HIT
    // 物化点已刷）∉ 本方系别集（view_elementalAttributes）→ 技能无效（SEALED，走补偿趟）。
    // 本质是"可穿盔"：攻方凭证 valid（穿透/强制执行）照样穿（skills.cpp 门判定处消费）。
    // ⚠️ ws 每回合 reset → 魂印 ROUND_START 按"律>0"重写（圣莫 soul 程序；换宠当回合
    //    不随下场解除，与最优系别同款近似，翻案则加 EVENT_SWAP 撤销）。
    bool nonnative_immune_armed[2]{};
    // 圣光格劳瑞 2532（2026-09-24）："当回合对手免疫过或自身受到过麻痹"检测旗
    //（阶段末分支消费，wl 魂印 watcher 置位、ROUND_END 清）。
    bool gla_parity_flag[2]{};

    // 最近一次恢复的实际体力值（按目标索引；封回血/恢复效果修正后的值）。
    // heal 原语写入；吃月亮二类（按实际恢复值）效果读取。
    int last_heal_amount[2]{};

    //========== 裸伤台账（按攻击方索引；2026-09-18，effect 422 族）==========
    //
    // 一次攻击在内存里产生**三个不同的"伤害值"**，别混（effect 422「附加所造成伤害值{0}%的
    // 固定伤害」实测暴露的坑，用户 2026-09-18 口径）：
    //   ① **裸伤** = 本字段。公式值（含暴击/浮动/连击/次数攻击增伤）+ **变威力重算后的
    //      第二次结果**，**不含伤害管线任何阶段**——增伤/减伤/保底/锁伤/挡伤都不进。
    //      实测口径："挡伤、锁伤所改变的红伤不是被这个效果记录的"。
    //   ② **管线后** = `resolvedDamage.final`（增伤/减伤/保底/锁伤/挡伤都已作用）。
    //      "按伤害比例吸血"族（101/1256）读的是这个——**两族不要混用同一个值**。
    //   ③ **实际落血** = `BattleEvent::EVENT_TAKE_DAMAGE` 的 amount（再扣掉护盾/护罩吸收），
    //      死亡归因 `last_damage_actor` 也按它写。
    //
    // ★ 唯一写入点 = `finish_attack_damage` 里 **`apply_variable_power_recalc` 之后、
    //   `damage_pipeline_.run` 之前**。为什么必须在那儿：
    //     · 变威力会**重跑一遍公式**（`apply_variable_power_recalc` 内部调
    //       `stage_simple_attack_damage`）并覆盖第一次结果 → 写早了记到的是被推翻的值；
    //     · 管线一跑 `final` 就被逐阶段改写 → 写晚了记到的是管线后的值。
    //   挪动这一行 = **改机制定义**（不是实现细节），动之前先做游戏内对照实验。
    // ⚠️ 每次**攻击尝试**开头清零（含被盔/被威的无效出口）→ 无效的那次攻击读不到上一击的残留值。
    // 生命周期：ws 每回合 reset 自动清零；按攻击方索引（同回合双方各一击互不覆盖）。
    int raw_attack_damage[2]{};

    //========== 技能威力视图层 ==========
    // 本回合视角的技能威力：攻击时由 resolve_skill_execution 物化 skill.power，
    // 效果（如无相谛 179"属性相同威力提升"、未来黯玉咒言随机/累积威力）在
    // SKILL_EFFECT 时点修改它，ATTACK_DAMAGE 阶段 calculateDamage 从 ws 读最终值。
    // ⚠️ **-1 = 未物化**（calculateDamage 回退 skill.power）；**0 是合法值**——强制执行打盔时
    //    故意置 0 来"只有效果、没有红伤"（用户 2026-09-13 口径）。
    // ⚠️ 类型是带标记的 `SkillView`：**效果写它 = 声明"这是变威力"** → ATTACK_DAMAGE
    //    收尾据此重算第二次（详见结构体注释）。读点因 `operator int()` 无感。
    SkillView skill_power_view[2];

    //========== 连击次数视图层（"1回合做 x~y 次攻击"）==========
    // 本次技能的连击次数 N：resolve_skill_execution 按 `Skills::roll_combo_count()` 掷一次
    // （**每次技能使用掷一次**，第一次/第二次结算共用同一个 N），效果可再加。
    // 伤害是"**一次公式 × N**"（不是 N 次独立结算），`×N` 打在 base 上 →
    // 增伤/减伤/护盾/次数型免伤全部作用于**总数**。默认 1（单段）。
    // ⚠️ N > 1 本身就是"这是变威力技能"（官方把"n次连击"列为变威力描述）→ 触发第二次结算，
    //    否则 ×N 吃到的是破防前的能力等级（连击就享受不到暴击破防）。
    SkillView combo_view[2];

    //========== 技能替换：kExecOnly 载体（艾欧丽娅式，见 SkillReplaceSource 注释）==========
    // "执行什么技能"的唯一取用点是 battleFsm.cpp 的 resolve_executing_skill；
    // active=false（默认）→ 用玩家点选的本槽位技能。技能对象不被拷贝，只重定向取用点。
    SkillReplaceSource skill_effect_source[2];

    //========== 技能元素/克制倍率视图层 ==========
    // 攻击结算视角的技能系别：默认物化 skill.element，效果可改
    // （"以XX系别计算克制倍数"类效果写这里）。克制倍率计算用它 vs 防御方元素；
    // 本系加成(involve) 仍用技能真实系别 skill.element（改系别只改克制、不改本系）。
    int skill_element_view[2][2]{};

    //========== 技能类别视图层（物理/特殊/属性）==========
    // 攻击结算视角的技能类别：resolve_skill_execution 与威力/连击/系别视图**同点物化**
    // skill.type（SkillType）。消费方：通用特性「精神」（特攻增伤门控）、「瞬杀」
    // （进攻类技能门控）。⚠️ ws.reset() 的 memset 归 0（=Physical）；每次攻击都会
    // 重新物化，读点全在攻击结算内部（ON_SKILL_HIT 之后、ATTACK_DAMAGE 管线里）。
    int skill_type_view[2]{};

    // 克制倍率视图：>=0 直接用作本次攻击克制倍率（"不会出现微弱"钳到1、
    // "不计算克制"设1、固定倍率直写）；<0 未设置 → 按 skill_element_view vs
    // 防御方元素计算。reset 须显式恢复 -1.0（memset 会清成 0）。
    double restraint_view[2] = {-1.0, -1.0};
    // "攻击时不会出现微弱"（effect 760）：760 置位后，本次克制若 <1（微弱）→ 钳到 1（普通），
    // 克制（>1）保持克制（区别于硬设 restraint_view=1 会连克制也削）。伤害公式在 restraint 算好后判断。
    bool no_weakness[2]{};

    // "以指定系别进行伤害结算"的**选择期授予**（effect 2490 一族）。
    // ★ 为什么是"授予"而不是直接写 `skill_element_view`：选择期效果跑在 **MOVE_RIGHT**，
    //   而 `resolve_skill_execution` 在 **ON_SKILL_HIT** 会把 `skill_element_view` **物化覆盖**成
    //   `skill.element` —— 直接写视图会被冲掉（与 `must_hit_grant` 同构的教训）。
    //   物化时若授予有效则用授予值打底；SKILL_EFFECT 时点的效果仍可在此基础上再改。
    int  skill_element_grant[2][2]{};
    bool skill_element_grant_valid[2]{};

    //========== 链首受击快照（GUARD_DETECT 阶段，增伤**之前**）==========
    // 由 core 的默认回调（`install_default_damage_guard_detect`）在伤害管线**第一位**写入：
    // 本次攻击打出的红伤值——**已含暴击与暴击抗性减免，未含任何增伤/减伤**。
    // 用途：「受高伤/受低伤」一族魂印（不灭地威·萨瑞卡 1217「自身受到的攻击伤害大于350则恢复自身
    // 所有体力，小于350则反弹…」）。它们必须取**增伤前**的值——否则链尾的增伤（693 的 AMP_EXTRA）
    // 会把实际伤害顶过阈值而魂印看不到 = 经典现象「**693 增伤乱穿犀牛**」。
    // ⚠️ 按**防御方**索引；写值只在"自己是本次攻击的防御方"时发生，所以自己当攻击方的那一趟
    //    不会覆盖自己（同一回合双方各攻击一次，两边互不干扰）。
    // ⚠️ `happened` 区分「本回合没被攻击」与「被打了但红伤为 0」——memset 默认 false。
    int  guard_detect_damage[2]{};
    bool guard_detect_happened[2]{};

    // 本次攻击**强行获得本系加成**（"获得本系属性加成"，effect 2490）。
    // ⚠️ `calculateDamage` 里 `involve()` 读的是**技能真实系别 `skill.element`**（刻意的设计：
    //    改系别只改克制、不改本系）→ "改了系别就自动吃本系加成"是**拿不到**的，故另开这个显式开关。
    //    选择期（MOVE_RIGHT）写、伤害公式读；ws 每回合 reset 天然清空。
    bool force_involve[2]{};

    //========== 命中效果失效标记（③层） ==========
    // 消费点已收口到 `Skills::query_usage` ②.5（按技能类型分别消费 RuleCenter 的两类条目），
    // 结果模式记在这里供执行期读：
    //   - `hit_invalid_mode`：本次失效的模式（HitInvalidMode；-1 = 无失效）。
    //     execute 据此置 `hit_invalid_zero_damage`。
    //   - `hit_invalid_zero_damage`：白板模式（kFullNull）→ ATTACK_DAMAGE 阶段把伤害归 0。
    //     kEffectsOnly（保留伤害）不置位。
    // 每回合 reset 自动清。
    int  hit_invalid_mode[2]{};        // 默认 0 = kEffectsOnly；只在失效时被写
    bool hit_invalid_detected[2]{};    // 本次是否发生 ③层失效（区分"模式=0"与"没失效"）
    bool hit_invalid_zero_damage[2]{};

    //========== 缓存计算值 ==========
    int cached_speed[2]{};            // 考虑异常后的速度
    int cached_crit_damage[2] = {200, 200};      // 暴击伤害倍率(默认200)

    // 取"本回合视角"的属性值（含能力等级修正）。**只用于 battle_attrs 的 6 项属性**
    // （攻击/特攻/防御/特防/速度/体力），不是"能力等级"的读取口——等级槽 5 是**命中**
    // 而属性槽 5 是**体力**，两套索引只在 0..4 重合（见 battleContext.h 的长注释）。
    int getTempAbilityValue(int owner, NumericalPropertyIndex i) const {
        if (owner < 0 || owner >= 2) {
            throw std::out_of_range("Owner index out of range");
        }
        auto &level = view_levels[owner];
        int index = static_cast<int>(i);
        if (index < 0 || index >= 6) {
            throw std::out_of_range("Index out of range");
        }
        // ⚠️ **体力（HP）没有能力等级**：官方能力提升状态只有 6 种（双攻双防速命中），
        //    不含体力。原实现在这里读 `view_levels[owner][5]`，而槽 5 是**命中** →
        //    等价于"体力值被命中等级缩放"，是纯粹的口径错误。
        //    好在**从无人以 HP 调用本函数**（调用点只有 skill.type / skill.type+2 / SPEED /
        //    SPECIAL_ATTACK），所以它一直是死分支——但仍必须堵上，免得将来有人踩。
        if (i == NumericalPropertyIndex::HP) {
            return battle_attrs[owner][NumericalPropertyIndex::HP];
        }
        if (level[index] < -6 || level[index] > 6) {
            throw std::out_of_range("Level out of range");
        }
        if (level[index] >= 0) {
            return static_cast<int>(battle_attrs[owner][i] * ((level[index] + 2) / 2.0));
        }
        // 负等级：官方 2/(2-|level|)，展开即 2/(2-level)。
        // ⚠️ 原写法是 2/(2+level)：-1 会算成 ×2（应当 ×0.67，方向还反了），
        //    **-2 直接除零** → int 溢出成 INT_MAX（红伤一击 5 亿，见场景 032 踩坑记录）。
        // ⚠️ 原先这里还有一段 `if (index == 5 && level[index] < 0)` 返回 85/70/55/45/35/25 的
        //    "命中等级为负时的特殊处理"——那是**命中率档位表**，误放在了按属性索引取值的函数里。
        //    2026-09-18 已搬到 abnormal-types.h 的 `hit_level_accuracy_pct()`（精度公式用它）。
        return static_cast<int>(battle_attrs[owner][i] * (2.0 / (2 - level[index])));
    }
};

#endif // BATTLE_WORKSPACE_H
