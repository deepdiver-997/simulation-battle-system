#ifndef BATTLE_PRIMITIVES_H
#define BATTLE_PRIMITIVES_H

#include <effects/effect.h>
#include <effects/rule_center.h>  // SealKind / SkillInvalidNotifyResult / EffectScope

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
 * @param stat   能力下标（0=攻击 1=特攻 2=防御 3=特防 4=速度 5=体力）
 * @param delta  变化量（正=提升，负=下降；越界则 AT_CAP 不变更）
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
 * @param amount 下降量（正数；<=0 → INVALID_PARAM）
 */
StatDropResult stat_drop(BattleContext* ctx, int target, int stat, int amount);

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

enum class KillResult {
    SUCCESS,        // 秒杀（目标体力归 0）
    ALREADY_DEFEATED,
    INVALID_PARAM,
};

/**
 * kill - 秒杀：目标体力直接归 0。
 * 无相谛 456"若对手体力不足{n}则直接秒杀"。
 */
KillResult kill(BattleContext* ctx, int target);

#endif // BATTLE_PRIMITIVES_H
