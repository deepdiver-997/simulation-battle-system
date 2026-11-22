# 内核注 —— BattleContext（include/fsm/battleContext.h）

锚点 = 声明处 C++ 符号名；代码侧短块末行 `详: 内核注#<本页锚点>`。
条目格式见 [README](README.md)。2026-11-22 首批迁移。

---

## ability_levels / kAbilityLevelSlotCount

一句话语义：能力等级**本体**——on-stage 作用域，换宠清空；权威状态在本容器。

谁写谁读：写入走 stat_change/反转等原语；伤害公式读 `ws.view_levels`（**回合内视图**，
每回合从本容器重基，见 battleFsm.cpp 的 view 同步点）——本体/视图分离不变，
只是"本体"从 pet 换到了 context。

生命周期：随上下场清（perform_switch 的 clear_ability_levels，与 abnormal_status_end_round
同类语义）。

裁决史/引注：
- ★ 2026-09-16 用户拍板：等级**只在在场期间有意义**，"切换清除等级状态"——原先放在
  `ElfPet::levels`（精灵本体、跨切换保留）是设计失误：换宠不清、跨对局残留
  （`clearAllEffects` 也不清 pet.levels，训练模式复用同一批 pet 对象时会被上一局污染）。
- 索引（**6 槽**，2026-09-18 口径更正：此前注释把槽 5 误标成"体力"）：
  0=攻击 1=特攻 2=防御 3=特防 4=速度 **5=命中**。
- 依据（用户 2026-09-18 口径 + 引擎内既有事实）：
  · **能力提升状态官方就是 6 种** = 双攻 + 双防 + 速度 + 命中；**没有"体力等级"**。
  · **基础数值**（`numerical_properties` / `battle_attrs`）也是 6 项，但第 6 项是**体力**；
    两套索引只在 0..4（攻/特攻/防/特防/速）重合，不是同一套东西。
  · `resources/moves_lib/stat_dispel.cpp:403-409` 早已写明该映射："官方序
    [攻,防,特攻,特防,速,**命中**] → 引擎序 `kStatMap[6]={0,2,1,3,4,5}`，**5命中→5**"，
    并且已经真的在调 `stat_change(self, 5, delta)` → 槽 5 事实上一直是命中。
  · `getTempAbilityValue` 里那句"命中等级为负时的特殊处理"（返回 85/70/55/45/35/25）
    本就是给槽 5 用的，只是被误放在一个按 `NumericalPropertyIndex` 取**属性值**的函数里
    （该函数从不以 HP 被调用 → 一直是死分支）。本线把该档位表搬到精度公式处。
- ⚠️ 槽 5 只作用于**精度公式**（skills.cpp 的 query_usage ①）；`getTempAbilityValue` 的
  `NumericalPropertyIndex` 域仍是 0..5，且**体力(5)不吃等级**（体力没有等级）。
- 槽位常量见 abnormal-system/abnormal-types.h 的 kAbilityLevelIndexHit（两者必须相等）。
- ⚠️ 不要与官方数据的能力码混用（官方码序逐族不同，见 trait_state.h 的
  trait_stat_index_from_code 说明；上引 stat_dispel 的 kStatMap 就是其中一套）。
- 事件带宽度钉子：`kSampleLevelSlots`（effects/state_tape.h）与本常量 static_assert
  互锁（内核头不能反向依赖 context，漂了编译失败而非静默少采）。

关联：`ability_level()` 访问器、`ws.view_levels`、`clear_ability_levels`。

---

## crit_happened

一句话语义：本次技能使用是否暴击（一次技能使用掷一次，多段/变威力共用）。

谁写谁读：`Skills::query_usage`（**miss 之后、门判定之前**）掷一次并写入；
`stage_simple_attack_damage` 只**消费**不重掷；每次 query_usage 开头清零。

生命周期：跟"这次攻击"对齐，不跟"这个回合"对齐。

裁决史/引注（用户 2026-09-13 口径）：
- ⚠️ 为什么放 context 而不是 ws：伤害管线分阶段算——先按**当前能力等级**算一次伤害，
  再看是否暴击（×2×(1-暴击抗性)），然后暴击改了能力等级、**变威力技能要按新的
  等级/威力量再算一遍**——重新结算时也必须知道"本次暴击"。
