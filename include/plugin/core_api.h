#ifndef CORE_API_H
#define CORE_API_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

// 插件 ABI 版本（2026-09-27 引入）。⚠️ 改 CoreApi 的**任何布局**（加槽/改槽签名/
// 改字段/改 PluginInitApi）必须 +1——这是插件与内核头"不同源"时唯一响亮报警，
// 不 bump 的结局是字段偏移错位、写进错误字段、行为随机（CLAUDE-full §5.13 事故）。
constexpr uint32_t kPluginAbiVersion = 1;

// Core 原语函数指针集合。由 sim_core 填充（见 src/effects/core_api.cpp 的 core_api()）。
struct CoreApi {
    // ── ABI 身份戳（2026-09-27）：内核填、插件装载即核对（verify_plugin_abi）──
    // 双保险：版本号防"同大小不同布局/语义"，sizeof 防"忘 bump 版本但结构变了"。
    uint32_t abi_version = kPluginAbiVersion;
    size_t api_size = sizeof(CoreApi);

    // 契约版本：插件初始化时可与 core 核对；不匹配可拒绝加载。
    const char* version = nullptr;

    // 施加异常（免疫/反弹/转化/抗性 全在 core 侧按序处理）。
    ApplyAnomalyResult (*apply_anomaly)(BattleContext*, int target, int anomaly_id,
                                        int duration_rounds, int actor) = nullptr;
    // ── 概率申报版（2026-09-22，概率闸门管道）────────────────────────────
    // 与上面三个同语义，**多了"申报概率"**：chance_pct >= 0 时内核在**掷骰之前**先问
    // 概率闸门（亮节族）改写，再按改写值掷一次；没过门返回 ROLL_FAILED。
    // chance_pct < 0 = 未申报（等价于旧槽，闸门不介入）。
    // 插件迁移到本槽即可让该调用点**受闸门管辖**（旧槽保留 = 未迁移调用点行为完全不变）。
    // source 是概率来源分类（亮节四类全管；永沐不含 Trait）。
    ApplyAnomalyResult (*apply_anomaly_checked)(BattleContext*, int target, int anomaly_id,
                                                int duration_rounds, int actor,
                                                int chance_pct,
                                                ChanceSource source) = nullptr;
    ApplyAnomalyResult (*apply_anomaly_ancient_checked)(BattleContext*, int target,
                                                        int anomaly_id, int duration_rounds,
                                                        int actor, int chance_pct,
                                                        ChanceSource source) = nullptr;
    ApplyAnomalyResult (*apply_anomaly_raw_checked)(BattleContext*, int target, int anomaly_id,
                                                    int duration_rounds, int actor,
                                                    int chance_pct,
                                                    ChanceSource source) = nullptr;
    // 断回合（查免断 + 发 EVENT_BREAK + 清回合类盔威封属）。
    BreakResult (*break_round_effects)(BattleContext*, int target) = nullptr;
    // 消除目标正等级能力提升（"消除双方能力提升状态"）。返回清掉个数(0=消强未成功)。
    // 查免消除强化(STAT_CLEAR)——命中返回 0。
    int (*clear_stat_boosts)(BattleContext*, int target) = nullptr;
    // 转换/吸取能力提升：from 正等级整体搬到 to（效果85 转化 / 效果1287 吸取共用）。
    // 返回搬走项数(0=无可转化 或 被 from 的免消除强化挡下)。
    int (*transfer_stat_boosts)(BattleContext*, int from, int to) = nullptr;
    // 按数值恢复（吃封回血 + 恢复效果修正% + 记 last_heal）。
    HealResult (*heal_amount)(BattleContext*, int target, int amount) = nullptr;
    // 授予"下一回合必先"（分等级：tier 越高越先；可被断回合移除）。
    void (*grant_guaranteed_first)(BattleContext*, int owner, int tier) = nullptr;
    // 能力等级变更（真实 pet.levels，持久；视层留 ws）。返回 SUCCESS/AT_CAP/INVALID_PARAM。
    // **不查免弱**——自身增益/原始变更用它；"对手施加的弱化"用 stat_drop。
    StatChangeResult (*stat_change)(BattleContext*, int target, int stat, int delta) = nullptr;
    // 弱化原语（"令对手攻击-2"）：先查附加禁令 ATTACH_BAN（按 actor 判定，-1=未声明跳过；
    // 2026-09-20 米斯蒂克 4676 引入）、再查免弱 STAT_DROP（无条件，有提升也照样失败）、
    // 可穿强化保护 STAT_CLEAR、钳 -6。返回 SUCCESS/AT_FLOOR/IMMUNE/INVALID_PARAM。
    StatDropResult (*stat_drop)(BattleContext*, int target, int stat, int amount,
                                int actor) = nullptr;
    // 挂"技能拦截"（盔/威/封属，含 hit_invalid 命中失效语义）。属性/攻击/次数/回合/scope 全部可配。
    // source=挂载(施放)方、target=生效(被封)方（统一语义）。
    // chance_pct<100 = 概率封属（每次响应时掷，695/936 用）。
    // 返回**授予句柄**：配合 EVENT_SKILL_ARMOR_RESOLVED（带 grant_id）让监听器精确匹配自己那条盔。
    int (*seal_skill)(BattleContext*, int source, int target, int effect_id, bool attribute,
                      bool attack, int count, int duration_rounds, bool penetrable,
                      int source_slot, EffectScope scope, bool hit_invalid, int chance_pct,
                      bool consumed_when_pierced) = nullptr;
    // ③层"命中效果失效"直通（按次）：target=被失效方。kEffectsOnly=效果失效伤害照常、
    // kFullNull=白板（效果失效+伤害归 0）。攻击/属性两类分开挂（要两类都失效调两次）。
    // 圣光莫妮卡·神灵涤荡（738"下2回合攻击无法造成伤害且命中效果失效"，2026-09-25 首用）。
    void (*hit_effect_invalid)(BattleContext*, int target, HitInvalidMode mode, int count,
                               bool is_attribute_skill, int source_id) = nullptr;
    // **体力重置**（≠恢复）：令体力等于最大体力的 pct%。不查封回血、不吃恢复修正、
    // 不发 EVENT_HEAL_RESTORED（口径见 battle_primitives.h；1909 首用，2026-09-25）。
    int (*reset_hp)(BattleContext*, int target, int pct) = nullptr;
    // 反转目标自身能力下降（负等级→提升）。区别于 clear_stat_boosts（消除提升）。
    StatReversalResult (*stat_reversal)(BattleContext*, int target) = nullptr;
    // 反转目标的**能力提升**（正→等负，"反转对手能力提升"）。弱化类动作 →
    // 先查免弱 STAT_DROP（免疫返回 BLOCKED）。与上面那个方向相反、免疫面不同。
    StatReversalResult (*stat_boost_reversal)(BattleContext*, int target) = nullptr;
    // 固定伤害（吃护罩/固定抗性/事件，复用 deal_damage）。返回"发生了什么"。
    FixedDamageResult (*fixed_damage)(BattleContext*, int target, int amount) = nullptr;
    // 回合数窗口家族（认证数据层声明）："next_rounds"（下N回合，本回合不算）/ 默认 InRounds（N回合内）。
    // 插件注册**持续 N 回合**的效果时应据此算起点：ctx->round_effect_start_round(owner, duration, kind)。
    EffectWindowKind (*effect_window_kind)(int effect_id) = nullptr;
    // 克制倍数查询：`attacker_elem` 克制 `defender_elem` 的倍率（官方原表 {0免疫,0.5减半,1普通,2克制}，
    // 双属性按官方组合相乘）。表是**运行时从 DB 加载**的全局数据（elemental-attributes.cpp），
    // 插件不链接 sim_core、拿不到该符号 → 经本槽查询；core 侧直接指向 Calculation::calculateRestraintMultiples。
    // 用途：**天敌**判定（对手固有属性克制自身固有属性 > 1，官方实测不看技能属性/不看视图）、
    //       "本系/克制"类插件效果。⚠️ 传的是**元素 id 数组**（pet.elementalAttributes / skill.element）。
    double (*restraint_multiplier)(const int attacker_elem[2], const int defender_elem[2]) = nullptr;
    // 粉伤·指定数值（FIXED 固定档 / PERCENT·PERCENT_VALUE 百分比档）。
    // 插件造粉伤的唯一入口（插件不能直调 deal_damage）；PERCENT_VALUE 的 amount 是具体值。
    FixedDamageResult (*deal_pink_damage)(BattleContext*, int target, int amount,
                                          DamageKind kind, int actor) = nullptr;
    // ⚠️ 填充纪律（2026-09-19 治理）：本结构**全槽 NSDMI = nullptr**，由
    //    src/effects/core_api.cpp 的 core_api() **具名赋值**填充，并在首次初始化时
    //    跑 `first_empty_slot` 完整性检查——漏填/错位当场 abort 报槽名，不再静默 null。
    //    新增槽三步：① 在此声明（NSDMI 会自动带上，**可以按家族插在中间**，
    //    不再要求追加到末尾）② core_api.cpp 具名赋值 ③ first_empty_slot 补一行检查。
    // 降低目标方所有技能 PP（clamp ≥0；`pp == -1` 的无限 PP 技能跳过）。
    // 每个实际变化的槽 emit EVENT_PP_REDUCED。效果会监听 PP 降低/清除 → **清 PP 必须走原语**，
    // 直写 `skills[i].pp` 会绕过事件。
    PpReduceResult (*pp_reduce)(BattleContext*, int target, int amount) = nullptr;
    // 清零目标方**指定槽位**的 PP（"随机{0}项技能 PP 归零"族专用，2026-09-23 批 8 修正线）。
    // 实际归零时 emit EVENT_PP_REDUCED（slot=本槽）；无限 PP 槽 INVALID_PARAM 不发。
    // actor = 清除来源方（0/1）；-1 = 无归属（己方清自身族，如 1237）——无归属
    // 不触发琼华之庇的保留/反应（2026-09-25 口径：001 自清不挂先制限制）。
    PpReduceResult (*pp_zero_slot)(BattleContext*, int target, int slot, int actor) = nullptr;
    // 恢复目标方所有技能 PP（2026-09-24 追加，极渊DS-001 2414"20%令自身所有技能PP值+1"
    // 首用；亮节"给对手 pp 为 0 的技能恢复 5 点"同用）。只补不削（clamp ≤ maxPP）、
    // 无限 PP 槽跳过；only_empty=true 只恢复 pp==0 的槽（亮节口径）。返回实际变化槽数。
    // ⚠️ 不 emit 事件（EVENT_PP_REDUCED 是"被削"语义，恢复暂无监听方）。
    int (*pp_restore)(BattleContext*, int target, int amount, bool only_empty) = nullptr;
    // PP 恢复（精灵王线 K7，圣莫 PP 辅助链/2432 用）：
    //   pp_restore_slot  —— 指定槽恢复到 maxPP（无限 PP 槽 INVALID_PARAM 不动）。返回恢复量。
    //   pp_restore_points—— 指定槽恢复指定点数（钳到 maxPP）。返回实际恢复量。
    int (*pp_restore_slot)(BattleContext*, int side, int pet_slot, int skill_slot) = nullptr;
    int (*pp_restore_points)(BattleContext*, int target, int slot, int points) = nullptr;
    // 消除目标方**能力下降**（负等级→0）。⚠️ **不查任何免疫**（清弱化对目标有利，
    // 免弱/免消除强化都不该挡它）——与 clear_stat_boosts（查 STAT_CLEAR）故意不对称。
    // 返回清掉的项数。
    int (*clear_stat_drops)(BattleContext*, int target) = nullptr;
    // 真实伤害（直通护盾/护罩、穿抗性/免疫）。**粉转真**一族用：
    // "对手免疫固定伤害时额外附加等量的真实伤害"（1737）、"减少值低于阈值则附加真伤"（2077）。
    // 效果侧读 `ctx->resolvedPink`（粉伤即时结算的结果）判定后再调本槽补真伤。
    FixedDamageResult (*deal_true_damage)(BattleContext*, int target, int amount, int actor) = nullptr;
    // **体力归零原语**（"秒杀"族专用）：不是伤害——护盾/护罩/减伤/抗性/免疫票不参与。
    // 每次调用都 emit EVENT_HP_TO_ZERO（amount=归零前体力、blocked=是否被短路/转化）；
    // 目标方 hp_zero_converted 标记或来源方瞬杀特性被抑制 → 短路（不归零、CONVERTED）。
    // 返回结算结果。通用特性·瞬杀与技能/魂印秒杀效果的统一落点（咤咒怨/琉梦检测挂事件）。
    HpZeroResult (*force_hp_to_zero)(BattleContext*, int target, int actor) = nullptr;
    // 古早异常施加（主动毒）：只查 ImmunityTier::Ancient 免疫（次免/魂免两段 + Mark 0 老魂免），
    // 跳过抗性/转化、不触发弹控。古早"命中后{0}%令对方XX"模板族（10/11/12/14/15/114）
    // 与特性接触施加（Eid 6/66/67）专用；现代施加（参数化族，如 2189）仍走上面 apply_anomaly。
    ApplyAnomalyResult (*apply_anomaly_ancient)(BattleContext*, int target, int anomaly_id,
                                                int duration_rounds, int actor) = nullptr;
    // 遗留裸施加（特性被动毒专用）：什么都不检测（免疫含 Ancient 层/Mark 0/弹控/抗性/转化
    // 全穿），校验后直接落地。"受到普通攻击（物攻）时 n% 使对方XX"（带电/高热/冰冷/阴森）。
    ApplyAnomalyResult (*apply_anomaly_raw)(BattleContext*, int target, int anomaly_id,
                                            int duration_rounds, int actor) = nullptr;
    // 吸取体力：目标掉 max_hp/{denom} 固定伤害，actor 恢复等量（恢复走 heal_impl，
    // 封回血生效）。"{0}回合内每回合使用技能吸取对手最大体力的1/{1}"（597）与
    // 无相谛 1257 同族。另有无参数版 drain_hp_amount（暂无调用方，未暴露）。
    DrainHpResult (*drain_hp)(BattleContext*, int actor, int target, int fraction_denom) = nullptr;
    // ── 精灵生命周期（存活/死亡/消逝，见 effects/spirit_lifecycle.h）──
    // 阵亡计数（四口径 ROSTER/OFF_FIELD/NOT_ON_STAGE/ALL，额外精灵只进后两个，
    // 已消逝四个口径都不计）。"己方每有 N 只阵亡则 XX"类效果用它，别手写 for(slot<6)。
    int (*count_dead)(const BattleContext*, int side, DeadScope scope) = nullptr;
    // 削减/提升体力上限（官方取整：削减向上、提升向下）。削减下限钳 1——
    // **永远不会**造成消逝（消逝只能由 vanish_spirit 产生）。
    int (*reduce_max_hp_pct)(BattleContext*, int side, int slot, int pct, int floor) = nullptr;
    int (*raise_max_hp_pct)(BattleContext*, int side, int slot, int pct) = nullptr;
    // 死亡漏斗（含场下精灵）+ 复活。defeat_pet 会询问登记过的死亡拦截器。
    DefeatResult (*defeat_pet)(BattleContext*, int side, int slot, int actor, DefeatCause cause) = nullptr;
    int (*revive_pet)(BattleContext*, int side, int slot, int hp) = nullptr;
    // 消逝（体力上限归零，不可逆、不发 EVENT_DEATH）。批量版返回**实际消逝数**
    // ——"任意一方消逝成功则获得收益"（魂帝尸骸）靠它分支。
    VanishResult (*vanish_spirit)(BattleContext*, int side, int slot, int actor) = nullptr;
    int (*vanish_dead_spirits)(BattleContext*, int side, int count, bool include_extra) = nullptr;
    // 死亡拦截器（残留体力免死 / 真2命复活）：sees_hp_consume 表达"消耗体力能否免死"，
    // 官方口径是残留免死不能、复活能（idx=339）——两层不可合并。
    void (*register_death_interceptor)(BattleContext*, DeathInterceptor interceptor) = nullptr;
    void (*remove_death_interceptors)(BattleContext*, int source_effect_id) = nullptr;
    // 消耗全部体力印记（死亡印记，赛学必修15口径）：挂载后每回合在挂印者行动结束
    // 自动结算一次（目标体力清 0 → HP_CONSUME 击杀，穿免死/复活可见）；目标切走或
    // 真死自动解除。幂等（同挂印者-同目标只挂一条）。
    void (*attach_hp_consume_mark)(BattleContext*, int marker_side, int marker_slot,
                                   int target_side, int target_slot) = nullptr;
    // 当回合无法行动（effect 39 族）：跳主流程 + 压制额外行动；回合完成自动清。
    void (*suppress_extra_action)(BattleContext*, int owner) = nullptr;
    // ── 点数版体力上限削减/提升（2026-09-17 追加，空元之渎/之录用）──
    // 契约与 pct 版一致：下限钳 1（"上限==0 ⇔ 被消逝"守卫）、已消逝拒绝、
    // 削减同步压低当前体力、提升不动当前体力。点数衰减用百分比凑不准（取整漂移）。
    int (*reduce_max_hp_flat)(BattleContext*, int side, int slot, int amount, int floor) = nullptr;
    int (*raise_max_hp_flat)(BattleContext*, int side, int slot, int amount) = nullptr;
    // ── 属性伤害（2026-09-18 追加；《赛学必修16—伤害类型》）──
    // "附加 X 点 {系} 伤害"族的结算出口：**数值由调用方按"点数 × 克制倍数"算好后传入**
    //（克制的防御方一侧取 `ws.view_elementalAttributes[target]`，插件侧可用上面的
    // `restraint_multiplier` 槽自查；也可直接用 core 的本原语让它一并算）。
    // 结算形式 = **红伤**（`DamageKind::NORMAL`）→ 吃护盾/增伤/减伤/锁伤/挡伤与对应免疫，
    // emit EVENT_TAKE_DAMAGE、进死亡漏斗。属性伤害**有系别**，既不是固定伤害（粉伤、无系别）
    // 也不是真伤（真伤"无法减免"，属性伤害受对应的增减免影响）。
    FixedDamageResult (*deal_attribute_damage)(BattleContext*, int target, int points,
                                               const int element[2], int actor) = nullptr;
    // ── 随机异常附加（2026-09-19 追加，混沌魔尊 1263 首用）──
    // 解析池（⊕ ANOMALY_POOL_MOD 改写票）→ 概率门（整个子句一次）→ 不重复挑选 →
    // 逐个 apply_anomaly 单通道。三段账见 RandomAnomalyResult；池 helper
    // （anomaly_pool_control/all）是头内联函数，插件直调不占槽。
    RandomAnomalyResult (*attach_random_anomalies)(BattleContext*, int target,
                                                   const std::vector<int>& base_pool,
                                                   int count, int probability_pct, int actor,
                                                   ChanceSource source) = nullptr;
    // 加速异常消耗（官方 effect 2207「使自身所处的异常状态剩余回合数-{0}」）。
    // 只压剩余回合数、不解除——到期由 tick 自然收尾（星盘族只认自然耗尽，见原语注释）。
    int (*reduce_active_anomaly_rounds)(BattleContext*, int target, int delta) = nullptr;
    // 异常槽查询（BattleContext::has_active_abnormal_status 非 inline，插件不可直调）。
    bool (*has_active_anomaly)(BattleContext*, int target, int status_id) = nullptr;
    // 异常剩余回合查询（get_abnormal_status_end_round 同为非 inline；返回 end_round 原值，
    // "还剩几回合"由调用方减当前回合——转移类效果（灵巢之主 4599）用）。
    int (*get_abnormal_status_end_round)(const BattleContext*, int target, int status_id) = nullptr;
    // 异常剩余回合直写（end_round 非 inline 同上）。灵巢 4599"被弹控的睡眠随攻击
    // 打到盔上而打醒"（end_round=当前回合 → 立即失效）用（2026-09-20 用户口径）。
    void (*set_abnormal_status_end_round)(BattleContext*, int target, int status_id,
                                          int end_round) = nullptr;
    // 解除全部生效中异常（效果解除路径：直接清槽、无伤害无事件——不触发自然到期）。
    // 星启（天启星魂 4677）/律理虚浮（2145）类"解除自身异常"用。返回解除条数。
    int (*dispel_active_anomalies)(BattleContext*, int target) = nullptr;
    // 按**名单**选择性解除生效中异常（同上路径，只清名单内那几种）。
    // 圣甲·盖亚 逆转机甲（544「解除自身的烧伤、冻伤、中毒状态」）首用——
    // 麻痹/睡眠等名单外异常必须留下。返回实际解除条数。
    int (*cure_anomalies)(BattleContext*, int target, const int* anomaly_ids, int count) = nullptr;
    // ── 对场下精灵的真实伤害（2026-09-20，天启帝君 1306 首用）──
    // 伤害类型 = 真实伤害（穿抗性/免疫），与"对场下造成粉伤"的效果族区分开。
    // 查场下保护门；击杀走 defeat_pet 漏斗；residue_floor 支持"致死时残留X点"族。
    // 不发 EVENT_TAKE_DAMAGE（场下结算无在途管线，事件只按方过滤会误触场上受击监听）。
    OffFieldTrueDamageResult (*deal_off_field_true_damage)(BattleContext*, int side, int slot,
                                                           int amount, int actor,
                                                           int residue_floor) = nullptr;
    // 重触发登场行为（2026-09-20 追加，莫塔里安 4541 击杀节点首用）：
    // 以 (side, slot) 为主体，重放该宠魂印的 early 节点 + on_enter 钩子——
    // **只执行、不重注册**桶节点。阿尔忒弥斯式副本重触发时主体是她自己，
    // 天然落到她自己的登场效果（官方实测"重新触发登场时的效果是狩猎女神自己的登场效果"）。
    void (*retrigger_entrance)(BattleContext*, int side, int slot) = nullptr;
    // 直接设定某条已存在异常的剩余回合数（2399 宿世归泯"衰弱回合数归 1"族）。
    // 只刷新已存在的，不无中生有；冻结不拦（写入非减少）。返回 1=刷新 / 0=无此异常。
    int (*set_anomaly_rounds)(BattleContext*, int target, int status_id, int rounds) = nullptr;
    // ── 精灵王线（2026-09-24，K2/K4/K5）───────────────────────────────
    // 主动消耗护盾（沧岚 2263"消耗自身的护盾值" / 混地 2318"消耗全部护盾炸盾"）：
    // 从最高优先级盾开始拿；被拿空的每条 emit EVENT_SHIELD_BROKEN（"视为被击破"）。
    // 返回实际消耗点数；broken_count 可空。
    int (*consume_shield)(BattleContext*, int target, int amount, int* broken_count) = nullptr;
    // 消除/吸取能力提升的**带归因**变体（混地"自身强化被消除或吸取时插入结算"）：
    // 行为同对应老原语，清除/搬走数 > 0 时 emit EVENT_STAT_REMOVED(actor/target/amount)。
    int (*clear_stat_boosts_as)(BattleContext*, int target, int actor) = nullptr;
    int (*transfer_stat_boosts_as)(BattleContext*, int from, int to, int actor) = nullptr;
    // 精灵王谓词（圣逼"给所有精灵王发标记/恢复精灵王"）：官方判据 = 技能全集含
    // 效果 760，装配期预计算在 pet.is_spirit_king。只认 (side, on_stage 上的当前槽)
    // ——场下精灵王不是"在场精灵王"，由调用方按官方文本自己决定要不要扫背包。
    bool (*is_spirit_king)(const BattleContext*, int side, int slot) = nullptr;
};

