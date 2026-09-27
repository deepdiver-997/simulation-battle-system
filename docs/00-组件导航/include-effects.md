# include/effects — 效果内核

效果怎么表示、怎么注册、到点怎么执行的**运行时内核**。这里多数是头文件内联（插件能调），
被 `entities/` 组装、被 `fsm/` 调度。

| 文件 | 职责（一句话） | 关键入口 / 字段 | 关联文档 |
|------|--------------|----------------|---------|
| `effect.h` | **效果本体**基元 | `Effect`（模板：id+fn+args）、`EffectArgs`（int数组+extra）、`EffectResult`、`EffectFactory`（`IEffectRegistry`） | CLAUDE.md §3.3 |
| `continuousEffect.h` | **连续效果**实例化（注册回合/持续/所有权/作用域） | `ContinuousEffect`：`source_id_`（去重）、`scope_`(ON_STAGE/TEAM)、`valid_id_`（版本/断回合）、`once_`（回合限一次） | CLAUDE.md §3.3 |
| `timed_bucket.h` | **时点桶**：效果注册到"某时点→玩家"的容器，FSM 到点执行它 | `TimedBucket`（`battleContext` 里的 `skills_effects`/`soul_mark_effects` 实例）：注册/去重(key=`source_id<<32|effect_id`)/执行/过期/epoch作废/回合计数 | CLAUDE.md §3.2（**效果执行唯一入口是桶**） |
| `effect_meta.h` | **效果元数据**目录（静态模板解读） | `EffectMetaCatalog`、`EffectMeta`（含 `nullify.hit_effect_invalidatable`）、`set nullify 判定` | CLAUDE.md §3.1 |
| `effect_unit.h` | **组合效果单元**（主动作+条件+概率+分支） | `EffectUnit`、`UnitCondition`（SameElement/FirstMove/…）、`SkillResolutionFlags` | CLAUDE.md §3.3 组合语法 |
| `effect_unit_parser.h` | **组合语法文本解析器**（脆弱层，模型离线编码不再扩运行时解析） | parser 识别“条件→动作”模板（179/700/1083/1257/456） | CLAUDE.md §3.3 ⚠️ 脆弱层 |
| `event_center.h` | **事件中心**：发生了什么→通知 watcher | `EventCenter`、`BattleEvent`、`EventWatcher`（`EVENT_DEATH`/`EVENT_ENTER_STAGE`/`EVENT_BREAK`）、register_break_callback | CLAUDE.md §3.6 事件 |
| `rule_center.h` | **统一规则容器**（免疫 + 盔/威/封属(含命中失效) + ③层命中失效 合并，header-only） | `RuleCenter`、`RuleTicket`(source 挂载→target 生效/覆盖键/source 锚生命周期)、`grant_immune`/`grant_seal`/`grant_hit_invalid`、`is_immune*`/`notify`(三态)/`consume_hit_invalid` | CLAUDE.md §3.6 + 权威口径 + [容器选型评估](../01-架构与设计/rule_center容器选型评估.md) |
| `damage_pipeline.h` | **伤害管线**：按 DamagePhase 顺序跑双方减伤/加伤效果 | `DamagePipeline`、`DamageEffect`、`DamageSnapshot` | CLAUDE.md §3.7 |

**三类容器速记**：①**时点桶**=到点执行自己 ②**事件中心**=发生了什么→通知 ③**查询目录**=我要做什么→被查
（免疫/盔威封属与③层命中失效已并入 `RuleCenter`，`penetration_grants` 都是③）。