- ⚠️ 为什么必须在这里掷：技能无效（盔/威/封属）时伤害结算**整段不执行**
  （handle_*_AttackDamage 因 allowAttackDamagePipeline==false 早退），在伤害处掷就永远
  掷不到——而"打在盔上一样触发暴击并破防"是官方要求（只有 miss 能阻止暴击）。

---

## hp_consume_marks（struct HpConsumeMark）

一句话语义：消耗全部体力印记（"死亡印记"）——绑定目标的每回合自动结算型印记。

谁写谁读：`attach_hp_consume_mark` 挂印；被动桶节点（AFTER_ACTION_END）结算；
perform_switch / defeat DEFEATED 清表。

生命周期（用户 2026-09-23 口径）：**场上**作用域、**非回合类**效果、**次数无限**。
目标切走（perform_switch 清）或目标真死（defeat DEFEATED 后清）解除；**挂印者**下场
同样失效（结算节点 ON_STAGE 随 epoch 作废 + 表条目同步清，见 perform_switch /
attach_hp_consume_mark）；断回合不杀（非回合类，invalidate_all_round_effects 对被动桶
常驻条目刷 epoch 保命，同常驻魂印待遇）。

裁决史/引注（"赛学必修15"口径 2026-09-22）："消耗全部体力"≠秒杀：绑定目标的印记，
不受免疫秒杀影响；每回合在**挂印者**行动结束自动结算一次——目标体力清 0 后走
defeat_pet(HP_CONSUME)：免死类拦截器不可见（官方 idx=339），复活类可见（真二命每回合
掉一命）。目标被复活拉起 → 印记保留下回合再算（白龙"斩杀延后"）。结算每回合只结一次
（先/后手两个结算节点取先到，`last_settled_round` 记账）。

关联：`clear_hp_consume_marks` / `clear_hp_consume_marks_attached_by`（perform_switch）。

---

## block_offstage_to_onstage

一句话语义：场下源抑制场域（薇尔诗 2513，双方通用）——开启时"场下源→场上目标"
效果被抑制。

谁写谁读：2513 的魂印程序在战斗开始置 true（roster_field.cpp
`soulmark_2513_open_field`）；各"场下源→场上目标"效果入口统一查
（`offstage_source_suppressed` 助手；内核侧消费 = `reclass_offfield_gate_pass`）。

生命周期：宿主阵亡后由**战斗开始注册的死亡监控**（EVENT_DEATH watcher，独立于魂印
开闭路径）清 false——不能用 on_exit 清：ROSTER 魂印切换离场不清（存活于背包仍生效），
死亡路径又会被"跳过时点"类效果（星皇之怒）跳掉。战斗结束清。

裁决史/引注："自身存活于出战阵容时，双方场下的精灵无法指定场上精灵为效果对象"
（effect_icon 2098）。不分敌我——薇尔诗在任一方存活即全场生效（档案 §4.2.3 定：
context 全局，非 per-owner）。判别轴（源是场下精灵还是印记）在各效果入口声明，
场域只提供开关（档案 §4.0）。

关联：`reclass_offfield_gate_pass`（驱逐重分类的非豁免票门）、roster_field.cpp
（903 之赐之佑同族）。

---

## pet_death_notified

一句话语义：死亡漏斗已登记该槽本次死亡的标记——**per-slot**（不是 per-side）：
场下精灵也会死（帝君之陨类效果、消耗全部体力印记、咤连锁击杀），per-side 一个布尔
表达不了"另一方还有几只场下宠各自死没死"。

谁写谁读：defeat_pet 登记置位；复活/重登场时复位。

生命周期复位点：
- perform_switch：登场的那只复位（一只宠一生可被登记多次——死亡→复活→再死）；
- sync_pending_deaths：扫到 hp>0（已复活）时复位。

---

## is_boss_challenge / 双背包模型（standby_*）

一句话语义（is_boss_challenge）：boss 挑战对局旗标——true 时魂印程序里 boss_invalid
标注的节点不注册不执行（子句级 boss 无效）；技能效果侧经 disabledEffects 预填
（见 effect_param.h）。对局创建时由 Room 从 BattleCreateRequest 写入，战斗中只读。

