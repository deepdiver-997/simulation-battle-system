#ifndef COMMON_TRAIT_EFFECTS_H
#define COMMON_TRAIT_EFFECTS_H

// 通用特性效果（new_se stat=1 的行为层，core 侧）。
// 运行时查询/变更层见 effects/trait_state.h + battleContext.h 的内联入口。

class BattleContext;

/**
 * 安装通用特性效果：红伤管线的常驻条目（TEAM 绑定，回调实时读当前登场精灵的生效特性）。
 *   - TRAIT_REPLACE（链首）：瞬杀 0 星——**条件式红伤拉高**：初始红伤低于对手当前体力时
 *     拉到当前体力（≥ 则不触发），之后的增减伤/犀牛检测照常作用于该值；
 *   - REDUCE_TRAIT：坚硬——靠前的乘法减伤（早于保底，只免红伤）；
 *   - AMP_EXTRA：精神——特殊攻击伤害 +n%（非通用增伤、乘法、按技能类别门控）。
 * init_battle / clearAllEffects 在管线 clear 之后调用（与 install_default_* 同一批）。
 */
void install_common_trait_effects(BattleContext* ctx);

/**
 * 瞬杀钩位（0-5 星统一，星级只差触发概率）：finish_attack_damage 在
 * `apply_resolved_damage` **之后**调用——红伤结算完之后，经 `force_hp_to_zero` 原语
 * 强制对手体力归零（不是粉伤；犀牛式回血挂 EVENT_TAKE_DAMAGE、drain 在状态桶之后
 * → 回血晚于归零，高伤+瞬杀同时触发时犀牛最后仍满血）。进攻类技能命中才掷点（必修6）。
 */
void trait_instant_kill_zero_hook(BattleContext* ctx, int attacker_id);

/**
 * 接触毒特性钩位（主动毒：静电/颤栗/火热/极寒；被动毒：带电/高热/冰冷/阴森）：
 * handle_BattleFirst/SecondOnSkillHit 在命中判定（resolve_skill_execution）之后、
 * **技能效果结算（SKILL_EFFECT）之前**调用——必修6："需要（自身）技能命中才可以触发"
 * （miss 不触发；被盔 SKILL_INVALID 不计为命中）。
 * 主动毒：攻方本体槽，静电/颤栗只认物理、火热/极寒只认特殊，命中后掷 args[0]%
 * （3~8 百分点）令**守方**中 args[1]（2~3 回合），走 apply_anomaly_ancient（主动毒通道）。
 * 被动毒：守方本体槽，攻方**物理**攻击命中时（"受到普通攻击"=物攻，用户 2026-09-16 确认）
 * 掷点令**攻方**中 args[1]，走 apply_anomaly_raw（遗留裸施加，什么都不检测）。
 * ⚠️ 只认本体槽——天女式复制毒不经特性节点（见 trait_state.h 架构拍板）。
 */
void trait_contact_poison_hook(BattleContext* ctx, int attacker_id);

/**
 * 被动属性降低钩位（反抗/反驳/忽略/草率/慌张，Eid 34）：同 ON_SKILL_HIT 钩位。
 * "受到**特殊攻击**时有 args[1]% 使**对方** m 降低 1 个等级"（m 由 args[0] 官方能力码
 * 经 trait_stat_index_from_code 重映射——官方码序与引擎 stat 索引不同序）。
 * 走 `stat_drop`（对手施予的弱化 → 查免弱 STAT_DROP）。
 */
void trait_passive_stat_drop_hook(BattleContext* ctx, int attacker_id);

/**
 * 被动属性提升钩位（反击/抵抗/反攻/坚韧/借风，Eid 35）：挂 **BEFORE_SKILL_HIT**。
 * "受到**任何攻击**时有 args[1]% 使**自身** m 提升 1 个等级"。
 * ★ 必修6 ①：赋予发生在**命中判定之前**（"因此会被对方一些技能带有消强/吸强/反强补偿影响"）
 *   → 故挂 BEFORE_SKILL_HIT 而非命中后；也不要求命中。
 * ★ 必修6 ②：**属性技能可以触发** → 不按技能类别门控。
 * 属**自身增益** → 走 stat_change（不查免弱，与 PassiveStatDrop 相对）。
 */
void trait_pre_hit_stat_boost_hook(BattleContext* ctx, int actor_id);

/**
 * 致死存活特性钩位（顽强 Eid 31/147 / 回神 Eid 33/148）：finish_attack_damage 里
 * **在 `trait_instant_kill_zero_hook`（瞬杀归零）之后**调用——用户 2026-09-16 拍板的口径：
 * "归零与强制保留/回满并不冲突，**看先后顺序定实际效果**，不要动不动就短路"
 * → 先归零、再由本钩把 0 体力抬回（顽强→m 点；回神→满血）。口径若有变只需挪这一个调用点。
 *
 * 只读**被攻击方**的登场特性槽；仅**战斗阶段**触发（本钩在攻击伤害出口，回合结束的
 * 粉伤/真伤路径不经过它——必修6 ①"回合结束后的致死伤害直接击杀"）。
 * "强制残留体力不受削续航影响" → 直写 hp，不走 `heal`（不给封回血拦）。
 */
void trait_survive_lethal_hook(BattleContext* ctx, int defender_id);

/**
 * 强攻(62)/强念(63) 的 **miss 分支**钩位：攻击技能 miss 时追加伤害照常扣血
 * （必修6 ②"攻击技能 miss 了也可以扣除对手体力"）。
 * 调用点：`Skills::execute` 的 miss 出口——**只对 MISS**；被盔/封技（SEALED）技能没打出去，不适用。
 */
void trait_extra_damage_on_miss_hook(BattleContext* ctx, int attacker_id);

#endif // COMMON_TRAIT_EFFECTS_H
