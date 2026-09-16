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

#endif // COMMON_TRAIT_EFFECTS_H