一句话语义（双背包模型）：每边**两张静态背包**——出战背包（6 格，进战斗）+
待命背包（6 格，不进战斗），合计最多 12 只；两张背包战斗创建时定死，战斗中不变。

谁写谁读（待命侧）：BP 模式由 Room 在构造 context 后、init_battle 前写入（只读）；
直开对局不传 → `standby_initialized=false` = 待命背包 6 格全空（老客户端语义，
格劳瑞类读者据此降级并记偏差）。旗标数组由 Room 按 id 走 PetFactory 装配期判定落账
（与出战侧 has_hp_consume_kit / is_spirit_king 同一判据）。

裁决史/引注（2026-09-25 用户口径校准；boss 线 2026-09-26）：
- ⚠️ 战斗 6 格 array（seerRobot[side].elfPets）= **BP（禁选/选序）结果按选序**排布，
  由大厅/客户端决定，与玩家编队的初始精灵排序**无关**——引擎只按到手的顺序消费，
  不做任何"按 id/阵容重排"。
- `is_pvp_battle`（2026-09-28 三视角线）：pvp=true 时 custom_equip_stats 里 pve-only
  行不生效，反之亦然（双 1 行恒生效）。缺省 false = PVE（图鉴/Boss 主语境）。

关联：`standby_initialized` / `standby_pet_ids` / `standby_is_spirit_king` /
`standby_hp_consume_kit`。

---

## suit_effects

一句话语义：套装效果执行表——第三类效果来源（魂印/技能之外）。

谁写谁读：注册入口 `activate_suits()`（内核合并前由场景测试在 start 前手动调，
模拟 init_battle 的自动调用点）；执行顺序**魂印 → 套装 → 技能**。

生命周期：条目一律 TEAM（套装穿在赛尔身上、全队生效、整场不变——用户口径
"生效范围全队，无法被任何效果抹除或者屏蔽"），不经 epoch 作废体系，换宠/断回合/
伤害抑制都碰不到它；clearAllEffects 清桶（下一局重新激活）。

裁决史/引注（2026-09-19 套装线）：官方口径"回合开始时先结算魂印，后结算套装"
（docs_local/docs/02-效果系统/时点判定流程表.md）；"造成伤害前"链里套装伤害前节点
也在魂印之后。

---

## worn_equipment / active_suits / equip_stat_flat / equip_stat_pct

一句话语义：装备/套装**静态数据**（战斗级，开战组装时定，之后只读）。

谁写谁读：构造时从 SeerRobot::equip_item_ids 拷入 + resolve_active_suits 算好激活集；
消费方自行读取并入基线视图。**数值层不是效果**：不进任何效果桶。

裁决史/引注：
- 装备穿在**赛尔（玩家）**身上，效果全队（官方 desc 口径"背包内精灵…"）——所以是
  per-side 而不是 per-pet。
- 激活判定 = 某套的穿戴件数 ≥ 该套 cloths 全长（suit 表部件清单；官方无独立
  "需求件数"字段）。resolve_active_suits 实现在 entities/suit.h，战斗中零判定。
- equip_stat_*：单件/成套数值加成累计（点数/百分比两本账，六维 = 引擎
  NumericalPropertyIndex 序）。来源 custom_equip_stats（离线编码——Unity equip.bytes
  已无结构化数值字段）。

---

## updater_effects

一句话语义：更新器桶——每个"活动中的魂印"登记一个更新器对象，回合边界刷新
once 节点。

谁写谁读：`register_updater` 登记（同魂印同源去重）；FSM 在 ROUND_COMPLETION 末尾
（advanceRound 之后、跳 OPERATION_CHOOSE_SKILL_MEDICAMENT 之前）执行本桶
（`execute_updater_actions`）→ 各更新器重注册自己的魂印节点。

生命周期：条目 TEAM（随魂印存活，不随上下场作废）；宿主阵亡后查找失败即自然失效。

