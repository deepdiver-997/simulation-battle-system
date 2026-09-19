#ifndef BATTLE_PRIMITIVES_H
#define BATTLE_PRIMITIVES_H

#include <effects/effect.h>
#include <effects/rule_center.h>  // SealKind / SkillInvalidNotifyResult / EffectScope
#include <effects/spirit_lifecycle.h>  // 存活/死亡/消逝 三态 + DeadScope + DefeatCause

class BattleContext;

// ================================================================
// 原语层 — 技能/魂印效果程序调用的原子动作
//
// 原语是闭集：增长慢、每个值得审。原语用"结果枚举"表达发生了什么，
// 技能据此分支后续行为（如"清除成功则下回合先手+1"）。
//
// 返回值约定：
//   - 状态变更类原语 → 结果枚举（区分不同失败/成功变体）
//   - 谓词/查询 → bool（is_immune、has_round_effects 留在内核）
//   - 发射即忘 → void（emit、grant_immunity）
// ================================================================

// ----------------------------------------------------------------
// 施加异常
// ----------------------------------------------------------------
enum class ApplyAnomalyResult {
    SUCCESS,               // 成功施加
    TARGET_IMMUNE,         // 目标免疫异常
    BLOCKED_BY_EFFECT,     // 被其他效果阻止（装备/场地/特定保护）
    TARGET_DEFEATED,       // 目标已死亡
    INVALID_PARAM,         // 无效参数（anomaly_id 非法、target 非法）
    REPLACED_EXISTING,     // 替换了已有的控场类异常
    DURATION_EXTENDED,     // 同种异常已存在，延长了回合数
    RESISTED_BY_RESISTANCE,// 异常抗性抵抗成功：直写附加"免疫异常"异常(21, 2回合)，击穿魂免
    REFLECTED,             // 弹控：目标免疫并将异常反弹给施放方（最多反弹 1 次）
    CONVERTED,             // 转化异常：进入异常时转为另一指定异常
};

// 异常施加通道（2026-09-16 双通道口径，用户拍板 + 语料《浅谈主动毒与魂印免控判定》）。
//
// 数据库证据：施加模板分两代——古早"写死异常名"族（effect 10/11/12/14/15「命中后{0}%令对方XX」
// + 114 易燃；通用特性 Eid 6/66/67 的接触施加同属此代，语料称"主动毒/被动毒"）与现代
// 参数化族（"「{0}%令对手{1}」×100+ 条模板，{1}=battle_effects 异常码，如月下伏 2189）。
// 两代在官方运行时走不同施加例程 → 对免疫/抗性/转化的可见性不同：
//   Modern（低级控）：全检查链响应（次免→抗性→魂免→弹控反弹→转化）；
//   Ancient（高级控=主动毒）：**只被 ImmunityTier::Ancient（古代层）免疫挡下**；
//     无视一切现代免疫（含全部弹控）、异常抗性、转化异常。
// 官方侧无任何字段区分这两代（施加路径是实现期行为，不在数据里）→ 通道由**施加方
// 效果的实现**选择调用哪个原语来表达，这是认证数据层的解释，不是官方数据的事实。
enum class AnomalyChannel {
    Modern = 0,   // 现代施加（参数化模板族，主流）：全检查链
    Ancient = 1,  // 古早施加（"命中后令对方XX"族 + 特性主动毒 = 主动毒）：只被古代层免疫挡
    Raw = 2,      // 遗留裸施加（特性被动毒专用，2026-09-16 用户拍板）：**什么都不检测**——
                  // 免疫（含 Ancient 层/老魂免 Mark 0）/弹控/抗性/转化全穿，直接落地。
                  // 比 Ancient 更早的遗留机制（带电/高热/冰冷/阴森"受到普通攻击时"）。
};

/**
 * 生成异常状态的随机持续回合数。
 * 绝大多数异常状态持续 2~3 回合（具体区间待确认）。
 */
inline int random_anomaly_duration() {
    return 2;
}

/**
 * apply_anomaly - 现代异常施加原语（AnomalyChannel::Modern）。
 * 全系统现代施加的唯一入口，返回"发生了什么"。
 * 检查链（全序）：次免/回合类免疫 → 弹控反弹 → 异常抗性 → 魂免 → 转化 → 落地。
 *
 * 成功（异常状态实际改变）时 emit EVENT_ANOMALY_APPLIED；
 * 若是控场类异常，额外 emit EVENT_CONTROLLED（第三方 watcher 监听）。
 *
 * @param actor 施放方（0/1），效果程序调用时传效果所属方；未知传 -1
 */
ApplyAnomalyResult apply_anomaly(BattleContext* ctx,
                                 int target,
                                 int anomaly_id,
                                 int duration_rounds = -1,
                                 int actor = -1);

/**
 * apply_anomaly_ancient - **古早异常施加原语**（AnomalyChannel::Ancient，主动毒）。
 * 古早"命中后{0}%令对方XX"模板族（10/11/12/14/15/114）与通用特性接触施加
 * （Eid 6/66/67，带电/高热/冰冷/阴森/静电/颤栗/火热/极寒）专用入口。
 *
 * 与 apply_anomaly 的差异（2026-09-16 口径，用户拍板 + 语料主动毒文章）：
 *   - 免疫只查 **ImmunityTier::Ancient** 条目（次免/魂免两段都查，Mark 0 老魂免兜底
 *     属古代层照常生效）；现代层免疫（含官方次数型"免下N次"——暂按 Modern，待实测）
 *     与**一切弹控**对古早施加不可见 → 不会反弹；
 *   - **跳过异常抗性**与**转化异常**（主动毒官方口径）；
 *   - 返回值永不出现 REFLECTED / RESISTED_BY_RESISTANCE / CONVERTED。
 *
 * @param actor 施放方（0/1），效果程序调用时传效果所属方；未知传 -1
 */
