#ifndef CORE_API_H
#define CORE_API_H

#include <primitives/battle_primitives.h>
#include <plugin/plugin_interface.h>

// ════════════════════════════════════════════════════════════════════
// Core → 插件 的函数指针集合（依赖倒置 / vtable 模式）
//
// 目的：插件效果要"走真原语"（断回合/异常/回血/清强…），但插件不链接 sim_core，
// 无法直调非内联原语，也不该为此把原语内联进头（那会让 dylib 间接依赖 core 符号）。
// 解法：core 在插件初始化时把**自己原语实现的函数指针**打包传给插件；插件只调指针。
//   - 原语留在 sim_core（单一真相源，免疫/异常/事件语义不外泄）。
//   - "原语缓慢增多" → 往 CoreApi 加一个槽，插件初始化自动拿到。
//   - 与现有"插件→core 注册"（IEffectRegistry）互补：这是"core→插件 传函数"另一半。
// ════════════════════════════════════════════════════════════════════

// Core 原语函数指针集合。由 sim_core 填充（见 src/effects/core_api.cpp 的 core_api()）。
struct CoreApi {
    // 契约版本：插件初始化时可与 core 核对；不匹配可拒绝加载。
    const char* version = nullptr;

    // 施加异常（免疫/反弹/转化/抗性 全在 core 侧按序处理）。
    ApplyAnomalyResult (*apply_anomaly)(BattleContext*, int target, int anomaly_id,
                                        int duration_rounds, int actor);
    // 断回合（查免断 + 发 EVENT_BREAK + 清回合类盔威封属）。
    BreakResult (*break_round_effects)(BattleContext*, int target);
    // 消除目标正等级能力提升（"消除双方能力提升状态"）。返回清掉个数(0=消强未成功)。
    // 查免消除强化(STAT_CLEAR)——命中返回 0。
    int (*clear_stat_boosts)(BattleContext*, int target);
    // 转换/吸取能力提升：from 正等级整体搬到 to（效果85 转化 / 效果1287 吸取共用）。
    // 返回搬走项数(0=无可转化 或 被 from 的免消除强化挡下)。
    int (*transfer_stat_boosts)(BattleContext*, int from, int to);
    // 按数值恢复（吃封回血 + 恢复效果修正% + 记 last_heal）。
    HealResult (*heal_amount)(BattleContext*, int target, int amount);
    // 授予"下一回合必先"（分等级：tier 越高越先；可被断回合移除）。
    void (*grant_guaranteed_first)(BattleContext*, int owner, int tier);
    // 能力等级变更（真实 pet.levels，持久；视层留 ws）。返回 SUCCESS/AT_CAP/INVALID_PARAM。
    // **不查免弱**——自身增益/原始变更用它；"对手施加的弱化"用 stat_drop。
    StatChangeResult (*stat_change)(BattleContext*, int target, int stat, int delta);
    // 弱化原语（"令对手攻击-2"）：先查免弱 STAT_DROP（无条件，有提升也照样失败）、
    // 可穿强化保护 STAT_CLEAR、钳 -6。返回 SUCCESS/AT_FLOOR/IMMUNE/INVALID_PARAM。
    StatDropResult (*stat_drop)(BattleContext*, int target, int stat, int amount);
    // 挂"技能拦截"（盔/威/封属，含 hit_invalid 命中失效语义）。属性/攻击/次数/回合/scope 全部可配。
    // source=挂载(施放)方、target=生效(被封)方（统一语义）。
    // chance_pct<100 = 概率封属（每次响应时掷，695/936 用）。
    // 返回**授予句柄**：配合 EVENT_SKILL_ARMOR_RESOLVED（带 grant_id）让监听器精确匹配自己那条盔。
    int (*seal_skill)(BattleContext*, int source, int target, int effect_id, bool attribute,
                      bool attack, int count, int duration_rounds, bool penetrable,
                      int source_slot, EffectScope scope, bool hit_invalid, int chance_pct,
                      bool consumed_when_pierced);
    // 反转目标自身能力下降（负等级→提升）。区别于 clear_stat_boosts（消除提升）。
    StatReversalResult (*stat_reversal)(BattleContext*, int target);
    // 反转目标的**能力提升**（正→等负，"反转对手能力提升"）。弱化类动作 →
    // 先查免弱 STAT_DROP（免疫返回 BLOCKED）。与上面那个方向相反、免疫面不同。
    StatReversalResult (*stat_boost_reversal)(BattleContext*, int target);
    // 固定伤害（吃护罩/固定抗性/事件，复用 deal_damage）。返回"发生了什么"。
    FixedDamageResult (*fixed_damage)(BattleContext*, int target, int amount);
    // 回合数窗口家族（认证数据层声明）："next_rounds"（下N回合，本回合不算）/ 默认 InRounds（N回合内）。
    // 插件注册**持续 N 回合**的效果时应据此算起点：ctx->round_effect_start_round(owner, duration, kind)。
    EffectWindowKind (*effect_window_kind)(int effect_id);
    // 克制倍数查询：`attacker_elem` 克制 `defender_elem` 的倍率（官方原表 {0免疫,0.5减半,1普通,2克制}，
    // 双属性按官方组合相乘）。表是**运行时从 DB 加载**的全局数据（elemental-attributes.cpp），
    // 插件不链接 sim_core、拿不到该符号 → 经本槽查询；core 侧直接指向 Calculation::calculateRestraintMultiples。
    // 用途：**天敌**判定（对手固有属性克制自身固有属性 > 1，官方实测不看技能属性/不看视图）、
    //       "本系/克制"类插件效果。⚠️ 传的是**元素 id 数组**（pet.elementalAttributes / skill.element）。
    double (*restraint_multiplier)(const int attacker_elem[2], const int defender_elem[2]);
    // 粉伤·指定数值（FIXED 固定档 / PERCENT·PERCENT_VALUE 百分比档）。
    // 插件造粉伤的唯一入口（插件不能直调 deal_damage）；PERCENT_VALUE 的 amount 是具体值。
    FixedDamageResult (*deal_pink_damage)(BattleContext*, int target, int amount,
                                          DamageKind kind, int actor);