裁决史/引注：为什么需要它——once 节点执行后会被移出桶（回合限一次），原先的重注册
挂在 BATTLE_ROUND_START——它在 CHOOSE **之后**才跑，导致本回合选择期缺失这些效果
（迟到一拍）。更新器在回合边界刷新，保证玩家进入选择期时本回合魂印效果已就位。

---

## grant_penetration

一句话语义：「无视」凭证授予 API（穿盔 / 穿透限伤）——**由 RuleCenter 承载**
（`RuleCategory::PENETRATE_ATTACK` / `PENETRATE_ATTRIBUTE`）。

谁写谁读：效果层授予；攻击技能跑伤害管线时查询并**必然消耗一次**
（`materialize_attack_credential`）；属性伤害走 `deal_attribute_damage` 查询另一条路
（`category` 决定作用路径，**要两条路都管就注册两条**，照 HIT_INVALID 的拆法）。

生命周期：counts>0 = 次数型（每次使用消费一次）；rounds>0 = 窗口型（响应不消耗、
tick 减、断回合清）。

裁决史/引注：
- 为什么不再用手写 vector：那种写法只有**次数**、没有**回合窗口**——"当回合有效的
  全凭证，当回合查询多次都不失效"（薇尔诗·白皑之纷争）根本表达不出来；而且覆盖键
  刷新、断回合作废、换宠清、每回合 tick 全都要自己再写一遍。RuleCenter 这些都已经有了。
- 霍光·无罔之心「下2次**攻击技能**无视」只登记攻击侧 → 属性直伤走另一条路，
  **不命中也不消耗**（用户 2026-09-18 口径）。
- 内联实现：插件动态库不链接 sim_core，需头文件可见（仿 843 先例）。

---

## round_effect_start_round

一句话语义：回合数窗口起算——"N回合内"（InRounds）与"下N回合"（NextRounds）
两套官方通用规则的统一起算点。

谁写谁读：core 注册路径与插件注册路径都要用它算 ContinuousEffect 的 register_round
（否则后出手会少结算一回合）。

裁决史/引注（官方通用规则，用户 2026-09-13 定）：注册**持续 N 回合**的效果时，
官方口径的生效起点不是"注册那一刻"，而是：
- InRounds（"{N}回合内"，默认）：
  先出手 → 本回合就结算一次 → 起点 = 本回合 → 生效 [R, R+N-1]；
  后出手 → 本回合已错过结算 → 顺延 → 生效 [R+1, R+N]；
  ⚠️ N==1（"本回合内"）**不顺延**：它就覆盖本回合，挪到下一回合反而是错的。
- NextRounds（"下{N}回合"，如"下2回合必定先手"）：
  本回合本就不算 → 先/后出手**都**从下一回合起算 → 生效 [R+1, R+N]
  （后出手**不再**额外顺延）。
- 家族声明来源 = 认证数据层（custom_effect_overrides, override_type='window'）→
  EffectMetaCatalog::find(id)->window；插件侧可经 CoreApi::effect_window_kind(effect_id) 查。

---

## rewrite_round_choice

一句话语义：**操作记录改写**的唯一入口——收包转化（玄武 CHOOSE_PET→EXPEL）与机器
填参（BATTLE_ROUND_START 锚点随机槽）都走这里。

谁写谁读：battleFsm 收包转化 + apply_forced_expels 锚点两个写点；下游
should_skip_action_flow / is_skill_action / 先手比较读改写后的记录。

生命周期：随 roundChoice 的回合边界（ROUND_COMPLETION 清）。

裁决史/引注（2026-11-21 驱逐线）：先例 = 朝曦 9901006 在桶节点里直写 roundChoice
（canglan_tears.cpp）——本单出现两个写点后收敛成一处，将来 op 审计/日志加账只改
此函数。改写发生在 handle_OperationChooseSkillMedicament 的 accepted 判定**之后**
（accepted 按玩家原值比对），不影响操作是否被接受。

---

## reclass_offfield_gate_pass

一句话语义：「场下源抑制」门（LEAVE_CAUSE_RECLASS 非豁免票专用）——薇尔诗 2513
场域开启且**源**（改写票持有者）在场下 → 门不通过（票被抑制）。

谁写谁读：battleFsm 收包转化查询桥以回调注入传给
`RuleCenter::has_leave_cause_reclass`（rule_center 只持 forward decl，判据要完整
BattleContext，所以门住在 context）。