ApplyAnomalyResult apply_anomaly_ancient(BattleContext* ctx,
                                         int target,
                                         int anomaly_id,
                                         int duration_rounds = -1,
                                         int actor = -1);

/**
 * apply_anomaly_raw - **遗留裸施加原语**（AnomalyChannel::Raw，特性被动毒专用）。
 * 带电/高热/冰冷/阴森（Eid 6 家族，"受到普通攻击时有 n% 使对方XX"）专用入口。
 *
 * 与前两个原语的差异（2026-09-16 用户拍板）：**什么都不检测**——免疫（含 Ancient 层/
 * Mark 0 老魂免）/弹控/异常抗性/转化异常全部穿透，校验参数与目标存活后直接落地
 * （同种异常仍按"回合长者覆盖"合并、成功路径照常 emit 事件）。返回值只会是
 * SUCCESS / DURATION_EXTENDED / TARGET_DEFEATED / INVALID_PARAM。
 *
 * @param actor 施放方（0/1），被动毒传异常来源方（守方）；未知传 -1
 */
ApplyAnomalyResult apply_anomaly_raw(BattleContext* ctx,
                                     int target,
                                     int anomaly_id,
                                     int duration_rounds = -1,
                                     int actor = -1);

/** 便捷函数：尝试施加异常，成功返回 true。 */
inline bool try_apply_anomaly(BattleContext* ctx,
                              int target,
                              int anomaly_id,
                              int duration_rounds = -1,
                              int actor = -1) {
    ApplyAnomalyResult r = apply_anomaly(ctx, target, anomaly_id, duration_rounds, actor);
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ----------------------------------------------------------------
// 异常自然结算点（到期衍化 + 到期/回合扣减点伤害 + 过期回写）
// ----------------------------------------------------------------

/**
 * tick_abnormal_statuses - **异常自然结算点**：在"回合扣减点"跑一遍异常的三部曲。
 *
 * 为什么需要它：引擎的异常过期一直是**纯读比较**（`roundCount < end_round`，从不清槽），
 * 于是"异常到期的那个瞬间"根本不存在 → 官方衍化（"焚烬结束后转化为烧伤"）**无处落脚**。
 * 本函数补上这个时点，顺序依官方 idx=59「回合结束时**先转化异常，再回合扣减**」与
 * idx=429《时点表》「异常沉默，束缚，烈焰诅咒均在该时点结算」：
 *
 *   ① **回合扣减点档伤害**：仍生效中的"每回合结束后"异常（沉默 30 / 烈焰诅咒 24）；
 *   ② **到期项**（`end_round <= roundCount`，本回合起已不再生效的那些）：
 *      ②a "…结束时"档伤害（束缚 28）；
 *      ②b **衍化**（`abnormal_derivation`）——直接写转出异常的 `end_round` + 附带弱化
 *          （`stat_drop_piercing`，**无视免弱**）：冰封→冻伤+速度-1、焚烬→烧伤+命中-1、
 *          诅咒→随机（烈焰诅咒/致命诅咒/虚弱诅咒）、感染→中毒+攻击&特攻-1、超频→1~2 回合瘫痪；
 *   ③ **过期回写**：把 `end_round <= roundCount` 的槽全部清 0
 *      —— 顺手修掉"过期不回写 → `end != 0` 判'有异常'是假阳性"的历史问题。
 *
 * ⚠️ 衍化**不走 `apply_anomaly`**：那是"新施加"，会重跑免疫/抗性/转化/弹控全链，而官方语义是
 *    "同一异常改变了形态"（idx=59「精灵还是算作处于异常，可以正常触发某些'处于异常则xx'效果」）。
 *    写成 apply_anomaly 会让"冰封→冻伤"被目标的冻伤免疫挡掉，与官方相反。
 *
 * ⚠️ `abnormal_status_end_round` 本身是 **on-stage 作用域**（换宠已清），故只处理在场精灵即可。
 *
 * @return 本次实际衍化出的异常条数（0 = 无衍化）
 */
int tick_abnormal_statuses(BattleContext* ctx, int target);

// ----------------------------------------------------------------
// 断回合
// ----------------------------------------------------------------
enum class BreakResult {
    SUCCESS,      // 清除了对手回合类效果（成功路径 emit EVENT_BREAK）
    NO_EFFECTS,   // 对手没有可清除的回合类效果
    IMMUNE,       // 对手免断，本次断回合无效
};

/**
 * break_round_effects - 断回合原语：清除目标回合类效果，返回"发生了什么"。
 * 技能可按结果分支："清除成功则XXX"（SUCCESS）、"对手免疫则XXX"（IMMUNE）。
 */
BreakResult break_round_effects(BattleContext* ctx, int target);

// ----------------------------------------------------------------
// 伤害
// ----------------------------------------------------------------
enum class DamageKind {
    NORMAL,        // 普通攻击伤害（红伤；吃护盾）
    FIXED,         // 固定伤害（粉伤·固定档；吃护罩/固定抗性）
    PERCENT,       // 百分比伤害（粉伤·百分比档；amount 是"占目标最大体力的百分比"）
    PERCENT_VALUE, // 百分比伤害·指定数值（粉伤·百分比档；amount **已是具体伤害值**，
                   //   不再按目标最大体力换算）——"附加自身已损失体力50%的百分比伤害"
                   //   （谱尼能量刻印）这类"值由来源算出、但走百分比抗性/护罩"的效果用。
    TRUE,          // 真实伤害（护盾护罩都不响应）
    // **属性伤害**（《赛学必修16—伤害类型》的独立一类）：由**属性技能**造成、声明系别与点数、
    // 数值 = 点数 × 克制倍数。走**红伤**落血（emit EVENT_TAKE_DAMAGE、进死亡归因），
    // 但**护盾/护罩不响应它**（用户 2026-09-18 口径："属性直伤其实不会被护盾/护罩抵挡"）。
    // ⚠️ 它**不是**真伤：真伤"无法减免"，属性伤害仍吃**锁伤**与**系别条件性抵挡**
    //    （见 `deal_attribute_damage` 的阶段子集）。单独列一档就是为了让"不吃护盾"与
    //    "不是真伤"这两件事各自可读，不要图省事借用 FIXED/TRUE。
    ATTRIBUTE,
};

/**
 * deal_damage - 伤害原语：统一伤害入口。
 *
 * 流程（粉伤）：PERCENT 换算 → **PinkDamagePipeline**（增粉/抗性/特效免减/上限/护罩/检测，
 *   见 effects/pink_damage_pipeline.h）→ 剩余 > 0 才扣血 → emit EVENT_TAKE_PINK_DAMAGE
 *   + EVENT_TAKE_DAMAGE。
 * 流程（红伤/真伤）：护盾吸收（按优先级，真伤直通）→ 扣血 → emit EVENT_TAKE_DAMAGE。
 * 护盾/护罩被击破时 emit EVENT_SHIELD_BROKEN。
 * 所有伤害类机制（攻击管线、效果、固定/百分比伤害）都应走这里，
 * 避免效果函数直接改 hp 绕过管线。
 *
 * ⚠️ **一次调用 = 一段**："1回合做N次固定伤害"是 N 次调用、逐段独立结算（各自取整/各自扣护罩/
 *    各自发事件），不是把 N 段合并成一个总量——官方算例 L445 逐段取整，合并会算错。
 *
 * @param target 承受方 (0/1)
 * @param amount 伤害量；PERCENT 时为占最大体力的百分比
 * @param kind   伤害类型
 * @param actor  施放方（未知传 -1）
 */
void deal_damage(BattleContext* ctx, int target, int amount,
                 DamageKind kind = DamageKind::NORMAL, int actor = -1);


// ----------------------------------------------------------------
// 技能拦截（封属性/封攻击）
// ----------------------------------------------------------------

/**
 * seal_skill - 给"目标方(target)"挂技能拦截效果（封属性/封攻击）。
 *
 * "对手下N次属性技能失效"（次数型）/"对手3回合内属性技能无效"（回合型）类效果。
 * 统一语义：**挂到施放方(source)**（决定生命周期——施放方换宠清、断施放方回合解封），
 * 生效对象 target=被封方。覆盖键 (source, effect_id, kind) 刷新不叠加。
 * 拦截发生时技能按 SKILL_INVALID 处理。回合型每回合递减、可被断回合清除。
 *
 * @param source         挂载/施放方 (0/1)（生命周期锚）
 * @param target         生效/被拦截方 (0/1)（谁的技能使用被无效）
 * @param effect_id      来源效果 id（覆盖去重 key）
 * @param attribute      是否封属性技能
 * @param attack         是否封攻击技能
 * @param count          次数型拦截次数（>0）
 * @param duration_rounds 回合型持续回合（>0 走回合型；0=次数型）
 * @param penetrable     可否被"无视攻击免疫"穿透（默认 true=可穿盔；false=条件盔/龙威）
 * @param source_slot    施放方精灵槽位（施放方换宠清理用；-1=未知）
 * @param scope          ON_STAGE=施放方换宠清 / TEAM=全队保留
 * @param hit_invalid    命中失效语义（默认 false）：true 且 attribute 时用 SealKind::SEAL_ATTRIBUTE_HIT ——
 *                       只封属性技能，但技能**照常命中、效果失效、不触发 SKILL_INVALID 补偿**。
 *
 * 注：拦截**没有**"免断"属性——免断是 owner 级的 `ImmunityType::BREAK`（免疫内核），
 *     由 `break_round_effects` 在入口统一查询，见技能判定流程与无效效果体系.md §二。
 */
// @return **授予句柄**（source_id）。"盔生效/被穿"事件带上它 → 监听器据此精确匹配自己那条盔
//         （同 effect_id 的多条盔靠它区分；被穿的盔要能用这个句柄自删监听器）。0 = 参数非法。
int seal_skill(BattleContext* ctx, int source, int target, int effect_id, bool attribute, bool attack,
               int count, int duration_rounds = 0, bool penetrable = true,
               int source_slot = -1, EffectScope scope = EffectScope::ON_STAGE,
               bool hit_invalid = false, int chance_pct = 100,
               bool consumed_when_pierced = false);

/**
 * hit_effect_invalid - 给目标方挂"命中效果失效"（③层，次数类）。
 *
 * 目标方后续技能命中时，命中效果按 mode 处理（效果不注册）：
 *   - kEffectsOnly：保留伤害（效果失效但伤害照常）
 *   - kFullNull：白板（效果失效 + 伤害归 0）
 * 强制执行（force_execute）可绕过③层。
 *
 * ⚠️ **按技能类型分两类**（用户 2026-09-13 口径）：命中失效**不是属性技能专用**，
 *   攻击技能同样会被失效 → RuleCenter 里拆成 `HIT_INVALID_ATTACK` / `HIT_INVALID_ATTRIBUTE`。
 *   本次是攻击技能就消费攻击那条、属性技能就消费属性那条。
 *   **要"两种技能都失效"就调两次**（一次 is_attribute_skill=false、一次 true），
 *   不是给一条加"通配"。
 *
 * @param target             被失效方 (0/1)
 * @param mode               失效模式（kEffectsOnly / kFullNull）
 * @param count              失效次数（>0）
 * @param is_attribute_skill true=只对**属性技能**生效；false=只对**攻击技能**生效
 * @param source_id          施放方（未知传 -1）
 */
void hit_effect_invalid(BattleContext* ctx, int target, HitInvalidMode mode,
                        int count, bool is_attribute_skill, int source_id = -1);

// ----------------------------------------------------------------
// 能力等级变化
// ----------------------------------------------------------------
enum class StatChangeResult {
    SUCCESS,       // 成功变更
    AT_CAP,        // 到上限/下限（等级越界，未变更）
    INVALID_PARAM, // 无效参数（target/stat 非法）
    // 目标处于"**无法处于能力提升状态**"（ImmunityType::STAT_BOOST：混沌魔君索伦森 1011
    // 的 2 回合封锁）→ 本次**提升**整次失败、等级一点不动。只挡 delta>0 的方向。
    BLOCKED,
};

// 弱化原语的返回（见 stat_drop）。
enum class StatDropResult {
    SUCCESS,       // 成功弱化（含"部分生效"：压到 -6 为止）
    AT_FLOOR,      // 已在 -6，无法再降（等级不变）
    IMMUNE,        // 目标免疫弱化（STAT_DROP）→ 整次失败，等级不变
    INVALID_PARAM, // 无效参数（target/stat 非法 / amount<=0）
};

/**
 * stat_change - 真实能力等级变更（pet.levels，持久；视层留 ws）。
 * 返回"发生了什么"，效果程序据此分支（如"提升失败则附加固定伤害"）。
 *
 * @param target 目标方 (0/1)
 * @param stat   能力下标（0=攻击 1=特攻 2=防御 3=特防 4=速度 5=体力 **6=命中**）
 *               —— **5 = 命中等级**（官方能力提升六项之一，2026-09-18 口径更正）：
 *               只作用于精度公式（skills.cpp 的 query_usage ①）；`NumericalPropertyIndex`
 *               的第 6 项是**体力**（属性值，不是等级），两套索引只在 0..4 重合。
 * @param delta  变化量（正=提升，负=下降；越界则 AT_CAP 不变更）
 *
 * ⚠️ **不查免弱（STAT_DROP）**——自身增益/原始变更用它；"对手施加的弱化"用 stat_drop。
 * ⚠️ 但**查 STAT_BOOST**（"无法处于能力提升状态"）：delta>0 且目标被封锁 → BLOCKED、
 *    等级一点不动。查的是"往上抬"的方向，往下压（delta<0）不受影响。
 * ⚠️ 本函数是所有"等级往上/往下改"的原语写入路径之一，调用它会 emit
 *    `EVENT_STAT_CHANGED`（本体/视图同步 + 监测级的事件）——**不要绕过它直写
 *    `ctx->ability_levels`**，否则监测能力等级变化的效果（索伦森 1011 的压制 / 后续
 *    "对手强化时…"一族）看不到这次变化。
 */
StatChangeResult stat_change(BattleContext* ctx, int target, int stat, int delta);

/**
 * stat_drop - **弱化原语**：降低目标的某项能力等级（"令对手攻击-2"之类）。
 *
 * 与 stat_change 的分工：
 *   - stat_change = 原始等级变更（自身增益 / 弱化之外的场景），**不查免弱**；
 *   - stat_drop   = "对手施加的弱化"，**第一件事就是查 `ImmunityType::STAT_DROP`（免弱）**。
 *
 * 规则（用户 2026-09-13 定）：
 *   1. **先查免弱、无条件**：目标有免弱 → 直接 IMMUNE 失败、等级一点不动。
 *      "即使对手身上还有强化也不能降低"——不许把"目标有 +N 提升"当作可以抵消弱化的理由。
 *   2. **弱化可以突破"强化保护"**：`STAT_CLEAR`（能力提升无法被消除或吸取）**不查**——
 *      原理不同：那个护的是"已有的提升被拿走"，这个是把等级往下压。
 *   3. **不突破 -6**：到 -6 即停（钳制），不越界（区别于 stat_change 的 AT_CAP 拒绝）。
 *
 * @param stat   能力下标（0..5，5=命中等级，见 stat_change 的说明）
 * @param amount 下降量（正数；<=0 → INVALID_PARAM）
 */
StatDropResult stat_drop(BattleContext* ctx, int target, int stat, int amount);

/**
 * stat_drop_piercing - **穿透版弱化**：不查免弱、直接压等级。
 *
 * 专供"**异常衍化带来的弱化**"（冰封 15→速度-1、焚烬 22→命中-1、感染 27→攻击&特攻-1）：
 * 语料 idx=37「**异常转化为弱化会无视免弱**」、idx=466「不能免疫异常带来的弱化（焚烬，冰封，感染）」。
 *
 * 与 stat_drop 的唯一差异：**第一件事不是查 `ImmunityType::STAT_DROP`**
 * （查了就会被"免弱"整条挡掉，与官方口径正好相反）。其余一致：
 * 不查 STAT_CLEAR（强化保护可穿）、钳 -6、本体/视图一并同步。
 *
 * ⚠️ `stat` 域与 stat_drop **相同**（0..5，5 = 命中等级）—— 本原语与它的差别只在**免弱**，
 *    不在域名。命中等级不写 `battle_attrs`（那只由 `NumericalPropertyIndex` 索引），
 *    只作用于精度公式（skills.cpp 的 query_usage ①）。
 * ⚠️ 写入走 `write_ability_level`（等级写入唯一落点）：衍化产生的等级变化同样会 emit
 *    `EVENT_STAT_CHANGED`，窗口型监听器（如索伦森 1011 的压制）能"写后即知"。
 *
 * @param amount 下降量（正数；<=0 → INVALID_PARAM）
 */
StatDropResult stat_drop_piercing(BattleContext* ctx, int target, int stat, int amount);

// ----------------------------------------------------------------
// 恢复体力 / 固定伤害
// ----------------------------------------------------------------
enum class HealResult {
    SUCCESS,       // 成功恢复
    INVALID_PARAM, // 无效参数
};

enum class FixedDamageResult {
    SUCCESS,         // 造成固定伤害
    TARGET_DEFEATED, // 目标已死亡
    INVALID_PARAM,   // 无效参数
};

/**
 * heal - 恢复目标最大体力的一定比例。
 * fraction_denom > 0：恢复 max_hp / fraction_denom；<= 0：恢复全部。
 */
HealResult heal(BattleContext* ctx, int target, int fraction_denom);

/**
 * heal_amount - 按具体数值恢复（吃封回血 + 恢复效果修正% + 记 last_heal）。
 * 用于"恢复已损失体力的 1/2"这类非整比例恢复（amount 由调用方算好）。
 */
HealResult heal_amount(BattleContext* ctx, int target, int amount);

/**
 * clear_stat_boosts - 消除目标方正等级上的能力提升（"消除双方能力提升状态"）。
 * 只清提升（等级 > 0 → 0），不动弱化/负等级。
 * 返回清掉的提升个数（0 = 目标本无提升 = "消强未成功"，调用方可据此决定后续分支，如"消强成功→必先"）。
 * ⚠️ 与 stat_reversal（反转下降→提升）**语义不同**，不能混用：这是"消除"，那个是"翻转"。
 * 查 `ImmunityType::STAT_CLEAR`（免消除强化）——命中则整次消除失败、返回 0。
 */
int clear_stat_boosts(BattleContext* ctx, int target);

/**
 * clear_stat_drops - 消除目标方负等级上的能力下降（"消除双方能力下降状态"）。
 * 只清弱化（等级 < 0 → 0），不动提升/正等级。返回清掉的下降个数。
 *
 * ⚠️ **不查任何免疫**（用户 2026-09-13 口径）：清弱化对目标**有利**，
 *   免弱(STAT_DROP) 挡的是"施加弱化"、免消除强化(STAT_CLEAR) 护的是"提升"——
 *   两者都不该挡"把弱化拿掉"。故与 clear_stat_boosts（查 STAT_CLEAR）不对称，
 *   这是**故意**的，不是漏查。
 * 本体/视图一并同步（ws.view_levels 是伤害公式的读取源）。
 */
int clear_stat_drops(BattleContext* ctx, int target);

/**
 * crit_defense_break - **暴击破防**：把目标**对应防御的正等级**归 0。
 *   物理攻击（skill_type=0）→ 防御（`levels[2]`）；特殊攻击（skill_type=1）→ 特防（`levels[3]`）
 *   ——与伤害公式同索引（`Defense = getTempAbilityValue(defender, skill.type + 2)`）。
 * 只清**正**等级（提升过的那一项），负等级/弱化不动。
 *
 * ⚠️ **不查任何免疫**（用户 2026-09-13 口径）：不查 `STAT_CLEAR`（免消除强化）——
 *   暴击破防不是"消除强化效果"，是暴击自带的规则，跟免消除强化无关。
 * ⚠️ 这是**引擎内建规则**（与克制倍率/暴击倍率同类），不是"效果"——调用点是攻击结算的
 *   收尾步骤，不在时点桶里。原因：技能无效（盔）时 ATTACK_DAMAGE 整段早退、桶根本不执行，
 *   而"打在盔上一样触发暴击并破防"要求它照样生效。
 *
 * @param defender    被破防方（0/1）
 * @param skill_type  0=物理 / 1=特殊（SkillType 的整数值）；其它值不动作
 * @return true = 确实破了（原本该项为正等级）
 */
bool crit_defense_break(BattleContext* ctx, int defender, int skill_type);

/**
 * transfer_stat_boosts - 转换/吸取能力提升：from 的**正等级**整体搬到 to（from 清零、to 等量累加）。
 * 官方 effect 85"使对手的能力提升效果转化到自己身上" / effect 1287"吸取对手能力提升"是同一动作，
 * 区别只在吸取成功后额外给的东西 → 共用本原语。
 * 返回搬走的属性项数（0 = 无可转化 或 被免消除强化挡下）。
 * 查 `ImmunityType::STAT_CLEAR`（**查 from**：要失去提升的那一方；被挡则 to 也拿不到）。
 * 本体/视图一并同步（ws.view_levels 是伤害公式的读取源）。
 */
int transfer_stat_boosts(BattleContext* ctx, int from, int to);

/**
 * stat_reversal - 反转目标自身的**能力下降**（负等级 → 正等级），不动已存在的提升。
 * 与 clear_stat_boosts（消除提升）互补且独立：反转是"下降翻成提升"，只作用于负等级。
 *
 * @param target 被反转方（通常是施放方自身）
 * @return REVERSED（有下降被翻转为提升）/ NOTHING（目标本无下降，无反转）/ BLOCKED（禁止反转阻断——预留）
 *
 * ⚠️ TODO（用户约定）：**禁止反转**（令能力下降不被反转的回合类规则）——当前引擎的"回合类查询效果"
 *   若允许命中 target，会令反转失败（返回 BLOCKED）。此处先不做，留作未来按 RuleCenter 查询接入。
 */
enum class StatReversalResult {
    REVERSED,   // 有等级被反转（至少一项）
    NOTHING,    // 无可反转项（该方向本就没有对应等级）
    BLOCKED,    // 被挡下（禁止反转 / 免弱免疫）
};
StatReversalResult stat_reversal(BattleContext* ctx, int target);

/**
 * stat_boost_reversal - 反转目标的**能力提升**（正等级 → 等量负等级）。
 * 官方 effect 143"使对手的能力提升效果反转成能力下降效果" / 743"反转对手能力提升，反转成功…"。
 *
 * ⚠️ 与 stat_reversal 是**两个方向、两面免疫**，故拆两个原语：
 *   - stat_reversal（自身下降→提升）：增益类，不查免疫；
 *   - stat_boost_reversal（对手提升→下降）：**弱化类**（把等级往下压）→
 *     **第一件事查 `ImmunityType::STAT_DROP`（免弱）**，免疫则 BLOCKED、等级一点不动。
 *   不查 STAT_CLEAR（强化保护）：那个护的是"已有提升被消除/吸取"，反转是把它压成下降，
 *   与弱化同一原理，可穿（用户 2026-09-13 定）。
 *
 * @return REVERSED / NOTHING（目标本无提升）/ BLOCKED（免弱免疫）
 */
StatReversalResult stat_boost_reversal(BattleContext* ctx, int target);

/**
 * grant_guaranteed_first - 授予"下一回合必定先出手"（必先，分等级）。
 * 注册一个 once 回合类效果到 BATTLE_FIRST_MOVE_RIGHT：下一次先手权时点置
 * ws.guaranteed_first[owner]=tier（tier 越高越先）。是回合类效果 → 可被断回合移除；
 * once + 先手权处 memset → 只在下一回合生效一次。
 */
void grant_guaranteed_first(BattleContext* ctx, int owner, int tier);

/**
 * fixed_damage - 固定伤害（复用 deal_damage，吃护盾/事件）。
 */
FixedDamageResult fixed_damage(BattleContext* ctx, int target, int amount);

/**
 * deal_pink_damage - 造成**指定数值**的粉伤（固定/百分比分型）。
 * 插件侧（moves_lib/soul_lib）造粉伤的唯一入口——插件不链接 sim_core，调不到 deal_damage。
 * kind 决定走哪一档抗性/护罩：FIXED=固定档、PERCENT/PERCENT_VALUE=百分比档；
 * 与 deal_damage 的关系：PERCENT_VALUE 是本函数新增的档（amount 为具体值，不按最大体力换算）。
 * 仍吃免粉/抗性/护罩，并 emit EVENT_TAKE_DAMAGE。
 */
FixedDamageResult deal_pink_damage(BattleContext* ctx, int target, int amount,
                                   DamageKind kind, int actor = -1);
/**
 * deal_true_damage - 真实伤害入口（插件可调）：直通护盾/护罩、穿抗性/免疫。
 *
 * 用途：**粉转真**一族——"对手免疫固定伤害时额外附加等量的真实伤害"（1737）、
 * "减少值低于阈值则附加真伤"（2077）、"未受到粉伤则附加真伤"。
 * 效果侧读 `ctx->resolvedPink` 判定后调本函数补真伤。
 */
FixedDamageResult deal_true_damage(BattleContext* ctx, int target, int amount, int actor);

/**
 * deal_attribute_damage - **属性伤害**结算出口（《赛学必修16—伤害类型》）。
 *
 * 属性伤害 = 由**属性技能**造成的**技能伤害**，数值 = 声明点数 × **克制倍数**（不涉及攻防值），
 * 效果文本里通常直接声明系别与点数（"附加 300 点电系伤害"）。它**有系别**——
 * 固定/百分比/真实三种官方明确"不存在系别概念"，它是第三种技能伤害。
 *
 * ★ 结算形式（用户 2026-09-18 口径）：**只吃克制关系，最后以红伤形式结算**
 *   → 这里走 `DamageKind::NORMAL`（红伤），因此**吃护盾/增伤/减伤/锁伤/挡伤与对应免疫**，
 *     并 emit `EVENT_TAKE_DAMAGE`、进死亡漏斗。**不要**用 FIXED（那是粉伤，且无系别）。
 *
 * @param target  承受方 (0/1)
 * @param points  声明点数（效果文本里的"X 点"）
 * @param element 造成伤害的**系别**（元素 id 对，如 次元·龙 = {17,15}）；
 *                克制倍数 = 该系别 vs `ws.view_elementalAttributes[target]`
 *                （复用官方克制表，双属性按官方组合相乘）。
 *                倍率 0（免疫）或结算不足 1 点 → **不打伤害**，返回 SUCCESS。
 * @param actor   施放方（未知传 -1）
 */
FixedDamageResult deal_attribute_damage(BattleContext* ctx, int target, int points,
                                        const int element[2], int actor);

/**
 * HpZeroResult - 体力归零原语的结算结果
 */
enum class HpZeroResult {
    EXECUTED,       // 已归零
    CONVERTED,      // 被短路/转化（目标方 hp_zero_converted 标记，或特性抑制）：未归零
    TARGET_DOWN,    // 目标本已倒地：无事发生
    INVALID_PARAM,  // 参数非法
};

/**
 * force_hp_to_zero - **体力归零原语**（"秒杀"族专用入口，插件可经 CoreApi 调）。
 *
 * 不是伤害：护盾/护罩/减伤/抗性一概不参与，直接把目标体力置 0（免疫票也不拦——
 * 它不是攻击伤害；免死类口径待官方证据，v1 不查）。
 *
 * ⚠️ **秒杀短路/转化**（用户 2026-09-16 实测口径 + 咤克斯专栏）：现代精灵大量利用秒杀
 *   机制做文章（咤克斯「对方的秒杀效果改为使咤获得1层魔王咒怨」、奥菲式免疫瞬杀…），
 *   所以本原语**每次调用都查 `ctx->hp_zero_converted[target]` 与目标方对来源方瞬杀特性的
 *   抑制条目**：命中 → **短路**（不归零），事件仍以 `blocked=true` emit —— 转化方
 *   （咤咒怨 +1 层）监听事件即可，短路与否都收得到。blocked=false = 真归零。
 *
 * ⚠️ 每次调用（无论短路与否）都 emit `EVENT_HP_TO_ZERO`
 *   （actor=来源方、target=被归零方、amount=归零前体力、blocked=是否被短路）——
 *   这是秒杀族的统一检测点（琉梦"被秒杀效果击败"类检测也挂这里）。
 *
 * ⚠️ 时点语义（用户 2026-09-15 实测）：瞬杀的归零在**红伤结算完之后**——
 *   犀牛式"回血"挂 EVENT_TAKE_DAMAGE（事件 drain 在状态桶之后）→ 回血天然晚于本原语
 *   的直写，红伤 >350 时犀牛最后仍满血（"高伤害和瞬杀同时触发犀牛不会死"）。
 *
 * @return 结算结果；amount（归零前体力）经事件载荷带出。
 */
HpZeroResult force_hp_to_zero(BattleContext* ctx, int target, int actor);

// ----------------------------------------------------------------
// 第二刀新原语（组合语法：无相谛 5 类条件模板）
// ----------------------------------------------------------------
enum class PpReduceResult {
    SUCCESS,        // 已降低（至少一个技能 PP 变化）
    INVALID_PARAM,  // 无效参数
};

/**
 * pp_reduce - 降低目标方所有技能的 PP。
 * 无相谛 700"先出手时降低对手所有PP"。target 方每个技能 pp -= amount（clamp ≥0）。
 */
PpReduceResult pp_reduce(BattleContext* ctx, int target, int amount);

enum class RemoveRoundEffectsResult {
    SUCCESS,     // 清除了目标回合类效果
    NONE,        // 目标没有可清除的回合类效果
    IMMUNE,      // 目标免断，本次无效
    INVALID_PARAM,
};

/**
 * remove_round_effects - 消除目标回合类效果（复用 break_round_effects，吃免断 + EVENT_BREAK）。
 * 无相谛 1083"若后出手则消除对手回合类效果"。
 */
RemoveRoundEffectsResult remove_round_effects(BattleContext* ctx, int target);

enum class DrainHpResult {
    SUCCESS,        // 吸取成功（造成固定伤害 + 自身恢复等量）
    TARGET_DEFEATED,// 目标已死亡
    INVALID_PARAM,
};

/**
 * drain_hp - 吸取体力：目标掉 max_hp/denom 固定伤害，actor 恢复等量（clamp 到 max_hp）。
 * 无相谛 1257"对手不处于异常状态则吸取对手最大体力的1/{n}"。
 */
DrainHpResult drain_hp(BattleContext* ctx, int actor, int target, int fraction_denom);

/**
 * drain_hp_amount - 吸取固定伤害：目标掉 amount 固定伤害，actor 恢复等量。
 * 恢复走 heal 原语（封回血/恢复效果修正生效；被封则吸不到血）。
 */
DrainHpResult drain_hp_amount(BattleContext* ctx, int actor, int target, int amount);

// ----------------------------------------------------------------
// 精灵生命周期：存活 / 死亡 / 消逝（三态定义与官方依据见 effects/spirit_lifecycle.h）
//
// 消逝与死亡是**两种机制**，别混用：
//   死亡 = 体力归零（可复活；仍占位、仍算背包/场下）
//   消逝 = 体力上限归零（不可复活；从所有空间与位置基准里剔除）
// 数值削减（空元卪"击败后减少体力上限"类）走 reduce_max_hp_*，下限钳 1，
// **永远不会**造成消逝——这条不变量是 is_vanished 判据成立的前提。
// ----------------------------------------------------------------

/**
 * count_dead - 阵亡计数（官方四口径，判别轴是措辞）。
 *
 * scope 取 ROSTER(背包) / OFF_FIELD(场下) / NOT_ON_STAGE(不在场) / ALL(全部阵亡)，
 * 见 DeadScope 注释（飞王=ROSTER、唐大=OFF_FIELD、帝君/西叶=ALL）。
 * **额外精灵只进 NOT_ON_STAGE 与 ALL**；**已消逝的四个口径都不计**。
 *
 * 用于"己方每有 N 只阵亡则 XX"类效果——一次调用拿到当前值，
 * 不要在效果里手写 for(slot<6)（那会把消逝的、把额外精灵都算错）。
 */
int count_dead(const BattleContext* ctx, int side, DeadScope scope);

/**
 * reduce_max_hp_pct - 削减体力上限（"减少自身体力上限的X%"）。
 *
 * 官方取整（idx=274 尤纳斯算例）：**削减向上取整**（576×0.9=518.4 → 519）。
 * floor = 下限（默认 1）：**钳到 1 就不再降**。下限的存在保证上限不会被削到 0，
 * 从而"上限==0"与"被消逝"严格等价（见 spirit_lifecycle.h 头注释）。
 * 削减后当前体力若超过新上限则同步压低；已消逝目标直接拒绝（消逝不可逆）。
 *
 * @return 新的体力上限；参数非法/目标已消逝返回 -1
 */
int reduce_max_hp_pct(BattleContext* ctx, int side, int slot, int pct, int floor = 1);

/**
 * raise_max_hp_pct - 提升体力上限（"提升X%的体力上限"、"记录体力上限"类）。
 * 官方取整：**提升向下取整**（571×1.01=576.71 → 576）。已消逝目标拒绝。
 * @return 新的体力上限；参数非法/目标已消逝返回 -1
 */
int raise_max_hp_pct(BattleContext* ctx, int side, int slot, int pct);

/**
 * reduce_max_hp_flat - 削减体力上限（**点数版**："每回合结束后减少200点体力上限"）。
 *
 * 契约与 reduce_max_hp_pct 完全一致：下限钳 floor（默认 1，"上限==0 ⇔ 被消逝"
 * 不变量的守卫）、削减后当前体力超出则同步压低、已消逝目标拒绝。
 * 为什么要点数版：点数衰减用百分比公式凑不准——p=ceil(amount*100/max) 的取整方向
 * 随 max 漂移（999 减 200 会得到 790 而不是 799），点数削减就该用点数原语。
 *
 * @return 新的体力上限；参数非法/目标已消逝返回 -1
 */
int reduce_max_hp_flat(BattleContext* ctx, int side, int slot, int amount, int floor = 1);

/**
 * raise_max_hp_flat - 提升体力上限（**点数版**："获得其消逝前体力上限的10%"，空元之录）。
 * 与 raise_max_hp_pct 同一套契约：提升上限**不动当前体力**、已消逝目标拒绝。
 * @return 新的体力上限；参数非法/目标已消逝返回 -1
 */
int raise_max_hp_flat(BattleContext* ctx, int side, int slot, int amount);

/**
 * defeat_pet - **死亡漏斗**（精灵倒下的唯一登记点，含场下精灵）。
 *
 * 契约（顺序固定）：
 *   1) 目标 hp > 0 → NOT_DOWN（调用方应保证先把体力打到 0）；
 *   2) 该槽本次死亡已登记 → ALREADY_DEAD（幂等，可安全重复调用）；
 *   3) 逐个询问 death_interceptors（**消耗体力成因会跳过 sees_hp_consume=false 的**，
 *      即残留体力免死对消耗体力无效、复活有效——官方 idx=339）；
 *      任一条返回 true 且把 hp 写回 > 0 → INTERCEPTED（死亡不成立）；
 *   4) 登记 pet_death_notified + emit EVENT_DEATH(actor=击杀方, target=倒下方,
 *      slot=槽位, cause=成因)。
 *
 * ⚠️ 为什么必须覆盖场下：官方 idx=62/440 圣光灵神「若于**场下或回合结束后**死亡时
 *    100%复活」，idx=225 咤克斯条目「背包存在朱雀和灵神这种真二命魂印，会在后场触发
 *    魂印复活」。只在场上判死会漏掉这类。
 *
 * @param actor 击杀方（未知传 -1）
 */
DefeatResult defeat_pet(BattleContext* ctx, int side, int slot, int actor, DefeatCause cause);

/**
 * revive_pet - 复活（真2命 / 重生类）。把体力写为 hp（钳到上限），并复位死亡登记
 * （同一只宠"死亡→复活→再死"要能再发一次 EVENT_DEATH）。
 *
 * ⚠️ **已消逝的目标拒绝**（官方 idx=251 重生之翼"被消逝的精灵除外"；消逝不可逆）。
 * 官方体力口径（idx=224）：**后场复活不是回满**，而是回到"游戏开始时记录的体力"
 * ——记录由效果侧自理（存 pet.soulmark_storage），本原语只负责落地。
 *
 * @return 实际写入的体力；参数非法/已消逝返回 -1
 */
int revive_pet(BattleContext* ctx, int side, int slot, int hp);

/**
 * VanishResult - 消逝原语的结果。
 */
enum class VanishResult {
    VANISHED,          // 本次消逝成功（上限归零 + 登记阵亡；**不发 EVENT_DEATH**）
    ALREADY_VANISHED,  // 已被消逝（幂等；后到的消逝方拿不到收益——官方 idx=198
                       // "已经被消逝过的精灵，自然就不会触发…消逝以及后续的重置"）
    INVALID,           // 参数非法
};

/**
 * vanish_spirit - **消逝原语**：把目标体力上限与当前体力一并归零。
 *
 * 语义（官方）：消逝蕴含阵亡，但**不是死亡事件**——它比死亡更强，故：
 *   - 不 emit EVENT_DEATH（否则会误触发击败/亡语类效果）；
 *   - 登记 pet_death_notified（使对账扫描不再为它发死亡事件）；
 *   - 从"背包/场上/场下/不在场"四个空间与位置基准中全部剔除（由 is_vanished 承载）。
 * 可作用于存活目标（官方 idx=223「令对手主动消耗全部体力并消逝」）。
 *
 * @return VANISHED / ALREADY_VANISHED（先到先得）/ INVALID
 */
VanishResult vanish_spirit(BattleContext* ctx, int side, int slot, int actor);

/**
 * vanish_dead_spirits - 对某一方"已阵亡精灵"批量消逝（空元之录 / 魂帝登场时点 / 无为觉者
 * "自身击败对手后令对方全部阵亡精灵消逝"共用入口）。
 *
 * @param count         最多消逝几只（<=0 = 不限，全消）；空元/魂帝按"双方各1只"传 1
 * @param include_extra 是否同时消逝该方**已阵亡的额外精灵**（官方 idx=149 #3：
 *                      空元消逝可让已死亡的金龙玄龙失效 → 传 true）。
 *                      ⚠️ 结算顺序：先本体后额外（本体的"可以吃"是常态，
 *                      额外精灵是补丁面）。
 * @return 实际消逝的数量（0 = 没有可消逝的目标）。**"任意一方消逝成功则获得收益"
 *         类效果就靠这个返回值分支**（官方 idx=33：魂帝"任意一方消逝成功则获得1具尸骸"）。
 */
int vanish_dead_spirits(BattleContext* ctx, int side, int count, bool include_extra);

/**
 * register_death_interceptor - 注册死亡拦截器（免死 / 真2命复活）。
 * 同 source_effect_id 重复注册为覆盖（幂等，供效果每次登场重注册）。
 * 生命周期：随 clearAllEffects 清空；效果侧切换失效请自行调 remove_death_interceptors。
 */
void register_death_interceptor(BattleContext* ctx, DeathInterceptor interceptor);

/** 注销某来源的全部死亡拦截器（效果失效/被消逝时撤掉自己的那条）。 */
void remove_death_interceptors(BattleContext* ctx, int source_effect_id);

/**
 * sync_pending_deaths - **对账兜底**：扫双方 6 槽，把"hp<=0 且未登记死亡"的补登记
 * （cause=EFFECT），并把"hp>0 且登记过"的复位（复活后能再死一次）。
 *
 * 为什么需要：引擎里存在**绕过原语的直写死亡**（咤克斯连锁击杀对场下宠直写 hp=0、
 * 反弹伤害直写 hp -= n）。纯钩子模型在这些路径上全漏；读方查状态不查事件流，
 * 所以漏一次钩子只丢一次通知、不丢事实——由本函数在固定时点补齐。
 * 幂等，可在多个时点重复调用。
 */
void sync_pending_deaths(BattleContext* ctx);

// （kill 原语已删除，2026-09-17）：旧的"直接 hp=0"秒杀绕过秒杀体系（不查秒杀免疫票/
// hp_zero_converted/瞬杀抑制、不 emit EVENT_HP_TO_ZERO），唯一调用方（EffectUnit 的
// Kill 标签，effect 456"若对手体力不足{n}则直接秒杀"）已改走 **force_hp_to_zero**。
// "秒杀"一律经 force_hp_to_zero——它是秒杀族唯一入口，见其注释。

#endif // BATTLE_PRIMITIVES_H