    // ⚠️ 新槽一律**追加到末尾**：本结构用位置初始化列表填充（src/effects/core_api.cpp），
    //    插在中间会让后面所有槽错位（曾连踩两次）。
    // 降低目标方所有技能 PP（clamp ≥0；`pp == -1` 的无限 PP 技能跳过）。
    // "归零"传一个大于任何 max_pp 的值即可（插件侧 pp_zero_all helper）。
    PpReduceResult (*pp_reduce)(BattleContext*, int target, int amount);
    // 消除目标方**能力下降**（负等级→0）。⚠️ **不查任何免疫**（清弱化对目标有利，
    // 免弱/免消除强化都不该挡它）——与 clear_stat_boosts（查 STAT_CLEAR）故意不对称。
    // 返回清掉的项数。
    int (*clear_stat_drops)(BattleContext*, int target);
    // 真实伤害（直通护盾/护罩、穿抗性/免疫）。**粉转真**一族用：
    // "对手免疫固定伤害时额外附加等量的真实伤害"（1737）、"减少值低于阈值则附加真伤"（2077）。
    // 效果侧读 `ctx->resolvedPink`（粉伤即时结算的结果）判定后再调本槽补真伤。
    FixedDamageResult (*deal_true_damage)(BattleContext*, int target, int amount, int actor);
    // **体力归零原语**（"秒杀"族专用）：不是伤害——护盾/护罩/减伤/抗性/免疫票不参与。
    // 每次调用都 emit EVENT_HP_TO_ZERO（amount=归零前体力、blocked=是否被短路/转化）；
    // 目标方 hp_zero_converted 标记或来源方瞬杀特性被抑制 → 短路（不归零、CONVERTED）。
    // 返回结算结果。通用特性·瞬杀与技能/魂印秒杀效果的统一落点（咤咒怨/琉梦检测挂事件）。
    HpZeroResult (*force_hp_to_zero)(BattleContext*, int target, int actor);
};

// sim_core 暴露的 CoreApi 单例（实际填充）。插件侧不调它；由 core 在初始化时传入。
const CoreApi& core_api();

// 插件初始化时 core 传入的打包句柄 = 注册表(插件→core) + core 函数指针(core→插件)。
struct PluginInitApi {
    IEffectRegistry* registry;
    const CoreApi* core;
};

#endif // CORE_API_H