生命周期：无状态查询——block_offstage_to_onstage 的生命周期即门的生效窗。

裁决史/引注（2026-11-21/22 用户裁决）：判据同 roster_field.cpp 的
offstage_source_suppressed 后半（源已消逝由票的 condition 前置拦，走到这里必在背包）。
全局开关 = rule_center.h 的 kReclassSubjectToOfffieldBlock（默认豁免 = 官方双标现状；
一视同仁翻常量即可）。场景 8740 F 组三态锁定。

---

## find_pet_with_soulmark

一句话语义：魂印源查询——在 owner 方 6 槽中查"携带指定魂印 id 且存活"的精灵槽位；
未找到返回 -1。

谁写谁读：效果执行时判定"源精灵此刻在场上还是场下"——
`== on_stage[owner]` → 源在场上；不等（或 -1）→ 在背包/已阵亡。inline 成员，供插件
（moves_lib/soul_lib 不链接 sim_core，CLAUDE.md 3.9）直接调用。

生命周期：无状态查询。

裁决史/引注：
- 典型场景：薇尔诗 2513 场域抑制"场下源→场上目标"；瀚宇星皇 903 判"星皇是否场下"
  （见 docs/02-效果系统/魂印机制设计与精灵表现档案.md §4）。
- 前提：每方同魂印注册唯一（单边同 ID 精灵只能带一只，且同名魂印效果不叠加），
  不存在多槽命中歧义（档案 §1）。
- 只认存活精灵——各子句普遍写作"自身存活于出战阵容时"，阵亡不应继续提供效果。
- 已消逝同样不提供（官方 idx=172"涉及到队友存活或者死亡收益的，默认会被消逝"；
  且消逝蕴含阵亡）——hp>0 判据天然覆盖，消逝的宠 hp 也已归零，无需额外条件。

---

## forced_expel_pending

一句话语义：强制驱逐待决位（effect_des 402 通用机制）——置位侧的下次操作阶段被
系统强制随机换宠，操作位被占用（面板锁死只留空过）。

谁写谁读：**单写者 = 效果插件**（混元天尊线"辛死亡驱逐对手"等触发方，内核从不
set——分工对称 is_locked）；三消费者 = ① compute_legal_actions 锁面板（合法集合只剩
空过）② FSM 收包校验（只认 NONE）③ BATTLE_ROUND_START 锚点
`apply_forced_expels`（守卫 + 随机补位 + 机器填 EXPEL op）。

生命周期：置位后到下一次 BATTLE_ROUND_START 锚点必被消费（守卫失败也清），无跨回合
残留；不必在 ROUND_COMPLETION 重置。

裁决史/引注（2026-11-21/22 用户裁决）：
- 守卫（effect_des 402）：对象体力 0 或背包无其他存活精灵 → 无法被驱逐。
- 随机补位候选**排除 is_locked**（神殇"不能切换上场"按字面管住一切上场途径；
  与死亡补位 is_forced 绕锁是两条规则，别"对称化"）。
- pet 级标志被否决：驱逐管的是"系统替你提交什么"而非"能提交什么"，ctx 级单写者
  标志 + op 类型作唯一下游载体即可。
- 锚定：effect_icon 行 2144（玄武 2553 重分类同机制家族）、effect_des 402/403。

关联：`apply_forced_expels`（battleFsm）、`rewrite_round_choice`、
`ActionType::EXPEL`、compute_legal_actions 的 expel_locked。

---

## reclass_offfield_gate_pass / forced_expel_pending 的机制族总览

玄武 2553「双方的主动下场均视为驱逐下场」（effect_icon 行 2144）与混元天尊真驱逐
（effect_des 402/403）共享同一套切出成因设施：SwitchCause 三档 + EXPEL 内部操作 +
LEAVE_CAUSE_RECLASS 改写票（场域管辖开关 kReclassSubjectToOfffieldBlock，默认豁免 =
官方双标现状）+ SwitchCause 事件载荷。内核耦合点登记：
docs/07-工具与测试/内核耦合效果清单.md §5。场景：scenario_8740（F 组锁场域开关三态）。