// ── 虚拟异常视图约定（K6，2026-09-25 改为查询时拉取，用户口径）──────────
// "视为不处于异常"类效果**不做主动置位/撤销**：插件只把层数作为 int 维护在
//   plugin_storage[kAnomalyViewMaskPubBase + effect_id * 8 + side]
// has_active_anomaly（插件条件查询面）在查询前扫描该段，任一来源 >0 → 该方视为
// 不处于任何异常；层数被消耗归零 → 自然解除。内核结算路径不受影响。
constexpr int kAnomalyViewMaskPubBase = 4699000;

// sim_core 暴露的 CoreApi 单例（具名填充 + 完整性检查，见 src/effects/core_api.cpp）。
// 插件侧不调它；由 core 在初始化时传入。
const CoreApi& core_api();

// 插件初始化时 core 传入的打包句柄 = 注册表(插件→core) + core 函数指针(core→插件)。
struct PluginInitApi {
    IEffectRegistry* registry;
    const CoreApi* core;
};

// 插件装载即校验（2026-09-27）：每个 dylib 入口拿到 core 指针的第一时间调用。
// 版本或大小不匹配 = 插件与内核头不同源（陈旧 dylib / 半新半旧构建）——继续跑
// 的结局是字段偏移错位、静默写错字段；按项目"静默失败是头号敌人"的治理惯例，
// 直接 abort 报双版本，指路全量重建。空指针不在这里处理（各入口自己判空）。
inline void verify_plugin_abi(const CoreApi* core) {
    if (!core) {
        return;
    }
    if (core->abi_version != kPluginAbiVersion || core->api_size != sizeof(CoreApi)) {
        std::fprintf(stderr,
                     "[plugin-abi] FATAL: 插件与内核 ABI 不匹配——core abi=%u size=%zu，"
                     "本 dylib 期望 abi=%u size=%zu。\n"
                     "[plugin-abi] 修复：全量重建（cmake --build build --clean-first "
                     "或 python3 tools/run_regression.py --clean-first）。\n",
                     core->abi_version, core->api_size,
                     kPluginAbiVersion, sizeof(CoreApi));
        std::abort();
    }
}

#endif // CORE_API_H