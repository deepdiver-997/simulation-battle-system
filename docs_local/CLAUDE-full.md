# CLAUDE.md — 回合制精灵对战模拟系统架构地图

本项目是一个回合制精灵对战模拟引擎（C++17，状态机驱动）。本文件是**架构心智地图**：先看"数据流"建立整体感，再看各子系统的职责与关键文件，最后是必须避开的坑。

---

## 0. 一句话

**数据（SQLite）→ Skills/SoulMark 程序化效果 → 注册进"时点桶" → FSM 逐时点驱动 → 桶里的效果在对应时点执行并读写 BattleContext。**

每次修改前先确认改的是哪一层：数据层（scripts/）、组装层（entities/skills.cpp）、内核层（effects/、primitives/）、调度层（fsm/battleFsm.cpp）、还是插件层（resources/*_lib/）。

---

## 1. 构建与运行

```bash
cmake -B build                    # 首次配置
cmake --build build -j8           # 增量构建
./build/sim_smoke_test            # 冒烟（test/test.cpp，本地未提交）
./build/sim_scenario_001_conditional_first_move   # 场景测试（每场景一个二进制，GLOB 自动发现）
```

**必须从项目根目录运行测试**——`PetFactory::initialize_runtime_data()` 用相对路径 `scripts/data/processed/seer_unity.sqlite` 初始化 DB 与插件。

构建目标：
- `sim_core`（静态库，引擎本体）
- `sim_server` / `sim_training_cli`（服务端/训练 CLI）
- `sim_scenario_0NN_<suffix>`（场景化对战测试，`test/scenario/scenario_0NN_*.cpp`，CMake GLOB 自动发现）
- `moves_plugin` / `soul_plugin`（动态库，技能/魂印效果实现）

---

## 2. 目录地图

```
include/                    # 头文件（大量逻辑内联在头里，插件依赖这个）
├── fsm/                    # 状态机：battleFsm.h(状态机)、battleContext.h(所有状态收纳盒)、
│                           #   battleWorkspace.h(每回合重置的临时层)、state.h(State 枚举)、iControlBlock.h
├── effects/                # 效果内核：effect.h(Effect/EffectFn/EffectArgs)、continuousEffect.h、
│                           #   effect_unit.h/effect_unit_parser.h(组合语法)、event_center.h、
│                           #   immunity_center.h、damage_pipeline.h、pink_damage_pipeline.h、effect_meta.h
├── entities/               # 组装层：skills.h、soul_mark.h/soul_mark_manager.h、elf-pet.h、pet_factory.h
├── primitives/             # battle_primitives.h：原语（断回合/能力/恢复/固伤/施加异常）
├── abnormal-system/        # 异常状态定义（abnormal-types.h）
├── db/                     # 官方 SQLite 访问（official_data_repository.h）
├── numerical-calculation/  # 伤害公式（calculation.h）
├── plugin/                 # plugin_interface.h：插件注册接口 + SoulMarkNodeRef
└── utils/                  # dynamic_library.h（动态库加载）

src/                        # 与 include 镜像的 .cpp 实现
resources/moves_lib/        # 技能效果插件（lib_1.cpp，不链接 sim_core）
resources/soul_lib/         # 魂印效果插件（lib_1.cpp，不链接 sim_core）
test/scenario/              # 场景化对战测试（scenario_harness.h + scenario_0NN_*.cpp，索引见 docs/07/场景测试说明.md）
scripts/                    # 官方数据抓取/导入（fetch/import *.py）
docs/                       # 按主题分目录的文档（01-架构与设计 / 02-效果系统 / ...）
reference/                  # 原始参考资料（官方专栏 JSONL 等）
```

---

## 3. 核心架构

### 3.1 数据流：一次攻击的旅程

```
玩家提交操作(CHOOSE) → FSM 推进
  → BATTLE_FIRST_MOVE_RIGHT     先手权结算（preemptive_right）
  → BATTLE_FIRST_ON_SKILL_HIT   技能执行判定：skills.execute() 返回 SkillExecResult + flags
       ├─ HIT        → 注册正常效果分支到各时点桶
       ├─ SKILL_INVALID → 注册 miss/技能无效补偿分支
       └─ EFFECT_INVALID → 不注册技能效果
  → BATTLE_FIRST_ATTACK_DAMAGE  stage_simple_attack_damage() 填 resolvedDamage →
       apply_crit_defense_break() → execute_bucket_actions(魂印桶, 技能桶) →
       [变威力/连击：威力视图被改过 或 combo>1 → 再 stage 一次，覆盖第一次] →
       damage_pipeline_.run() → apply_resolved_damage()
  → ... 各"行动后"时点 → BATTLE_ROUND_COMPLETION → 回到 CHOOSE（下一回合）
```

### 3.2 战斗状态机（FSM + 时点桶）

- `BattleFsm::run(BattleContext*)` 按 `State` 线性序推进；`stateHandlerMap` 把每个 State 映射到 handler。
- 每个 handler 里调 `execute_registered_actions(robotId, State)` —— **这是效果执行的唯一入口**。
- `State` 枚举在 `include/fsm/state.h`（`-1..39`），有 `state_name_cn` 中文名、`state_coverage_bit/coverage_union/coverage_all`（免疫覆盖位图）。
- **时点桶 = `TimedBucket`**（`include/effects/timed_bucket.h`）：`BattleContext::skills_effects` 与 `soul_mark_effects` 都是它的实例。
  - 内层是 `unordered_map<State, array<map<uint64_t, unique_ptr<ContinuousEffect>>, 2>>`：第一维=触发时点，第二维=玩家(0/1)。
  - `map` key = `(source_id << 32) | effect_id`（`timed_effect_key`）→ **同源同 effect 新注册覆盖旧**（去重）；`source_id==0` 用自增唯一 key 不去重。
  - 类封装了**注册/去重/执行/过期清理/epoch 作废/回合计数**全生命周期；**回合计数在桶内**（不再有与桶同步的影子索引），`has_round_effects` 是两桶求和。
  - 新增一类"按 State 触发的容器"（如计划中的回合首时点**更新器桶**）→ 多声明一个成员即可。
- **魂印桶先于技能桶执行**（battleContext.cpp `execute_registered_actions`）——魂印优先级是结构性保证的。
- **插件注册效果走 inline 入口**：`ctx->register_skill_effect(...)` / `ctx->register_soulmark_effect(...)`（头文件内联，见 3.9）；断回合用 `ctx->invalidate_all_round_effects(target)`。插件**不要**再手抄引擎的 valid_id/计数/key 逻辑。

### 3.3 效果系统

- `EffectFn = EffectResult(*)(BattleContext*, const EffectArgs&)`。`EffectArgs` 是 int 数组 + 可选 extra 指针。
- `Effect` = 模板（id + 函数指针 + 参数），不可变；`ContinuousEffect` = Effect 实例化（注册回合、持续回合、owner、作用域）。
- `ContinuousEffect` 关键字段：`source_id_`（去重来源）、`scope_`（ON_STAGE=切换作废 / TEAM=切换保留）、`valid_id_`（版本号，断回合/切换作废用）、`once_`（回合限一次，执行后由执行器移除）。
- **原语**（battle_primitives.h）：`break_round_effects`、`stat_change`、`heal`、`fixed_damage`、`apply_anomaly`、`pp_reduce`、`remove_round_effects`（复用 break）、`drain_hp`、`drain_hp_amount`、`kill`、`stat_reversal`（反转下降）、`clear_stat_boosts`（消除提升）、`clear_stat_drops`（消除下降）、`transfer_stat_boosts`（转换/吸取提升）、`stat_drop`（弱化）、`stat_boost_reversal`（反转对手提升）、`deal_pink_damage`（粉伤·指定数值，插件造粉伤的唯一入口）——效果函数通过原语做原子动作；原语动作前查免疫。
- **能力等级六条通道，语义各不同（别混用）**：`stat_change`=原始变更（自身增益/自降，**不查免弱**，越界 AT_CAP 拒绝）；`stat_drop`=**弱化**（先查 `STAT_DROP` 免弱且**无条件**——目标身上有 +N 也照样失败，不拿提升抵消；**可穿** `STAT_CLEAR` 强化保护，原理不同；钳 -6 不越界）；`stat_boost_reversal`=**反转对手提升**（正→等负，同属弱化类动作 → **先查免弱**返回 BLOCKED；不查 `STAT_CLEAR`）；`stat_reversal`=反转**自身下降**（增益类，不查免疫）；`clear_stat_boosts`/`transfer_stat_boosts`=消除/吸取提升（查 `STAT_CLEAR`）；`clear_stat_drops`=消除下降（**故意不查任何免疫**——清弱化对目标有利，免弱挡"施加弱化"、免消除强化护"提升"，两者都不该挡"把弱化拿掉"。用户 2026-09-13 口径："清理弱化什么都不用查"）。**六者都要同步 `ws.view_levels`**（本体/视图分裂会打出 INT_MAX，见 §5.8）——
  ★ 2026-09-18 起这六条通道**全部收口到 `write_ability_level()`**（battle_primitives.cpp 内部）：
  它一次做三件事 = 写本体 + 写视图 + emit `EVENT_STAT_CHANGED`（载荷 stat_index/level_after/level_delta；
  **无 actor**，原语本身不带归因参数）。**新增改等级的路径必须走它，别直写 `ctx->ability_levels`**，
  否则「监测等级变化」一类效果（窗口型压制，代表 3414 混沌魔君索伦森 1011 的子句②）会静默漏掉这次变化。
  同批新增票 **`STAT_BOOST`（「无法处于能力提升状态」）**：`stat_change`(delta>0) /
  `transfer_stat_boosts` 的 to 侧 / `stat_reversal`（负翻正）三处查询，命中返回 `StatChangeResult::BLOCKED`。**恢复类（heal/drain 自身恢复）**：查 `HEAL_BLOCK` 封回血（位覆盖时点）+ 应用 `ctx->heal_mod_pct[target]`（恢复效果修正%）+ 记 `ws.last_heal_amount[target]`（吃月亮二类用实际恢复值）。
- **组合语法第二刀**：`EffectUnit` 加 `condition`（UnitCondition：SameElement/FirstMove/SecondMove/TargetNoAnomaly/TargetHasAnomaly/TargetHpBelow），执行器在概率 roll 前求值条件，不满足不触发。parser 识别"条件→动作"模板（无相谛 179 威力/700 降PP/1083 清回合/1257 吸取/456 秒杀）。
- **技能威力视图层**：`ws.skill_power_view[2]`（类型是带标记的 **`SkillView`**，见 §3.8d），攻击时 resolve_skill_execution **物化**（`materialize()`，不打标记）skill.power，效果（SKILL_EFFECT 时点）可改，calculateDamage 从 ws 读最终值——为 179 及随机/累积威力效果提供统一落点。**效果写它 = 声明"这是变威力"**。
- **技能元素/克制倍率视图层**：`ws.skill_element_view[2][2]` + `ws.restraint_view[2]`（-1=未设置）。克制按**技能系别视图 vs 防御方元素**算（官方机制：克制=技能系别 vs 防御方系别，**非攻击方精灵系别**）；restraint_view>=0 直接覆盖（"不微弱"钳到1、"不计算克制"设1）。效果在 SKILL_EFFECT 改视图（如"以自然/光/冰/超能/神灵中最优系别算克制"写 skill_element_view）。**本系加成(involve) 仍用技能真实系别 skill.element**（改系别只改克制、不改本系）。
  ⚠️ **天敌判定**（"遇到天敌时效果翻倍"类，如落芳天华 497）是**另一条**：看**对手固有属性是否克制自己固有属性**
  （`calculateRestraintMultiples(对手 elementalAttributes, 自己 elementalAttributes) > 1`），**不看技能属性、也不看属性视图**
  —— 实测：龙琰改了系别也不影响天敌翻倍（`pet.elementalAttributes` 是固有属性，改系别只写视图）。
- **精灵系别半持久化视图**：`ctx->elf_element_view[2][2]` + `elf_element_view_bound_slot[2]`（绑定的精灵槽）。改系别效果（属性反转/龙琰）写它——跨回合保留（ws 每回合 reset 会清，故放 context）；`sync_workspace_from_on_stage` 每回合拷到 `ws.view_elementalAttributes`，**槽变化时从新精灵重基**（bound=-1 开战基线；换宠 invalidate 置 -1 自动重基），同槽保留效果改动。
- **粉伤伤害档（DamageKind）**：`NORMAL`(红伤) / `FIXED`(粉伤·固定档) / `PERCENT`(粉伤·百分比档，amount 是最大体力百分比) / **`PERCENT_VALUE`**(粉伤·百分比档，**amount 已是具体值**——「附加自身已损失体力50%的百分比伤害」这类) / `TRUE`。插件侧统一走 `CoreApi::deal_pink_damage`（插件不能直调 deal_damage）。
  **克制倍数查询**走 `CoreApi::restraint_multiplier`（克制表是运行时从 DB 加载的全局，插件拿不到该符号）——
  **天敌**判定即 `restraint_multiplier(对手固有属性, 自身固有属性) > 1`（官方实测：不看技能属性/不看视图）。
- **粉伤抗性系统**：粉伤两来源=固定（"吸取对手xxx体力"）/百分比（"吸取最大体力1/2"）。**伤害抗性三种（官方）：暴击/固定/百分比**。`deal_damage`（FIXED/PERCENT）护盾前过 `pink_immune[2]`（免疫粉伤）→ **对应来源抗性** → `pink_reduce_pct[2]`（减粉%）→ 被挡且 `pink_to_true[2]` 改以 TRUE。**暴击**：query_usage ①.5 按 `skill.critical_strike_rate * crit_rate_mod` roll（默认 0），`stage_simple_attack_damage` 里**先乘暴击系数（整段放大，默认 200）再乘暴击抗性（也是乘整段）**—— 100 伤 → 暴击 200 → 35% 抗性 → **130**（不是 100+100×65%=165）；100% 抗性 → **0**。L99 同源：「暴击系数×暴击抗性…折算为 2×0.65=1.3」。免粉补偿一类=效果侧比较 HP（挡下则 HP 不变→补偿）。
- **⚠️ 伤害抗性本体 vs 有效视图（两层）**：
  - **本体** `ElfPet::damage_resist`（crit/fixed/percent）——精灵自身属性，**跨切换保留**。
  - **有效视图** `ws.eff_*_resist_pct[2]`——伤害计算**只读这里**。基线由 `sync_damage_resist_view` 在**回合开始 / 换宠**时从本体重基（`ws` 每回合 reset 会清视图，故必须重基）。
  - 临时 buff 改抗性（如混元天尊死亡 buff 把己方精灵抗性**视为 100%**，3 回合后恢复）**写视图不写本体**；且只提升**非 0** 的抗性（本体未开抗性=0 则 buff 不生效）。buff 效果在回合内重新施加（视图已随 reset 清空）。
  - 原先抗性放在 `BattleContext[2]` 且换宠被 `invalidate_on_stage_effects` 清零 —— 精灵**自身**的抗性切走再切回来就丢了（已修，场景 N8 验证）。
- **⚠️ Skills 拷贝重定基**：`Skills` 拷贝构造须把引用 `parsed_units_` 元素的 Effect `args.extra` 重定基到新 vector（否则宠物拷贝后组合语法效果悬垂崩溃）。`rebase_parsed_unit_pointers`。
- **万相乖离条件取消**：`Skills::cancel_next_parsed_condition()`（头内联）把 parsed_units_ 第一条 condition!=None 置 None（永久无条件）；2260 PP=0 触发它 + skill.power+50 + 持久槽计数。场景 J 验证 3 回合逐条取消（power 0→150）。
- **⚠️ parser 定位**：执行器（EffectUnit+原语+分支）是稳定的运行时表示；**文本解析器是脆弱层**（官方换措辞就挂）。方向：新模板由模型离线编码，不扩运行时文本解析。
- **回合效果过期**：注册时记 `registered_round`，`isExpired` 判断 `currentRound - registered_round >= duration`。`cleanup_expired_effects()` 在 `BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS` 统一清理。
  ⚠️ **先/后出手的回合数判定（官方通用规则，用户 2026-09-13 定，已实现）**："N回合内"类效果，**先出手**生效 `R..R+N-1`（本回合就结算一次）、
  **后出手**顺延为 `R+1..R+N`（N≥2；N==1"本回合内"不顺延）；「**下**N回合」则先/后出手**都**从 `R+1` 起算（不叠加顺延）。
  折算入口：`BattleContext::round_effect_start_round(owner, duration, kind)`（inline，插件可调）；
  家族声明 = **认证数据层** `custom_effect_overrides(override_type='window', map_value='next_rounds')` → `EffectMeta::window`
  → 插件侧经 `CoreApi::effect_window_kind(id)` 查（core 的 `register_branch` **只对 `left_round>0` 的窗口效果**折算，
  一次性动作节点不挪）。场景 038 验证；详见 `docs_local/docs/02-效果系统/官方机制理解与引擎缺口对照.md` §五.0 /
  `docs_local/docs/03-数据与数据库/认证数据层.md` §3.1。

### 3.4 技能模型（运行时组装）

`Skills`（include/entities/skills.h）：
- `effectBranches`: `map<SkillExecResult, vector<SkillEffectNode>>` —— 按执行结果分叉；每个节点带 `registerState`（注册到哪个时点）+ 可选 pending 触发。
- `selection_effects_`: 选技能时立即注册的效果（先制等）。
- **技能替换 = 重定向，不拷贝技能对象**，两级**按生命周期拆两个载体**（场景 T/V）：
  `resolve_replacement_skill` 统一解析（pending 优先）→ `resolve_executing_skill` 唯一取用点
  （effectBranches/威力/系别/暴击/伤害公式）。艾欧丽娅式 = `ws.skill_effect_source[2]`
  （ROUND_START 后由效果设，随 reset 自动失效；PP 与 selection_effects_ 走原槽位，固有效果保留）；
  米修莉式 = `ctx->pending_skill_replacement[2]`（操作提交瞬间、抢在 on_selected 前生效——
  固有先制根本不注册即"失去天生先制"；用后即耗（结算完消费）、换宠即清（"下场不保留"））。
  权威口径见技能判定流程与无效效果体系.md §七/§7.5。
- **三阶段生命周期**：
  1. 选择期 `query_selectable`（PP/封印检查）→ `on_selected`（注册先制）。
  2. 执行期 `query_usage`（miss/封效果，此时物化攻击凭证）。
  3. 主流程 `execute` → `register_branch` 把结果分支注册到各时点桶。
- `loadSkills()` 从 DB 读 moves/side_effect/effect_info；已注册函数走插件，未注册模板走 `effect_unit_parser`（组合语法）。

### 3.5 魂印程序模型（与技能对等）

`SoulMark` = "程序" = `vector<SoulMarkNodeRef{State trigger_state, EffectFn effect_fn, bool once, bool early, SoulScope scope}>`：
- 按 `trigger_state` 注册进**魂印桶**（对等技能的 effectBranches，但坍缩掉"执行结果分叉"维度）。
- **`scope` 决定注册条件与作废语义**（`plugin_interface.h`）：
  - `STAGE`（默认）：随上下场——只在宿主上场时注册，`ContinuousEffect::scope_=ON_STAGE`（离场 epoch 作废）。
  - `ROSTER`：常驻——宿主**存活于出战背包即注册**（不要求在场），`scope_=TEAM`（跨切换保留、不被断回合）。星皇 903 / 薇尔诗 2513 用这个。
  - ⚠️ `early` 节点同样受 scope 约束（否则场下魂印会把信号错误施加给场上精灵）。
- **注册时机（★ 关键）**：`activate_soul_mark`（登场：战斗开始 + 切换上场）+ **更新器桶**（回合边界）。
  - 战斗开始 `handle_OperationEnterExitStage` 扫**全部 6 槽**（不是只 on_stage），同魂印 id 每方只激活一次、场上槽优先。
  - `once`（回合限一次）执行后从桶移除，靠**更新器桶**在回合边界重注册复位。
  - ⚠️ **不要再挂 `BATTLE_ROUND_START` 重注册**——线性序里 ROUND_START 排在 `OPERATION_CHOOSE_SKILL_MEDICAMENT` **之后**，在那里注册会让本回合选择期缺失 once 效果（"迟到一拍"）。更新器桶在 `ROUND_COMPLETION` 末尾（advanceRound 之后、跳 CHOOSE 之前）执行，恰好避开。
- **钩子**（魂印级，可选，插件提供）：`on_enter`（登场时，引擎重注册之外）、`on_exit`（**纯 STAGE** 魂印切换下场 / 阵亡时，清理"不在桶里"的状态如 2513 的全局抑制标记）。桶内条目靠 epoch 作废，不需要钩子。插件注册：`registerSoulMarkProgram(id, nodes)` + `registerSoulMarkHooks(id, hooks)`；旧单效果 `registerSoulMark(id, fn)` 兼容（其桶注册改由 `activate_soul_mark` 完成）。
- **判定"源此刻在场上还是场下"用 `ctx->find_pet_with_soulmark(owner, id)`（返回槽位，与 `on_stage[owner]` 比较）**；"场下源 → 场上目标"类效果入口必须查统一门 `offstage_source_suppressed(ctx, owner, mark_id)`（soul_lib 内联 helper：场域开 && 源场下 → 抑制；源已死 → 恒抑制）。
- **已落地的 ROSTER 参考实现（2026-09-11，场景 U）**：薇尔诗 2513 场域（`ctx->block_offstage_to_onstage` 全局 bool + **战斗开始注册的 EVENT_DEATH 死亡监控**清场域——独立于魂印开闭路径，防"跳过时点"类效果残留）；瀚宇星皇 903 之赐（AFTER_ACTION_END 回 1/8）/之佑（PROTECTION_1 授 3 回合免异常+免弱）。**两个坑**：①开场授予不能与开场场域同桶（ENTER_EXIT_STAGE 桶按 source_id 升序跑，授予放 PROTECTION_1）；②`grant_immunity` 同 source_id 二次授予会覆盖前条 type（免异常+免弱要各占一条）。
- 机制设计档案（机制↔精灵表现追溯）：`docs_local/docs/02-效果系统/魂印机制设计与精灵表现档案.md`。
- **对比技能**：技能要按执行结果分叉注册（HIT/补偿），魂印不需要——魂印常驻注册、触发时点固定，条件判断放进效果函数内部。

### 3.6 异常 / 免疫 / 断回合

- **异常状态**：`abnormal_status_end_round[player][status]`（失效回合号），`apply_anomaly` 是唯一施加入口。
- **统一规则容器 `RuleCenter`**（`include/effects/rule_center.h`，header-only 插件可直调）：**免疫 + 盔/威/封属(含 `SEAL_ATTRIBUTE_HIT` 命中失效) + ③层命中失效 轧为一个容器**（原 `ImmunityCenter`/`SkillInvalidCenter`/`hit_effect_invalids` 合并）。每条 `RuleTicket` 显式带 **source(挂载方)→target(生效对象)** 两维——免疫单对象(source==target=被护方)；封属 source=施放方、target=被封方。覆盖键 `(source_owner, source_effect_id, category, subtype)`：**同来源同效果覆盖、不追加**，subtype 区分使免异常+免弱(903)各占一条；⚠️ 免疫的 `source_effect_id` 默认 `-1`（匿名来源）→ **同类型免疫互相覆盖**，"窗口类免控 + 次数型次免"要并存必须各传自己的 effect_id。次数型免疫 `counts>0`（"免下N次"）由 `is_immune*` 判定、`consume_immune` 消费（**扫描会跳过窗口条目**——官方"存在回合类免控时次免照常消耗"；被弹回的异常不消耗）。**生命周期锚 source**：施放方换宠清 ON_STAGE、断施放方回合清回合类封属（"封属属于施放方"；免疫挂被护方自身）。三组入口：`is_immune*`(纯查询不消费) / `notify`(扫 target==user 的 SEAL 消费次数→三态 INVALID/HIT_INVALID/NONE─不触发补偿走 `execute` 命中失效路径) / `consume_hit_invalid`(③层防御方按键)。权威口径见 `docs_local/docs/02-效果系统/技能判定流程与无效效果体系.md`。
- **异常抗性（纠正优先级）**：`ElfPet::resistance`（ResistanceSystem：控制/非控制槽 + 全免疫%）。apply_anomaly 顺序：**①次免/回合类免疫(soul=false) → ②异常抗性 roll → ③魂免(soul=true)**。抗性成功**直写附加"免疫异常"异常(21, 2回合)——击穿魂免**（不走免疫检查），先走转化（exploit：攻击方预置 `anomaly_conversion[target][21]=Y` → 抵抗结果直接转成 Y）。
- **反弹/转化异常**：`ctx->reflect_anomaly[2]`（弹控=免疫+反弹给施放方，**最多反弹 1 次防双方弹控打乒乓球**，弹回不过抗性、可免疫、可转化）；`ctx->anomaly_conversion[2]`（`std::map<int,int>` 入→出，单跳）。
- **断回合**：`++round_effect_valid_id[owner]` 使旧回合效果版本号失效（O(1)）；`break_round_effects` 是原语（查免疫 + 发 `EVENT_BREAK`）。被断补偿 = 监听 `EVENT_BREAK` 的 watcher（`register_break_callback`）。
- **`BattleEvent::state`（emit 当时的时点）**：事件是「入队 + handler 跑完后 drain」，回调里 `ctx->currentState` 已被
  `generateState()` 推到**下一个**状态，判不出「这次伤害发生在哪个时点」（如「受到**攻击伤害**后」要区别于粉伤）。
  `deal_damage` 的 EVENT_TAKE_DAMAGE 会在 emit 时把 `ctx->currentState` 记进 `ev.state`，回调读它即可。
- **死亡/上场事件**：`EVENT_DEATH` 的收敛发射点是 `handle_BattleAfterDefeated`（线性序每回合必经；on-stage 死亡方在此 emit，`death_notified[2]` 每方一次、`perform_switch` 复位）——"战斗开始注册的死亡监控"类 watcher（如 2513 场域死后清除）监听它，独立于魂印开闭路径。`EVENT_ENTER_STAGE` 在 `perform_switch` 统一 emit（主动中切 + 死亡换宠的共同漏斗，actor=登场方）——"下一只登场精灵"类 watcher（自爆传承，场景 W）监听它；⚠️ 此类监控必须 `once=false` + 回调内过滤 actor + 自删（`remove_watcher` 自身 id），不能 `once=true`——过滤不命中也会被 once 消耗（对手先切人监控就静默消失）。
- **切换作废**：`invalidate_on_stage_effects(owner)` 递增版本号 + 清穿透授予/命中失效/魂印信号（`force_execute_on_pp0`/`ignore_pp`/`pp_reverse`）/粉伤抗性/弹控/转化规则 + `elf_element_view_bound_slot=-1`（新精灵下次 sync 重基系别）。

### 3.7 伤害管线

- `stage_simple_attack_damage` 填 `resolvedDamage`（含 base/afterAdd/afterMul/final/crit）。
- `damage_pipeline_.run()` 按 DamagePhase 顺序执行双方伤害效果（**GUARD_DETECT→AMP→AMP_EXTRA→REDUCE_FLAT→REDUCE_PCT→FLOOR→CAP→BLOCK→DETECT→FINAL_CORRECT**，见 §3.8f），读写 `resolvedDamage`。**减伤 `MITIGATE` 与挡伤 `BLOCK` 是两个类别**：官方"使对手挡伤失效"（沧岚 蚀砚之泪≥4滴 = `damage_suppress_mask |= BLOCK|DETECT`）只废归零类挡伤，**减伤不可被无视**。次数型免伤（"免疫下N次攻击伤害"）由 `install_default_damage_block()` 挂在 BLOCK 阶段，命中即整段归零 + `consume_immune` 扣次数（被掩码压制时**不触发也不扣**）。
- **增伤也走管线**：`install_default_damage_amp()`（AMP 阶段，AMP 类别，只读**攻击方**桶）把
  `ws.damage_add_pct[attacker]`（%）与 `ws.damage_add_flat[attacker]`（固定值）接进管线
  （`final = final*(100+pct)/100 + flat`）——这两个 ws 字段此前是**死字段**。
  ⚠️ **回合效果不要直接注册管线回调来表达"N回合增伤"**（如 776 伤害翻倍 / 693 伤害+150%）：
  管线回调注册后永久驻留，既不随回合过期也无法被断回合移除。正确做法是**回合效果每回合写 ws**
  （ws 每回合 reset 天然清空 + 效果走时点桶 → 断回合/切换作废免费获得），见 moves_lib `effect_set_damage_amp`。
  同一理由体现在免伤上：**窗口型**免伤（525）用"管线回调 + 自算回合窗口"（因为它要逐次掷概率、
  且必须吃 BLOCK 类别的挡伤失效掩码），**次数型**免伤（虚无/2006）用"管线回调 + counts"。
  两类都已知边界：**管线回调不随时点桶作废**（断回合不解除），待官方口径核对后再定。
- **克制倍率**：`elementalAttributesRestraints`（`vector<vector<double>>`）存官方 types_relation 原始倍率 {0免疫, 0.5减半, 1普通, 2克制}，`calculateRestraintMultiples` 双属性组合相乘。⚠️ 旧实现压缩 int 三档把 0.5 减半丢成免疫（半克制伤害归 0），已修复。
- **命中效果失效③层**：已并入 RuleCenter（`HIT_INVALID` 类，target=被失效方，按键次数）——`consume_hit_invalid`（kEffectsOnly 保留伤害 / kFullNull 白板归零）；`force_execute` 绕过（skills.cpp `is_hit_effect_invalid` 上层判断）。
- **护盾/护罩分离**：`pet.shield_bank_`（护盾，**只响应红伤 NORMAL**）+ `pet.hood_bank_`（护罩，**只响应粉伤 FIXED/PERCENT**），结构同 `ShieldBank`（多来源+优先级消耗），独立实体。**真实伤害 TRUE 直通**（护盾+护罩都不响应）。`ws.ignore_shield[2]`（按攻击方）：效果置位后本次攻击无视护盾/护罩响应（如无极圣武魂印"自身攻击无视护盾承伤效果"）。

### 3.8 穿透凭证

- `ws.attack_credential`（攻击时物化：必中/无视免疫/无视伤害限制/强制执行/等级）。
- `penetration_grants`（次数授予，"下N次攻击无视"），成功攻击后 `consume_penetration_grants_after_attack` 统一消费。
- `rule_center_.grant_seal`（技能拦截，封属性/封攻击）：**挂到施放方 source**（生命周期锚——施放方换宠清 ON_STAGE、断施放方回合清回合类；被封方换宠保留）。每条 `RuleTicket` 带 `target`（显式封锁对象）/`source_effect_id`（覆盖键）/`remaining_counts`（次数型，响应即减 0 注销 / `remaining_rounds`（回合型，tick 递减 + 可被断回合清除）/`penetrable`/`SealKind`（含 `SEAL_ATTRIBUTE_HIT` 命中失效）。
  **概率封属**（695「{0}回合内{1}%令对手使用的属性技能无效」类）走 `RuleTicket::condition`：`seal_skill(..., chance_pct)` 在 chance<100 时挂一个"每次响应掷一次"的条件（**不是**授予时掷一次——官方表现是这几回合里每回合都有机会封住）。条件在 `notify` 逐条求值，不中就当这条没响应（不消耗次数）。
  **被穿消不消耗次数**（★ 用户 2026-09-13 实测）：**看盔模板有没有后续子句**——带子句（2270「触发成功则…」/2006「免疫成功则…」/1288/1293）被穿**消耗**；裸的"免疫下{N}次对手的攻击"（570/2269/2203）被穿**保留**。引擎 = `RuleTicket::consumed_when_pierced`（默认 false）。**回合类不看这个**——被穿是否结束取决于穿盔技能有没有断回合。
  **带子句的盔的子句怎么挂**：不要特判"被穿"——`notify` 只对**真正生效**的盔回调 `on_armor_triggered`，core 据此发 `EVENT_SKILL_ARMOR_TRIGGERED`（`amount` = 盔的 source_effect_id）。插件按 effect_id 挂 watcher；**被穿的根本不走这条 → 子句天然不触发**。
  **强制执行 vs 盔**（★ 用户 2026-09-13 实测）：**狮盔/龙威一定把伤害降为 0，强制执行也不例外**——它只"让效果生效"。落点 = `query_usage` ②.0：盔/威/封属**照常响应与消耗**（notify 照跑）但**不拦截**（返回 OK → HIT 分支注册效果）+ **`ws.skill_power_view[owner]` 置 0**。唯一例外是带"**重新进行伤害结算**"词条的（effect **2501**，全库仅此一条）——它属于 **SKILL_INVALID 分支**，在 SKILL_EFFECT 时点改写视图威力 → 被早退出口的变威力检查点抓到 → 隔着盔重算并打出红伤（场景 046）。⚠️ 因此 `calculateDamage` 的"未物化"哨兵是 **-1**，**视图威力 0 是合法值**。⚠️ 公式有 `+2` 常数项 → 视图威力 0 时仍打出 **1~2 点**红伤，不是严格 0。

### 3.8b 暴击 + 暴击破防（2026-09-13，场景 040）

- **只在攻击技能上掷**；暴击率 = `moves.crit_rate`（**分母 16 的分子**，`0` = **0/16 = 永不必暴**
  ——确实有天生暴击率 0 的技能，没有引爆效果就永远不暴） × `ws.crit_rate_mod[owner]`。
  效果改 `crit_rate_mod`（"下N回合暴击率提升"类**每回合写 ws**，同 `damage_add_pct`）。
- **判定位 = miss 之后、门判定之前**（`Skills::query_usage` ①.5 步）→ **只有 miss 阻止暴击**；
  盔/威/封属、命中效果失效（③层）都**保留**暴击结果。结果存 **`ctx->crit_happened[owner]`**
  （放 **context** 不放 ws——伤害管线分阶段算，暴击改了能力等级后**变威力要按新等级/威力量再算一遍**，
  标志的生命周期要跟"这次攻击"对齐），`stage_simple_attack_damage` **只消费不重掷**；
  一次技能使用**掷一次**（多段/变威力共用）。
  ⚠️ **为什么必须前移**：技能无效时伤害结算**整段不执行**（`handle_*_AttackDamage` 因
  `allowAttackDamagePipeline==false` 早退），在伤害处掷就永远掷不到——而"打在盔上一样触发暴击并破防"是官方要求。
- **暴击破防**（原语 `crit_defense_break`）：物理 → 对手**正防御**等级归 0；特殊 → **正特防**；
  只清**正**等级；**不查任何免疫**（`STAT_CLEAR` 挡不住——它不是"消除强化效果"，是暴击自带规则）。
- **顺序 = 先结算伤害再重置等级** → 变威力/多段"重置后再次结算"的那一段可无视对手的提升。
- **调用点是攻击结算的收尾步骤，不是时点桶效果**：`handle_Battle{First,Second}AttackDamage` 的**两条出口**都调
  （正常出口在 `apply_resolved_damage` 之后；被盔早退出口在清快照之后 → "打盔也破防"）。
  ⚠️ **为什么不放时点桶**：ATTACK_DAMAGE 的桶在技能无效时**整段不执行**；挪到 AFTER_ACTION 又落在
  "下一次结算"之后（变威力在同一次 ATTACK_DAMAGE 内重复结算就吃到未重置的等级）。它是**引擎内建规则**（同克制/暴击倍率）。
- 权威口径与已知边界（多段结算未实现、额外行动复用结果）见 `docs_local/docs/02-效果系统/技能判定流程与无效效果体系.md` §八。

### 3.8c 必中凭证 / 失明 / ③层命中失效分两类（2026-09-13，场景 041）

- **必中是每回合凭证**，判"是否必中"一律读 **`ws.attack_credential[owner].must_hit`**，
  **不要只读 `skill.must_hit`**——存在**条件必中的固有效果**。凭证三合一（`materialize_attack_credential`）：
  `skill.must_hit` ∨ `ws.must_hit_grant[owner]`（本回合效果授予）∨ `force_execute`。
  条件必中（如 2000「对手处于能力提升则先制+1**且必中**」）的效果体在 **MOVE_RIGHT 时点**同时写
  `ws.preemptive_level` 与 `ws.must_hit_grant`——**`on_selected` 会被 ROUND_START 的 ws reset 冲掉**，
  **直接改 `skill.must_hit` 是永久的**（切宠/跨回合都不复原）。⚠️ 插件里再写这类效果时按此办。
- **失明**（异常 20）：非必中技能**必定 miss**；必中技能 **50% 正常命中 / 50% 命中效果失效**；
  **`force_execute` 最先短路**（完全绕过失明）。落点 `query_usage` ①，失明的 50% 档照常走 ② 门判定。
- **③层命中失效按技能类型拆两类**：`RuleCategory::HIT_INVALID_ATTACK` / `HIT_INVALID_ATTRIBUTE`
  （**不是**一条加通配；要两种技能都失效就**注册两条**）。`grant_hit_invalid(..., is_attribute_skill)`、
  `consume_hit_invalid(defender, is_attribute_skill)`。**票据挂防御方**（`target` = 持标记方），
  攻击方出手时从 `1 - owner` 侧消费。**消费点收口在 `query_usage` ②.5**，模式记 `ws.hit_invalid_mode`。
  ⚠️ 与 ② 门判定的 `SEAL_ATTRIBUTE_HIT`（封属·命中失效）**不是一回事**：那个**天生只响应属性技能**。

### 3.8d 变威力（2026-09-14，场景 043）

**官方对"威力"的定义本身就是这条规则**：「当命中效果对技能威力值进行修改时，**会以当前状态
重新进行伤害结算覆盖原本的伤害值**」——即改威力 ≠ 赋值，而是**推翻第一次伤害计算、重算第二次、
第二次覆盖第一次**。
- **触发条件是"威力被*改过*"，不是"变了"**：「提升0点威力也属于变威力」。所以判据是
  `ws.skill_power_view[owner].rewritten`，由 **`SkillView::operator=` / `operator+=` 自动置位**
  ——效果侧**零纪律成本**（直接写 `ctx->ws.skill_power_view[owner] = x` 就行，写 0 也算）。
  **别做值比较**。引擎自己的物化（`materialize()`）与**强制执行置 0** 不算，由
  `resolve_skill_execution` 在 `execute()` 之后 `consume()` 清掉。
- **落点**：`apply_variable_power_recalc`（battleFsm.cpp），在
  `execute_registered_actions(ATTACK_DAMAGE)` **之后**、`damage_pipeline_.run()` **之前**再跑一次
  `stage_simple_attack_damage`（它是"覆盖式"的：开头清两个快照 + 末尾整体赋值，**不会叠加**）。
- ⚠️ **伤害管线仍然只跑一遍**（重算在管线之前）——这是该落点的全部意义：绕开"跑两遍就错"的效果
  （BLOCK 阶段 `consume_immune` 会**再扣一次次数型免伤**、effect_525 概率挡伤**重掷**、
  effect_1236 **再打一次粉伤**、`resolvedDamage.addPct` **累加**）。**千万别改成"重跑管线"**。
- **重算什么**：能力等级（双攻/双防）、克制系数、威力——都是公式当场读的字段，重跑即天然覆盖。
  暴击**不**重掷（`ctx->crit_happened` 一次技能只掷一次，重算照它再乘一次倍率）；伤害浮动 `rand()` **重掷**。
- **唯一肉眼可见后果**：**暴击破防只对变威力技能生效**——破防在"第一次公式之后"（§3.8b），
  非变威力技能本次伤害已算完，享受不到；变威力第二次结算读到归零后的双防。这就是官方
  "变威力暴击无视双防"的全部成因。
- **变威力"结算靠后" → 隔着盔也能打出攻击伤害**：强制执行把视图置 0 后，技能自己的变威力效果
  改回来 → 红伤照常（场景 043 V4 / 042 N8）。
- **技能无效出口也接上了检查点**（★ 2026-09-14，场景 046）：技能被盔/威/封属拦下时
  `handle_*_AttackDamage` **整段早退**（`allowAttackDamagePipeline=false`），但
  **SKILL_INVALID 分支的效果照常注册**（`registerSkillEffects` 仍为 true）并在
  `BATTLE_*_SKILL_EFFECT` 时点执行。它改写的视图威力会被早退出口的
  `run_attack_damage_from_invalid_skill` 抓到 → 隔着盔重算并打出红伤。
  **effect 2501「技能无效时，重新进行伤害结算且自身每有260点特攻值威力翻倍1次」即此**
  （薇尔诗·乐园之初诞 38421；官方"技能效果还是无效，只有攻击伤害"）。
  ⚠️ 这条路**只对"带变威力词条"的技能生效**——没有标记时无效就是零伤害（不会白给伤害）。
  见 `docs_local/docs/05-任务清单/待做-变威力与增减伤时点.md`。

### 3.8e 连击（"1回合做 x~y 次攻击"，2026-09-14，场景 044）

**是"一次伤害公式 ×N"，不是 N 次独立结算**——官方公式里 `×连击次数` 排在**最后**
（`…×浮动×暴击抗性×暴击系数×连击次数`）。
- **静态基数**：`Skills::combo_min / combo_max`，`loadSkills` 从技能自己的 `side_effect` 参数里取
  （`skills.cpp` 的 `combo_arg_indices` 开关表：多数是 `{0}-{1}`，**1172/1577/1627** 是
  `{0}回合做{1}-{2}次` → 下标 +1）。**不碰 `effect_info.info` 文本**。
- **每次技能使用掷一次**：`Skills::roll_combo_count()` → `ws.combo_view[owner]`（区间退化 1~1 时
  短路**不消耗 `rand()`**）。第一次/第二次结算、多段共用同一个 N。
- **落点**：`stage_simple_attack_damage` 里**暴击乘完之后**、快照落地之前 `base *= N`。
  乘在 `base` 上是刻意的：下游 `deal_damage`／伤害管线／日志**只消费 `final`** →
  **增伤/减伤/护盾/次数型免伤自动作用于总数**（挂"免疫下1次攻击伤害"时 N=3 的攻击**整段归零**，
  不是只挡第一段）。`DamageSnapshot::hitCount` 只作日志/调试用。
- **连击本身就是变威力**（官方把"n次连击"与"威力提升n%/n点"**并列**为变威力描述）→
  `apply_variable_power_recalc` 的判据含 `combo > 1` → 走第二次结算 →
  **连击技能吃得到暴击破防**（场景 044 C4：N=3 的伤害 ≈ N=1 的 6 倍 = 3×连击 × 2×破防；
  不做第二次结算会停在 3 倍）。
- ⚠️ **未做**：动态加数（`1108`「每处于一种能力提升则连击数+1」/`484`/`1795`/`1863`/`1930`）、
  连击上限修正（`1500`/`1546`/`1577`/`1593`/`1627`/`1666`/`1685`）、逐段子效果
  （"每次攻击有 X% 概率…"）、无参数常量 `1608/1609/1610`。
  `ws.combo_view` 是 `SkillView`（写它自动打变威力标记），补效果时直接 `+=` 即可。
- ⚠️ **`moves.atk_num` 不是连击段数**（用户 2026-09-14 判读：极电千鸟 `atk_num=5` 疑为
  "命中后 5% 令对方麻痹"这类**老旧异常施加字段**）→ **不要**拿它当 N。

### 3.8f 增减伤时点链（2026-09-14，场景 045）

`DamagePhase` 重排为 **10 阶段**，**`kOrder` 是一个可改常量——顺序即机制**，改一行就是改机制，
不用动任何回调：

```
GUARD_DETECT  犀牛魂印：受高伤检测/挡伤/回满 —— 看到的是**未增伤**的值
AMP           通用增伤（加法）     —— damage_add_pct 求和后一次性施加
AMP_EXTRA     非通用增伤（乘法）   —— damage_add_extra_mul 逐槽连乘
REDUCE_FLAT   点数减伤            —— damage_reduce_flat 求和后从伤害里扣
REDUCE_PCT    百分比减伤          —— damage_reduce_add 求和(钳±100) + damage_reduce_mul 连乘
FLOOR         保底伤害            —— 减伤之后、锁伤之前
CAP           伤害锁定            —— 最后再拦一次；**697 凭证可穿**
BLOCK         伤害免疫/挡伤归零   —— 可被"挡伤失效"掩码抑制
DETECT        受击检测/触发       —— 反伤、附伤
FINAL_CORRECT 最终修正            —— 不可击穿
```

- **依据**：时点链「犀牛魂印—通用增伤—693增伤—保底伤害—护盾」+ 减伤区固定顺序「点数减伤——
  百分比减伤——伤害锁定——伤害免疫（不再相互穿插）」+ 用户 2026-09-14 逐条确认。
- **`GUARD_DETECT` 在链首**是"693 增伤乱穿犀牛""保底不触发超350回满"两条现象的**结构性成因**
  ——犀牛检测之后才被抬起来的伤害它看不到（场景 045 P7 用探针锁死）。
- **增伤两通道的判据是文本里的"额外"**：「造成攻击伤害提升X%」→ 加法（AMP，776）；
  「造成攻击伤害**额外**提升X%」→ 乘法（AMP_EXTRA，693）。
- ⚠️ 两条**已被踩过的观测坑**（写这类探针场景时必看）：① 探针挂在**防守方**的桶（owner 1）
  才能看到减伤后的值——管线在一个阶段里按 {攻击方, 防守方} 走两个桶，挂攻击方会**整体前移一格**；
  ② 后手方（属性技能）也会跑一遍管线 → 探针必须滤 `attackerId/defenderId`，否则记录翻倍。
- ⚠️ 不能在 `strike()` 之前直接写 `ws` 字段：回合开始 `ws.reset()` 是 memset 会冲掉。
  要走真实路径在 **SKILL_EFFECT** 时点写（693/776 的生产写法即是）。
- **三种伤害抗性（暴击/固定/百分比）形状统一：都是乘"整段"伤害。**
  · 暴击抗性：先 `×暴击系数`（整段放大）**再** `×暴击抗性`（也是乘整段）→ 100 伤暴击后 200、
    35% 抗性 → **130**（**不是**只削加成部分的 165）；**100% 抗性 → 0**。
  · 固定/百分比抗性（`deal_damage` 粉伤层）：`effective -= effective * resist_pct / 100` 同一形状。
  ⚠️ 别把它当"只削暴击加成那部分"——那是错的（2026-09-14 用户纠正；旧注释与场景 014 N7 都已改）。
- **减伤钳在 100%**（`Calculation::applyDamageReduction`）：加算槽求和钳 ±100、乘算槽逐槽钳 ±100、
  结果再夹到 ≥0 → **超出 100% 的部分不算，绝不溢出或打出负数伤害**（场景 045 P9）。
- **链首受击快照 `ws.guard_detect_damage[defender]`**（core 的 `install_default_damage_guard_detect`
  在 GUARD_DETECT 阶段写）：**含暴击与暴击抗性、未含任何增伤/减伤**。供「受高伤/受低伤」一族魂印读
  （不灭地威·萨瑞卡 1217）。**必须由管线写**——攻击伤害时点的桶只跑当回合 mover 一侧，防守方魂印
  在自己不是 mover 时根本不执行。配套 `guard_detect_happened[defender]` 区分"没被攻击"与"0 伤"。
  回血/反弹那类**落地之后**的动作要挂 `EVENT_TAKE_DAMAGE`（伤害已落地），别挂 `_AFTER_ACTION`。
- ⚠️ **未做**：`FLOOR`/`CAP` 零注册（保底/锁伤的具体效果待做）——实现**锁伤时必须查
  `ws.attack_credential[attacker].ignore_damage_limit`（697）**，否则它一直是死凭证；
  护盾/护罩是否挪进管线**待实测**；粉伤通道**完全不过管线**。

### 3.8g 选择期"微弱→奖励"一族（2026-09-14，场景 047）

`2490`「**此技能**微弱对手时技能先制+3、获得本系属性加成并**以神灵系**进行伤害结算」
（薇尔诗·乐园之初诞 38421，args_num=0 → 数字与系别都写死在文案里）。

- **走选择期集合**：core 在 `loadSkills` 里把它放进 `Skills::selection_effects_`，`on_selected`
  统一注册到 `BATTLE_FIRST_MOVE_RIGHT` —— 先制必须在**先手权结算之前**定（同 610 天敌 / 2000 条件先制）。
- ★ **判"是否微弱"读的是技能槽位的静态系别，不是 `ws.skill_element_view`**：视图会被
  "以XX系别进行伤害结算"这类效果改写，读视图等于把改写后的系别喂回判定（用户口径）。
  落地：core 把技能静态系别**追加进 effect args**（`args[2]/args[3]`）喂给插件
  —— 插件拿不到 core 内部的 `resolve_executing_skill`，这样比开 CoreApi 新槽干净。
- ★ **"以XX系别结算"要写授予**（`ws.skill_element_grant` + `_valid`），不能直接写
  `skill_element_view` —— 后者会被 `resolve_skill_execution` 在 **ON_SKILL_HIT 的物化覆盖**
  （MOVE_RIGHT 早于 ON_SKILL_HIT）。物化已接这个授予。**同 `must_hit_grant` 的教训。**
- ★ **"获得本系属性加成"走 `ws.force_involve`**：`calculateDamage` 里 `involve()` 读的是
  **技能真实系别 `skill.element`**（刻意设计：改系别只改克制、不改本系）→ 改元素视图**拿不到**
  本系加成，必须另开显式开关。
全族三条（785/2030/2490）都已落地，**判据系别不同**（用户 2026-09-14 定）：
| 模板 | 原文 | 判据 | 加成 |
|---|---|---|---|
| 785 | 若**自身**攻击对手时克制关系为微弱 | **精灵**系别 —— 读 `ws.view_elementalAttributes[self]`（**视图**：属性反转类改系别应计入） | +2 |
| 2030 | 若**本次技能**微弱对手 | **技能**槽位静态系别（core 追加进 args[2]/args[3]） | +1 |
| 2490 | **此技能**微弱对手时… | **技能**槽位静态系别 | +3 + 本系加成 + 以神灵系结算 |

- 判"此技能"读**静态值**是为了防**自污染**——本招自己的"以XX系别进行伤害结算"会改写视图；
  判"自身"读**视图**是因为精灵改系别（属性反转类）**应该**计入。场景 048 把两条判据故意做成
  相反结论，读错哪一条都会露馅。
- ⚠️ **同系相打不一定是 0.5**：`grass|grass=0.5`、`steel|steel=0.5`，但 `dark|dark=**2.0**`、
  `normal|normal=1.0`。挑"不微弱"的对照系别时必须查 `types_relation`。

### 3.8h 条件盔（2026-09-14，场景 050）

`RuleTicket::condition` = **条件盔**的落点：`RuleCenter::notify` 对**条件不满足的条目跳过、不消耗**。

- 签名 **`std::function<bool(BattleContext*, int user, int power, bool is_attribute_skill)>`** ——
  由 `notify` 逐条求值，`power` 传的是 **`Skills::power`（面板威力）**，不是 `ws.skill_power_view`。
- 代表效果：`854`「令对手下1次使用的威力高于{0}的攻击技能无效」（天叶领域 24560 / 余温之辰 26170，
  参数 `[120]`）。条件 = `!is_attribute && power > 阈值`，`counts=1`、`penetrable=false`、`scope=ON_STAGE`。
- ★ **"不消耗"是白送的**：威力不够的技能/属性技能/对手本回合没出手，都不会吃掉这次数
  —— 不需要"选完技能后注册当回合盔 + 事件撤销监听器"那套（我们 2026-09-14 的疑问到此结清）。
- `penetrable=false` 归"龙威"一侧（699 穿盔凭证穿不过去）；概率封属（`chance_pct<100`）也走同一个
  `condition` 字段（每次响应时掷，不是授予时掷一次）。
- ⚠️ **条件盔照样被 miss 消耗**（用户 2026-09-15 拍板）：miss 也走 `notify`，且**不**因为
  "没打中"就跳过条件——与"一旦命中失败（miss 类）会消耗所有可响应的次数类效果"一致。
  即 854 在对手**打空**的大威力攻击上也会被响应并扣掉次数。

### 3.8i 粉伤通道（PinkDamagePipeline，2026-09-15，场景 051）

粉伤（固定/百分比）**不走红伤管线**：没有攻击方/技能/克制/暴击那一套输入，也不吃红伤的点数减伤/
百分比减伤/锁伤。共有的只有"值在有序阶段表里流经节点"这个形状 → **另起一条更短的独立管线**。

- `include/effects/pink_damage_pipeline.h`：
  `PinkDamagePhase{RESIST, IMMUNE, REDUCE_EXTRA, AMP, PINK_TO_HOOD, PINK_TO_TRUE_IMMUNE,
  CAP, HOOD, PINK_TO_TRUE_UNHARMED, FINAL}`（抗性免减 → 免疫粉伤 → 百分比免减 →
  **粉伤提升 n%** → 粉转护罩检测点 → **粉转真·免疫粉伤检测点** → 上限/锁伤 → **护罩** →
  **粉转真·未受到粉伤检测点** → 最终粉伤）+ `kOrder`；
  值在 `ctx->resolvedPink`（`PinkDamageResolved{target,actor,tier,raw,final,absorbed,to_true,to_hood}`）
  里流经各节点。**两个粉转真是分支不是先后**：免疫型在护罩**前**（故转真不消耗护罩）、
  未受到型在护罩**后**。
- **顺序依据 =《赛学必修8—免疫粉伤》的流程图**（配图，2026-09-15 核对）+ 官方乘算链
  `固定/百分比伤害 ×(1−抗性免减)×(1−技能特效免减)×(1−魂印特效免减) − 护罩值`
  —— 抗性在前、其他免减**乘法**连乘、**护罩最后**（`HOOD` = 默认回调里 `hood_bank_.absorb`）。
  ★ **增粉排在所有免减之后**（曾放错在链首；顺序可观测，场景 051 P8 用加法探针锁住）。
  抗性取整是官方算法 `伤害量 − 伤害量×抗性`（乘法向下取整）→ 写成 `final -= final*resist/100`。
- **两个粉转真检测点**：*免疫粉伤型* = `PINK_TO_TRUE_IMMUNE` 阶段置 `to_true`，
  `run_pink_damage` 在管线跑完后改以真伤结算（`raw` 全额）；*未受到粉伤型* =
  `PINK_TO_TRUE_UNHARMED` 阶段（护罩之后），**引擎不做事**——它是给免粉补偿一族挂的检测点
  （类1 只看 `final`、类2 连 `absorbed` 一起看、类3 比 `final` 阈值）。要按"实际掉血"判的
  挂 `EVENT_TAKE_PINK_DAMAGE`（只在真扣到本体时发）。
- ⚠️ **星皇之怒无视区间**横跨 免疫/免减/增粉/护罩、**不含抗性**。现有 `ws.ignore_shield` 只让
  `HOOD` 跳过，范围偏窄——完整表达要一条**粉伤侧凭证**，待做（别拿 `ignore_shield` 硬撑）。
- ★ **多段粉 = 一次 `deal_pink_damage` 调用 = 一段**：逐段独立结算（各自取整 / 各自扣护罩 / 各自检测）。
  官方算例 `300×0.65` 是**逐段取整后相加**，不是 `(300×4)×0.65`。引擎端**没有段计数状态**，
  "调 N 次"就是"N 段"。`PinkDamageTier` 只分 FIXED/PERCENT 两档（`PERCENT` 入口已换算成数值，
  与 `PERCENT_VALUE` 在抗性/护罩侧同类）——刻意不存 `DamageKind`，否则头文件会与
  `primitives/battle_primitives.h` 成环。
- ★ **免粉与「挡伤」走同一条路**：`ImmunityType::PINK_DAMAGE` 的 RuleCenter 票据
  （同一个 `grant_immunity`/`is_immune`/`consume_immune`），不再有 `pink_immune` 裸 bool 字段。
  ★ **次数型免粉只挡一段**：多段粉 = N 次独立结算 → 只消费掉 1 次，其余各段照常落到本体
  （counts=0 的窗口/永久型则每段都免）。场景 051 P10 锁住。
- ★ **免疫 / 抗性 100% / 效果减粉 不区分**（用户口径）：对"免粉补偿"一类检测三者等价——
  检测 = 即时算一段粉、当场应用、再看**体力有没有降**。
- ★ **`EVENT_TAKE_PINK_DAMAGE`**（新增事件）：只在**经过护罩抵消后仍有剩余结算到本体**时才发，
  `amount` = 实际扣血量。要区分"检测体力**和护罩**"那一类（L338 类2）在 `DETECT` 阶段读
  `resolvedPink.absorbed`。事件**每段各发一次** → 阈值类检测天然逐段判定。
- **粉转真**（`pink_to_true`）：被免减削到 0 才转，**护罩挡下不算**（`absorbed > 0` 即排除）
  —— 落点就是"护罩抵消粉伤后以为没受到伤害又追加一段真伤"那条口径。
- **已落地子句**：怒涛·沧岚 **2343**「此护盾存在时自身受到的固定伤害、百分比伤害不超过此护盾的数值」
  = `CAP` 阶段。登场盾用 `ShieldBank::kSoulShieldPriority`（**最高优先级** = 与其他护盾叠加时
  **优先被消耗**）+ `source_id = 魂印 id` 登记；条款用 **`quantity_of(source_id)` 只认自己那一笔**
  （读 `total()` 会让别的来源抬高上限），且回调**实时**读、不缓存（护盾会刷新、蚀砚之泪≥3滴会让护盾失效）。
  ★ 不需要给 `ShieldBank` 加新结构：`{priority, quantity, source_id}` 三件套已经够了
  ——"优先消耗"= priority、"只对本盾生效"= source_id。约定：**魂印专属盾至多一个**（用同一个
  source_id + `refresh_source`，不要并排叠好几笔，否则"不超过**此**护盾"指向哪笔会含糊）；
  箫澈 **2099** 悲龙之鸣「每次自身受到≥300的固定伤害、百分比伤害」= 首个事件消费者。
  ⚠️ 两者的其它子句（永恒之水 / 蚀砚之泪阈值 / 哀霆之声红伤支 / 战斗阶段结束）**未做**；
  哀霆之声需要"下1次固定/百分比伤害免疫"的**次数型粉免**，现有 `pink_immune` 只是 bool。
- **已接入的三条真实技能**（场景 052，覆盖三种判据）：
  · 库坦「残戮砍杀」36575 / **1737** —— 已损失PP总和×{0} 固伤，**免疫型**转真（等量）
  · 启灵元神「神之束缚」27020 / **1531** —— {0}回合内每回合附加{1}固伤，**免疫型**转真；
    逐回合触发靠注册到 `BATTLE_{FIRST,SECOND}_SKILL_EFFECT` 两格（同回合只有一格以 owner 为 mover）
  · 圣谕邪魇「神明送葬」28442 / **2077** —— {0}回合吸取{1}，**高额减粉型**：实际减少值 < **180**
    （模板字面值）则补 {3} 点真伤
  · 新增 `CoreApi::deal_true_damage` + 原语 `deal_true_damage`（插件补真伤的唯一入口）
- ⚠️ `AMP`/`CAP`/`DETECT` 目前零注册（增粉一族/锁粉一族/免粉补偿一族待做）；
  粉伤管线**不消费 `damage_suppress_mask`**——官方"挡伤失效"枚举里没有护罩，粉伤侧抑制口径空白，
  宁留空也不照搬红伤语义（将来有口径就在 `run` 里加两行，同红伤 `DamagePipeline::run`）。

### 3.8j 批 4：圣灵谱尼 5000 经典基础技能（2026-09-15，场景 053）

12 条落地 + 35 空占位 + 3 条粉转真（场景 052）。三处新增引擎位：

- **`ws.must_crit[2]`**（effect 58 圣光气「下N回合自身攻击技能**必定**致命一击」）：
  ⚠️ **不能复用 `crit_rate_mod`** —— 那是**乘算**修正，技能 `crit_rate == 0` 时
  `0 × 任何数 = 0`，表达不了"必定"（`crit_rate==0` 就是"永不必暴"）。消费点在 `skills.cpp` 的暴击掷处。
- **`ws.attribute_must_miss[2]`**（effect 86 圣洁「N回合内属性攻击对自身必定miss」）：
  [被保护方]，攻击方出手时查 `1 - owner`。⚠️ 是"必定 **miss**"（技能打空）**不是**"失效"——
  走 miss 路径（照常 notify 消费对方次数类盔，§2.3）；必中技能绕过它（与失明一致）。
- **effect 52 虚无的判据位置**：`handle_BattleFirstMoveRight` 是「先清 pr → 跑桶 → 再算 pr」，
  MOVE_RIGHT 的桶里读 `ws.preemptive_right` **恒为 NONE**。改用
  `BATTLE_FIRST_BEFORE_SKILL_HIT` —— 该时点的桶**只跑先手方**，"被执行"本身就等于"我先手"。

**取整口径**：28 圣灵魔闪光「降低对方 1/{0} 的 hp」用 `PERCENT_VALUE` 传 `max_hp / denom`
得**精确的 1/N**；用 `PERCENT`（整数百分比）1/8 只能表达成 12%，会少算。

**弹伤三族**（都是"把伤害打回去"，但走三条不同的路）：
| 型 | 例 | 落点 |
|---|---|---|
| 真伤反弹 | 816 弦月锋华 / 2360 王·龙予狂宴 | `DamagePhase::BLOCK` + `deal_true_damage`（`arm_immunity_reflect_true` 共享） |
| 直写 hp 弹伤 | 34 能量 / 鲁斯王克制 | **时点桶**里读 `ws.resolvedDamage`（本回合红伤结果）× 倍率，直写对手 hp、**不管减伤**。⚠️ 必须**自检 `damage_suppress_mask`** |
| 老代码等同真伤 | 7 同生共死 | `deal_true_damage` |

### 3.9 插件约束（重要）

**moves_lib / soul_lib 不链接 sim_core**——插件只能：
- ✅ 调用**内联方法**（如 `grant_immunity`）和访问**公开字段**（如 `ctx->ws`、`ctx->roundChoice`、`ctx->seerRobot`）。需要插件调用的能力要提供 inline 入口（如 `register_skill_effect` / `invalidate_all_round_effects` / `find_pet_with_soulmark` / `grant_skill_invalid` 挂盔威封属 / `grant_penetration`）。
- ✅ **走真原语：走 `CoreApi`（依赖倒置，`include/plugin/core_api.h`）**——core 初始化时把 `break_round_effects`/`apply_anomaly`/`heal_amount`/`clear_stat_boosts` 等**原语函数指针**经 `PluginInitApi{registry, &core_api()}` 传给插件，插件存 `g_core` 后 `g_core->break_round_effects(...)` 直调真原语（查免断/免疫/发事件, 单一真相源在 sim_core）。**"插件要调原语就加 CoreApi 槽"，别手写等价/别直改底层字段。**（起因：原语非内联、插件不可直调；原语被引擎+插件两侧都用, 故不搬 dylib 也不内联进头）
- ❌ 调用**非内联方法**（如 `clear_on_stage_abnormal_statuses`、`seal_skill` 原语）——会链接失败；对策：能用 `CoreApi` 的走 `g_core->…`，否则提供/使用 inline 入口，或直写底层字段（如 `ctx->abnormal_status_end_round[owner].fill(0)`）。
- 效果函数签名 `EffectFn`，owner 从 `args.int_args[0]` 读（引擎绑定）。
- ★ **效果表走静态自注册（2026-09-15 起）**，插件侧**不再手写中央注册清单**：
  ```cpp
  SKILL_EFFECT(816, effect_816_immunity_reflect_true);   // 效果函数之后写一行
  SOUL_MARK(1004, soulmark_canglan_apply_eternal_tears);
  SOUL_PROGRAM(1217, SoulMarkNodeRef(State::..., &fn, false, false));
  ```
  宏展开成静态初始化对象（dlopen 时塞进本 dylib 的表），`plugin_register` 里
  `plugin_reg::flush(registry)` 一次性推给 registry。详见 `include/plugin/plugin_interface.h` 的长注释。
  ⚠️ 宏参数是**函数名**，**不要写 `&`**（`##` 粘不了 `&`）。`SOUL_PROGRAM` 支持多节点（`__VA_ARGS__`）。
  ⚠️ 表是**每 dylib 一份**（inline 函数的函数内静态各 .so 一份；插件间不互相链接）。
  ⚠️ **init 符号去不掉**：插件需要 `CoreApi*`，递数据只能靠一次调用 —— 这里只是把
  "逐个 register" 换成"推一张表"。
- ★ **未实现 effect id 的启动审计**：`EffectFactory::getEffect` 在 `fn == nullptr` 时打一条到
  stderr（同 id 只报一次）。原本是**静默**跳过（调用方 `if (!effect.logic) continue;`），
  症状是"技能看着生效了、其实什么都没做"。⚠️ **有假阳性**：连击模板（1172/1454/1593/1627/1685
  等）不需要 EffectFn（由 `combo_arg_indices` 消费）。

### 3.10 效果私有槽（pet 持久槽 ×2 —— 按生命周期二选一）

**两个槽是一对，选哪个只看生命周期**（引擎不关心布局，内容由效果自解释）：

| 槽 | 生命周期 | 用法示例 |
|---|---|---|
| `ElfPet::soulmark_storage` | **下场保留**（跨回合 + 跨切换，随 pet 存活；`clearAllEffects` 清） | 无相谛 2260 蓄力（**故意**要跨场累计）；星皇/薇尔诗等常驻状态 |
| `ElfPet::on_stage_storage` | **本次上场**（`perform_switch` 换宠即清 + `clearAllEffects` 清） | "每次使用递增"类计数（落芳天华 497 固伤递增——官方实测**下场不保留**，用户 2026-09-13 确认） |

- **用法**（插件可访问，std::any + elf-pet.h 头内联）：`auto& slot = ctx->getPet(owner).on_stage_storage[id]; if (!slot.has_value()) slot = MyState{}; auto& s = std::any_cast<MyState&>(slot);`
- ⚠️ **选错槽的后果是静默的**（该清的没清 / 该留的丢了）。判断方法：问"下场的瞬间它还该在吗"。
- **已落地验证**：场景 H（soulmark_storage 蓄力跨 3 回合）、场景 037（两槽分工 + 497 递增/封顶/下场重置）。
- **无相谛+万相乖离完整数据驱动**：需扩展 effect_unit_parser 覆盖 无相谛 6 模板中的 5 类（威力提升/PP降低/消除回合类/吸取/秒杀；仅 1248 异常附加已被第一刀覆盖）。"取消触发条件" = 去掉 EffectUnit 的条件分支。
- 印记写在 pet 对象（marks）；重新登场"回接"到战斗的动作待换宠实现后接线。

---

## 4. 测试

- **场景化对战测试**（test/scenario/，推荐新增测试用这个）：
  - `scenario_harness.h` 手动构造精灵/技能/魂印（减少 DB 依赖），走真实 FSM。
  - `make_pet` 元素类型用 **0**（类型 1 相互克制=0 会打出 0 伤害）。
  - `ScenarioBattle`: `start()` 驱动到 CHOOSE → `submit_operation()` → `drive()` 跑完回合；`step_until/step_past` 单步看时点。
  - 场景 001-023 覆盖：条件先制/技能无效补偿/吸取减伤/穿透/强制执行/魂印事件回调和多时点/2260 完整子句/组合语法与条件取消/换宠与盔威封属/克制与系别视图/粉伤抗性/异常抗性/护盾护罩/封回血/魂印作用域/技能替换/场域/自爆传承（逐条见 docs/07/场景测试说明.md）。
- **smoke**（test/test.cpp，gitignore 未提交）：段式断言，覆盖穿透/失效/组合语法等。
- 插件（moves_lib/soul_lib）改动后**确保重建**——增量构建可能漏掉插件。

---

## 5. 关键坑（改动前必读）

1. **插件不能调非 inline 方法**（见 3.9）。
2. **State 枚举在 state.h**，不在 battleContext.h；想引用 State 的文件若只 include 了轻量头，需显式 `#include <fsm/state.h>`。
3. **同源去重**：同 `source_id`+`effect_id` 会覆盖旧效果。**魂印节点现按 `id*100+节点序号` 取 effect_id**
   （`soul_mark_manager.cpp`）→ 同一魂印可在**同一 State 放多个节点**（谱尼 1038 在 ENTER_EXIT_STAGE 挂 3 个、
   ROUND_END 挂 3 个，按声明序执行）；旧「节点必须放不同 State」的规避已不需要。
   另：同一 effect id **后注册覆盖先注册**——插件里重复注册同一 id 会让先注册的变成死代码（见 §5.10）。
4. **测试从项目根跑**（相对 DB 路径）。
5. **换宠已实现**（`perform_switch`，battleFsm.cpp）：CHOOSE_PET 在 ACTION_START 触发——`invalidate_on_stage_effects`（清公共状态+拦截桶）+ `clear_on_stage_abnormal_statuses` + `on_stage` 更新 + 新宠 `activate_soul_mark` + ws 同步；死亡换宠走 `handle_ChooseAfterDeath`。被拦截方换宠会洗掉自己身上的封属性（拦截挂被拦截方桶）。
6. **数字即"细码"**：很多效果参数是官方模板 id/细码，先查 `docs_local/docs/03-数据与数据库/官方数据关联查找指南.md` 或直接 sqlite3 查 `effect_icon`/`effect_info`。
7. `BattleWorkspace` 每回合 `handle_BattleRoundStart` 开头 reset()——本回合凭证放 ws 自动清，跨回合的走效果桶/context 字段。
8. **能力等级伤害倍率**（`BattleWorkspace::getTempAbilityValue`）：正等级 `(level+2)/2`，负等级 **`2/(2-level)`**。⚠️ 旧写法是 `2/(2+level)`：-1 算成 ×2（方向反了），**-2 直接除零** → int 溢出成 INT_MAX（一击 5 亿，场景 032 踩到、028 加了回归断言）。改这块务必跑 028（它会打一次带 -2 等级的攻击）。
9. **`find_pet_with_soulmark` 只认存活精灵**（hp>0）——"判某事件时源是否在场"用它没问题；但**若该事件本身就是"刚被打到 hp==0"**（锁血类回调），它会把自己判成不在场 → 直接读 `seerRobot[owner].elfPets[on_stage[owner]].soulMark.id` 比较（场景 032 踩过）。
10. **同一 effect id 后注册覆盖先注册**（`effect_cache_[id] = fn`）——moves_lib 里曾同时有 `effect_skill_5` 与末尾的 `effect_5_change_opp_stat`，前者是**死代码**（被覆盖），调试时"改了却没反应"极易误判成"插件没生效"。改插件前先 `grep registerSkillEffect` 查同 id 是否重复。另：**只 build 场景 target 不会重建插件 dylib**（`--target sim_scenario_0NN` 的依赖链里没有 `moves_plugin`），改插件后必须整包 `cmake --build build -j8`。
11. **`ElfPet::hp` 是 `numericalProperties[HP]` 的引用别名**（`int& hp = numericalProperties[HP];`）——所以 `numericalProperties[HP]` 是**当前**体力，**不是最大体力**。最大体力在 **`numericalBase[HP]`**（引擎侧 `heal_impl` / `deal_damage` 的 PERCENT 换算都读它）。插件写"最大体力的 1/N"类效果时读 `numericalProperties[HP]` 会拿到当前血 → 回血量/百分比伤害随掉血而缩水（场景 039 的 Z5 踩过：满血恢复只回到了 2400/3000）。moves_lib 里的 `max_hp_of()` helper 就是为此存在。
12. ★ **`EventCenter` 回调内"自删"必须靠 `deliver` 先拷贝 `fn`**（2026-09-14 修，症状极具误导性）：
    `deliver` 里原本直接 `watcher.fn(ctx, event)`，而回调第一件事常是 `remove_watcher(自身 id)`
    —— 那会 `erase` 掉 `watchers_` 里的条目，**把正在执行的 `std::function`（连同它的闭包存储）
    当场析构**；回调之后再读任何**捕获变量**就是在读已释放内存。表现是**捕获值与注册时不一致**
    （effect 2006 里 `self`/`opp` 读成垃圾 → 子句作用到错误的精灵），且**同一个二进制时好时坏**。
    修法：`deliver` 先把 `fn` **拷贝一份**再调，闭包就有了独立存储。
    ⚠️ 这条对**所有"回调内自删"的 watcher 都成立**（自爆传承/2006 子句等），别再靠运气。
13. ★ **插件是用 `POST_BUILD` 拷到 `resources/<lib>/` 的，改成常驻部署 target**（2026-09-14 修）：
    运行时加载的是 `resources/moves_lib/liblib_1.dylib`（`EffectFactory::getInstance("resources/moves_lib")`），
    而构建产物在 `build/plugins/`。`POST_BUILD` **只在该 target 重新链接时才跑**——`.o` 没重编
    （改了头但 .cpp 没变 / 源码与 .o 的 mtime 落在同一秒）就不链接 → **不拷贝** → 运行时跑的是**旧插件**。
    更坏时插件与 core 头不一致（**字段偏移错位**）→ 写进错误的字段、行为随机。
    已改成 `add_custom_target(... ALL ...)`（无输出 → 每次构建都执行），根治。
14. **`CoreApi` 字段用位置初始化列表填充**（`src/effects/core_api.cpp`）——**新槽一律追加到 `struct CoreApi` 末尾**，插在中间会让后面所有槽错位（连踩两次）。另：**函数指针不吃默认参数**，给原语加了带默认值的形参后，插件侧经 `g_core->xxx` 的调用必须把实参补全（场景：`seal_skill` 加 `chance_pct` 时 plugin 两处调用漏参编译失败）。
15. **`ws.skill_power_view` / `ws.combo_view` 是带标记的类型 `SkillView`，不是裸 int**——`view[owner] = x` / `+= x` 会**自动**打上"这是变威力"的标记 → ATTACK_DAMAGE 收尾重算第二次。所以：① **引擎自己的写入要用 `materialize(x)`**（物化打底、强制执行置 0、连击掷点都不算效果改写），用 `=` 会白触发一次重算；② 读点因为 `operator int()` 无感（`==`/赋值/`static_cast` 都行），但**流输出有个 `operator<<`**；③ **重算在伤害管线之前 → 管线只跑一遍**，不要把它改成"重跑管线"（次数免伤会多扣、概率挡伤会重掷、粉伤会重打，见 §3.8d）；④ `reset()` 是 `memset` → 两个槽的默认值（威力 `-1`、连击 `1`）**必须显式恢复**。
16. **伤害管线的阶段序就是机制**（`include/effects/damage_pipeline.h` 的 `DamagePhase` + `kOrder`）——"693 增伤能否穿过犀牛""保底能否被锁伤压住"这类问题**不要写判断，调顺序**。新增伤害类效果时先确认它属哪个阶段（增伤在 AMP/AMP_EXTRA、减伤在 REDUCE_FLAT/REDUCE_PCT……），别随手挂一个"看起来能用"的阶段。另：`EffectCategory`（AMP/MITIGATE/BLOCK/DETECT）只决定**能否被"挡伤失效"掩码压制**，选错会让效果被蚀砚之泪误压或漏压。

---

## 6. 常用 SQL 速查

```sql
-- 技能 + 效果模板
SELECT * FROM moves WHERE id=37218;                       -- 烬灭神咒剑
-- 魂印（effect_icon 是魂印表！）
SELECT effect_id, kind, tips FROM effect_icon WHERE id=1862;   -- 无为觉者魂印（effect_id 2260）
-- 效果模板
SELECT * FROM effect_info WHERE id=2260;
```

---

## 7. 文档导航（docs/ 已按主题分目录）

- `docs/00-组件导航/` — **★ 快速理解代码库入口**：每个核心目录一篇导航表（文件×职责×关键入口×关联专题）。新同事从这里 + CLAUDE.md 起步
- `docs/01-架构与设计/` — 架构评审、演进、并发、训练控制块
- `docs/02-效果系统/` — 效果内核设计（公开）；机制口径类文档（技能判定流程/无效体系、魂印机制档案、时点判定流程表）在 `docs_local/docs/02-效果系统/`（不入库）
- `docs_local/docs/` — 机制口径、数据管线、任务清单、交接等本地文档（gitignore）：03 数据库三篇、04 效果重构落地计划、05 任务清单、06 交接、08 参考
- `docs/07-工具与测试/` — 测试指南、命令行客户端使用说明

---

## 8. 语料检索管线（tools/corpus/，2026-09-27 起）

机制解析语料（reference/scrape/out/all_posts_final.jsonl，479 篇）的检索库：
`python3 tools/corpus/build_corpus_db.py build`（产出 build/corpus_search.sqlite）。
查询：`... query <词>`；别名入库：`... alias-add <官方名> <昵称> --pet-id N --source <出处>`。

**维护纪律（用户 2026-09-28）**：
- 语料 jsonl 更新（重爬/增量）后**必须重跑 build**——FTS 索引和别名都在库里，不重建就查不到新文章。
- 别名真相源 = `tools/corpus/aliases_confirmed.json`（用户批注 + 扫描 strong，带出处）；
  build 时自动装载。临时试验用 alias-add 会随重建丢——正式别名进 confirmed 文件。
- 解析规则：①同名子串别名一般指序号大者；②id>5000 是皮肤/boss 不理会；③图鉴漏记
  新宠以 unity 库 monsters.**def_name**（小写）为准。候选文件：aliases_candidate.{json,md}。
- idx 约定 = all_posts_final.jsonl 的 0-based 行号（全库文档引用 idx=NN 即此）。

## 9. 本地控制台运维约定（2026-09-28）

**每次更新引擎/插件/前端后，重启本地 web 对战控制台**（用户手机经局域网连接）：

```bash
pkill -f web.gateway.app; pkill -f 'build/sim_server'   # 先清旧进程（防 SO_REUSEPORT 多实例并存）
nohup ./build/sim_server --port 4399 > /tmp/sim_4399.log 2>&1 &
nohup web/.venv/bin/python -m web.gateway.app --host 0.0.0.0 --port 8070 > /tmp/gw_8070.log 2>&1 &
```

- 网关**必须 `--host 0.0.0.0`**（局域网可连，手机用 `http://<本机IP>:8070`；本机 ip 用
  `ipconfig getifaddr en0` 查）、**不加 --auth-token**（本地免验证）。
- 前端源码（web/frontend/src）改动后要 `cd web/frontend && npm run build`（网关服务 dist/）。
- 重启前 `lsof -nP -i :4399 -i :8070` 清点旧进程——SO_REUSEPORT 会放行多实例并存
  （曾同时 3 个 sim_server 挂同一端口，流量随机分流，症状是"状态随机跳"）。

**多 agent 并行纪律**：agent 并发上限 2；每个会话只在自己的 worktree 里构建/跑回归；
**不要多会话同时全量构建+跑全套**（N 路并行编译+回归会把内存/CPU 打满导致卡死）——
错峰跑，或全套回归留给最后一只在主线验收的会话。
